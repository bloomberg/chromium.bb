// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/lzma_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

extern "C" {
#include "third_party/lzma_sdk/7z.h"
#include "third_party/lzma_sdk/7zAlloc.h"
#include "third_party/lzma_sdk/7zCrc.h"
#include "third_party/lzma_sdk/7zFile.h"
}


namespace {

SRes LzmaReadFile(HANDLE file, void *data, size_t *size) {
  if (*size == 0)
    return SZ_OK;

  size_t processedSize = 0;
  DWORD maxSize = *size;
  do {
    DWORD processedLoc = 0;
    BOOL res = ReadFile(file, data, maxSize, &processedLoc, NULL);
    data = (void *)((unsigned char *) data + processedLoc);
    maxSize -= processedLoc;
    processedSize += processedLoc;
    if (processedLoc == 0) {
      if (res)
        return SZ_ERROR_READ;
      else
        break;
    }
  } while (maxSize > 0);

  *size = processedSize;
  return SZ_OK;
}

SRes SzFileSeekImp(void *object, Int64 *pos, ESzSeek origin) {
  CFileInStream *s = (CFileInStream *) object;
  LARGE_INTEGER value;
  value.LowPart = (DWORD) *pos;
  value.HighPart = (LONG) ((UInt64) *pos >> 32);
  DWORD moveMethod;
  switch (origin) {
    case SZ_SEEK_SET:
      moveMethod = FILE_BEGIN;
      break;
    case SZ_SEEK_CUR:
      moveMethod = FILE_CURRENT;
      break;
    case SZ_SEEK_END:
      moveMethod = FILE_END;
      break;
    default:
      return SZ_ERROR_PARAM;
  }
  value.LowPart = SetFilePointer(s->file.handle, value.LowPart, &value.HighPart,
                                 moveMethod);
  *pos = ((Int64)value.HighPart << 32) | value.LowPart;
  return ((value.LowPart == 0xFFFFFFFF) && (GetLastError() != NO_ERROR)) ?
      SZ_ERROR_FAIL : SZ_OK;
}

SRes SzFileReadImp(void *object, void *buffer, size_t *size) {
  CFileInStream *s = (CFileInStream *) object;
  return LzmaReadFile(s->file.handle, buffer, size);
}

}  // namespace

// static
int32 LzmaUtil::UnPackArchive(const std::wstring& archive,
                             const std::wstring& output_dir,
                             std::wstring* output_file) {
  VLOG(1) << "Opening archive " << archive;
  LzmaUtil lzma_util;
  DWORD ret;
  if ((ret = lzma_util.OpenArchive(archive)) != NO_ERROR) {
    LOG(ERROR) << "Unable to open install archive: " << archive
               << ", error: " << ret;
  } else {
    VLOG(1) << "Uncompressing archive to path " << output_dir;
    if ((ret = lzma_util.UnPack(output_dir, output_file)) != NO_ERROR) {
      LOG(ERROR) << "Unable to uncompress archive: " << archive
                 << ", error: " << ret;
    }
    lzma_util.CloseArchive();
  }

  return ret;
}

LzmaUtil::LzmaUtil() : archive_handle_(NULL) {}

LzmaUtil::~LzmaUtil() {
  CloseArchive();
}

DWORD LzmaUtil::OpenArchive(const std::wstring& archivePath) {
  // Make sure file is not already open.
  CloseArchive();

  DWORD ret = NO_ERROR;
  archive_handle_ = CreateFile(archivePath.c_str(), GENERIC_READ,
      FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (archive_handle_ == INVALID_HANDLE_VALUE) {
    archive_handle_ = NULL;  // The rest of the code only checks for NULL.
    ret = GetLastError();
  }
  return ret;
}

DWORD LzmaUtil::UnPack(const std::wstring& location) {
  return UnPack(location, NULL);
}

DWORD LzmaUtil::UnPack(const std::wstring& location,
                       std::wstring* output_file) {
  if (!archive_handle_)
    return ERROR_INVALID_HANDLE;

  CFileInStream archiveStream;
  CLookToRead lookStream;
  CSzArEx db;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  DWORD ret = NO_ERROR;

  archiveStream.file.handle = archive_handle_;
  archiveStream.s.Read = SzFileReadImp;
  archiveStream.s.Seek = SzFileSeekImp;
  LookToRead_CreateVTable(&lookStream, false);
  lookStream.realStream = &archiveStream.s;

  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  CrcGenerateTable();
  SzArEx_Init(&db);
  if ((ret = SzArEx_Open(&db, &lookStream.s,
                         &allocImp, &allocTempImp)) != SZ_OK) {
    LOG(ERROR) << L"Error returned by SzArchiveOpen: " << ret;
    return ERROR_INVALID_HANDLE;
  }

  Byte *outBuffer = 0; // it must be 0 before first call for each new archive
  UInt32 blockIndex = 0xFFFFFFFF; // can have any value if outBuffer = 0
  size_t outBufferSize = 0;  // can have any value if outBuffer = 0

  for (unsigned int i = 0; i < db.db.NumFiles; i++) {
    DWORD written;
    size_t offset;
    size_t outSizeProcessed;
    CSzFileItem *f = db.db.Files + i;

    if ((ret = SzArEx_Extract(&db, &lookStream.s, i, &blockIndex,
                         &outBuffer, &outBufferSize, &offset, &outSizeProcessed,
                         &allocImp, &allocTempImp)) != SZ_OK) {
      LOG(ERROR) << L"Error returned by SzExtract: " << ret;
      ret = ERROR_INVALID_HANDLE;
      break;
    }

    size_t file_name_length = SzArEx_GetFileNameUtf16(&db, i, NULL);
    if (file_name_length < 1) {
      LOG(ERROR) << L"Couldn't get file name";
      ret = ERROR_INVALID_HANDLE;
      break;
    }
    std::vector<UInt16> file_name(file_name_length);
    SzArEx_GetFileNameUtf16(&db, i, &file_name[0]);
    // |file_name| is NULL-terminated.
    base::FilePath file_path = base::FilePath(location).Append(
        base::FilePath::StringType(file_name.begin(), file_name.end() - 1));

    if (output_file)
      *output_file = file_path.value();

    // If archive entry is directory create it and move on to the next entry.
    if (f->IsDir) {
      CreateDirectory(file_path);
      continue;
    }

    CreateDirectory(file_path.DirName());

    HANDLE hFile;
    hFile = CreateFile(file_path.value().c_str(), GENERIC_WRITE, 0, NULL,
                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)  {
      ret = GetLastError();
      LOG(ERROR) << L"Error returned by CreateFile: " << ret;
      break;
    }

    if ((!WriteFile(hFile, outBuffer + offset, (DWORD) outSizeProcessed,
                    &written, NULL)) ||
        (written != outSizeProcessed)) {
      ret = GetLastError();
      CloseHandle(hFile);
      LOG(ERROR) << L"Error returned by WriteFile: " << ret;
      break;
    }

    if (f->MTimeDefined) {
      if (!SetFileTime(hFile, NULL, NULL,
                       (const FILETIME *)&(f->MTime))) {
        ret = GetLastError();
        CloseHandle(hFile);
        LOG(ERROR) << L"Error returned by SetFileTime: " << ret;
        break;
      }
    }
    if (!CloseHandle(hFile)) {
      ret = GetLastError();
      LOG(ERROR) << L"Error returned by CloseHandle: " << ret;
      break;
    }
  }  // for loop

  IAlloc_Free(&allocImp, outBuffer);
  SzArEx_Free(&db, &allocImp);
  return ret;
}

void LzmaUtil::CloseArchive() {
  if (archive_handle_) {
    CloseHandle(archive_handle_);
    archive_handle_ = NULL;
  }
}

bool LzmaUtil::CreateDirectory(const base::FilePath& dir) {
  bool ret = true;
  if (directories_created_.find(dir.value()) == directories_created_.end()) {
    ret = base::CreateDirectory(dir);
    if (ret)
      directories_created_.insert(dir.value());
  }
  return ret;
}
