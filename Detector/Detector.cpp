// Detector.cpp : 定义控制台应用程序的入口点。
//

#include <stdio.h>
#include <string.h>
#include "zlib/unzip.h"
#include "zlib/zlib.h"
#ifdef WIN32
#include <windows.h>
#endif
#ifdef LINUX
#include <dirent.h>
#endif
#include "Detector.h"


int unz64local_getByte(const zlib_filefunc64_32_def* pzlib_filefunc_def, voidpf filestream, int *pi)
{
	unsigned char c;
	int err = (int)ZREAD64(*pzlib_filefunc_def,filestream,&c,1);
	if (err==1)
	{
		*pi = (int)c;
		return UNZ_OK;
	}
	else
	{
		if (ZERROR64(*pzlib_filefunc_def,filestream))
			return UNZ_ERRNO;
		else
			return UNZ_EOF;
	}
}


int unz64local_getShort (const zlib_filefunc64_32_def* pzlib_filefunc_def,
	voidpf filestream,
	uLong *pX)
{
	uLong x ;
	int i = 0;
	int err;

	err = unz64local_getByte(pzlib_filefunc_def,filestream,&i);
	x = (uLong)i;

	if (err==UNZ_OK)
		err = unz64local_getByte(pzlib_filefunc_def,filestream,&i);
	x |= ((uLong)i)<<8;

	if (err==UNZ_OK)
		*pX = x;
	else
		*pX = 0;
	return err;
}


int unz64local_getLong (const zlib_filefunc64_32_def* pzlib_filefunc_def,
	voidpf filestream,
	uLong *pX)
{
	uLong x ;
	int i = 0;
	int err;

	err = unz64local_getByte(pzlib_filefunc_def,filestream,&i);
	x = (uLong)i;

	if (err==UNZ_OK)
		err = unz64local_getByte(pzlib_filefunc_def,filestream,&i);
	x |= ((uLong)i)<<8;

	if (err==UNZ_OK)
		err = unz64local_getByte(pzlib_filefunc_def,filestream,&i);
	x |= ((uLong)i)<<16;

	if (err==UNZ_OK)
		err = unz64local_getByte(pzlib_filefunc_def,filestream,&i);
	x += ((uLong)i)<<24;

	if (err==UNZ_OK)
		*pX = x;
	else
		*pX = 0;
	return err;
}

int unz64local_CheckCurrentFileCoherencyHeader (unz64_s* s, uInt* piSizeVar,
                                                    ZPOS64_T * poffset_local_extrafield,
                                                    uInt  * psize_local_extrafield)
{
    uLong uMagic,uData,uFlags;
    uLong size_filename;
    uLong size_extra_field;
    int err=UNZ_OK;

    *piSizeVar = 0;
    *poffset_local_extrafield = 0;
    *psize_local_extrafield = 0;

    if (ZSEEK64(s->z_filefunc, s->filestream,s->cur_file_info_internal.offset_curfile +
                                s->byte_before_the_zipfile,ZLIB_FILEFUNC_SEEK_SET)!=0)
        return UNZ_ERRNO;


    if (err==UNZ_OK)
    {
        if (unz64local_getLong(&s->z_filefunc, s->filestream,&uMagic) != UNZ_OK)
            err=UNZ_ERRNO;
        else if (uMagic!=0x04034b50)
            err=UNZ_BADZIPFILE;
    }

    if (unz64local_getShort(&s->z_filefunc, s->filestream,&uData) != UNZ_OK)
        err=UNZ_ERRNO;
/*
    else if ((err==UNZ_OK) && (uData!=s->cur_file_info.wVersion))
        err=UNZ_BADZIPFILE;
*/
    if (unz64local_getShort(&s->z_filefunc, s->filestream,&uFlags) != UNZ_OK)
        err=UNZ_ERRNO;

    if (unz64local_getShort(&s->z_filefunc, s->filestream,&uData) != UNZ_OK)
        err=UNZ_ERRNO;
    else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compression_method))
        err=UNZ_BADZIPFILE;

    if ((err==UNZ_OK) && (s->cur_file_info.compression_method!=0) &&
/* #ifdef HAVE_BZIP2 */
                         (s->cur_file_info.compression_method!=Z_BZIP2ED) &&
/* #endif */
                         (s->cur_file_info.compression_method!=Z_DEFLATED))
        err=UNZ_BADZIPFILE;

    if (unz64local_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* date/time */
        err=UNZ_ERRNO;

    if (unz64local_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* crc */
        err=UNZ_ERRNO;
    else if ((err==UNZ_OK) && (uData!=s->cur_file_info.crc) && ((uFlags & 8)==0))
        err=UNZ_BADZIPFILE;

    if (unz64local_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* size compr */
        err=UNZ_ERRNO;
    else if (uData != 0xFFFFFFFF && (err==UNZ_OK) && (uData!=s->cur_file_info.compressed_size) && ((uFlags & 8)==0))
        err=UNZ_BADZIPFILE;

    if (unz64local_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* size uncompr */
        err=UNZ_ERRNO;
    else if (uData != 0xFFFFFFFF && (err==UNZ_OK) && (uData!=s->cur_file_info.uncompressed_size) && ((uFlags & 8)==0))
        err=UNZ_BADZIPFILE;

    if (unz64local_getShort(&s->z_filefunc, s->filestream,&size_filename) != UNZ_OK)
        err=UNZ_ERRNO;
    else if ((err==UNZ_OK) && (size_filename!=s->cur_file_info.size_filename))
        err=UNZ_BADZIPFILE;

    *piSizeVar += (uInt)size_filename;

    if (unz64local_getShort(&s->z_filefunc, s->filestream,&size_extra_field) != UNZ_OK)
        err=UNZ_ERRNO;
    *poffset_local_extrafield= s->cur_file_info_internal.offset_curfile +
                                    SIZEZIPLOCALHEADER + size_filename;
    *psize_local_extrafield = (uInt)size_extra_field;

    *piSizeVar += (uInt)size_extra_field;

    return err;
}

bool checkZip(char *szPath)
{
	if (szPath == NULL) return false;

	FILE *file = fopen(szPath,"rb");
	if (!file) return false;

	char buff[5] = {0};
	if (0 >= fread(buff,1,4,file)) 
	{
		fclose(file);
		return false;
	}

	fclose(file);
	if (buff[0] == 0x50 && buff[1] == 0x4B && buff[2] == 0x03 && buff[3] == 0x04)
		return true;

	return false;
}


int parseZip(char* szPath)
{
	int nRet = 0;
	unzFile uzf = unzOpen(szPath);
	if (uzf == NULL)	
		return -1;

	unz_global_info gi;
	if (unzGetGlobalInfo(uzf,&gi) != UNZ_OK) 
		return -1;

	char file[256] = {0};
	char ext[256] = {0};
	char com[1024] = {0};
	unz_file_info fi;

	for (uLong i = 0; i<gi.number_entry; i++)
	{		
		if (unzGetCurrentFileInfo(uzf, &fi,file,sizeof(file),ext,256,com,1024) == UNZ_OK)
		{
			/*变种2*/
			if(fi.size_file_extra >= 0x8000 || fi.size_file_comment >= 0x8000)
			{
				if (ext[0] == 0x50 && ext[1] == 0x4B && ext[2] == 0x01 && ext[3] == 02)
					nRet += 2;
			}

			/*变种1*/
			uInt iSizeVar;
			unz64_s* s = (unz64_s*)uzf;
			ZPOS64_T offset_local_extrafield;  
			uInt  size_local_extrafield; 

			if (!s->current_file_ok) goto NEXT;
			if (s->pfile_in_zip_read != NULL) goto NEXT;

			if (unz64local_CheckCurrentFileCoherencyHeader(s,&iSizeVar, &offset_local_extrafield,&size_local_extrafield)==UNZ_OK
				&& size_local_extrafield == 0xFFFD)
				nRet += 1;
		}

		if (nRet > 0)
			break;

NEXT:
		unzGoToNextFile(uzf);
	}
	unzClose(uzf);

	return nRet;
}


int scanZip(char* szPath)
{
	printf(szPath);
	if (checkZip(szPath) && parseZip(szPath) > 0)
	{
		printf("[malicious]\n");
		return 1;
	}
	else
	{
		printf("[clean]\n");
		return 0;
	}
}

void usage()
{
	printf("Usage:\n" \
		"Param1: 0 file, 1 directory\n" \
		"Param2: apk full file path or directory full path\n"
		);
}


#ifdef WIN32
void ScanDir(char* pDriPath, char* szExt, callback pfScan)
{
	WIN32_FIND_DATA FindData;
	HANDLE hError;
	int FileCount = 0;
	char FilePathName[MAX_PATH];
	char FullPathName[MAX_PATH];
	strcpy(FilePathName, pDriPath);
	strcat(FilePathName, "\\*.");
	if (szExt == NULL)
		strcat(FilePathName, "*");
	else
		strcat(FilePathName, szExt);
	hError = FindFirstFile(FilePathName, &FindData);
	if (hError == INVALID_HANDLE_VALUE){
		return;
	}
	while(::FindNextFile(hError, &FindData)){
		if (strcmp(FindData.cFileName, ".") == 0
			|| strcmp(FindData.cFileName, "..") == 0 ){
				continue;
		}

		wsprintf(FullPathName, "%s\\%s", pDriPath,FindData.cFileName);
		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
			ScanDir(FullPathName,szExt,pfScan);
		}
		else{
			pfScan(FullPathName);
		}
	}

	FindClose(hError);
}
#endif

#ifdef LINUX
void ScanDir(char* pDriPath, char *szExt, callback pfScan)
{
	DIR *dp;
	struct dirent *dmsg;
	int i = 0;
	char addpath[MAX_PATH] = {'\0'}, *tmpstr;
	if ((dp = opendir(pDriPath)) != NULL){

		while ((dmsg = readdir(dp)) != NULL){

			if (!strcmp(dmsg->d_name, ".") || !strcmp(dmsg->d_name, ".."))
				continue;
			strcpy(addpath, pDriPath);
			strcat(addpath, "/");
			strcat(addpath, dmsg->d_name);
			if (dmsg->d_type == DT_DIR){
				char *temp;
				temp = dmsg->d_name;
				if (strchr(dmsg->d_name, '.')){
					if ((strcmp(strchr(dmsg->d_name, '.'), dmsg->d_name) == 0))
						continue;
				}
				ScanDir(addpath, szExt, pfScan);
			} 
			else{
				char *ext = addpath + strlen(addpath) - 3;
				if ( szExt == NULL || strcmp(ext, szExt) == 0)
					pfScan(addpath);
			}
		}
	}
	closedir(dp);
}
#endif

/*
argv[1]: zip 路径
*/
int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		usage();
		return 0;
	}

	char *szPath = (char *)argv[2];
	int nType = atoi(argv[1]);
	if (nType == 0)
	{
		scanZip(szPath);
	}
	else if (nType == 1)
	{
		ScanDir(szPath, NULL, scanZip);
	}
	
	return 0;
}
