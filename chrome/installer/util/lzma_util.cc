// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/lzma_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"

extern "C" {
#include "third_party/lzma_sdk/Archive/7z/7zExtract.h"
#include "third_party/lzma_sdk/Archive/7z/7zIn.h"
#include "third_party/lzma_sdk/7zCrc.h"
}


namespace {

typedef struct _CFileInStream {
  ISzInStream InStream;
  HANDLE File;
} CFileInStream;


size_t LzmaReadFile(HANDLE file, void *data, size_t size) {
  if (size == 0)
    return 0;

  size_t processedSize = 0;
  do {
    DWORD processedLoc = 0;
    BOOL res = ReadFile(file, data, (DWORD) size, &processedLoc, NULL);
    data = (void *)((unsigned char *) data + processedLoc);
    size -= processedLoc;
    processedSize += processedLoc;
    if (!res || processedLoc == 0)
      break;
  } while (size > 0);

  return processedSize;
}

SZ_RESULT SzFileSeekImp(void *object, CFileSize pos) {
  CFileInStream *s = (CFileInStream *) object;
  LARGE_INTEGER value;
  value.LowPart = (DWORD) pos;
  value.HighPart = (LONG) ((UInt64) pos >> 32);
  value.LowPart = SetFilePointer(s->File, value.LowPart, &value.HighPart,
                                 FILE_BEGIN);
  return ((value.LowPart == 0xFFFFFFFF) && (GetLastError() != NO_ERROR)) ?
      SZE_FAIL : SZ_OK;
}

SZ_RESULT SzFileReadImp(void *object, void **buffer,
                        size_t maxRequiredSize, size_t *processedSize) {
  const int kBufferSize = 1 << 12;
  static Byte g_Buffer[kBufferSize];
  if (maxRequiredSize > kBufferSize)
    maxRequiredSize = kBufferSize;

  CFileInStream *s = (CFileInStream *) object;
  size_t processedSizeLoc;
  processedSizeLoc = LzmaReadFile(s->File, g_Buffer, maxRequiredSize);
  *buffer = g_Buffer;
  if (processedSize != 0)
    *processedSize = processedSizeLoc;
  return SZ_OK;
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
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  CArchiveDatabaseEx db;
  DWORD ret = NO_ERROR;

  archiveStream.File = archive_handle_;
  archiveStream.InStream.Read = SzFileReadImp;
  archiveStream.InStream.Seek = SzFileSeekImp;
  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;

  CrcGenerateTable();
  SzArDbExInit(&db);
  if ((ret = SzArchiveOpen(&archiveStream.InStream, &db,
                           &allocImp, &allocTempImp)) != SZ_OK) {
    LOG(ERROR) << L"Error returned by SzArchiveOpen: " << ret;
    return ret;
  }

  Byte *outBuffer = 0; // it must be 0 before first call for each new archive
  UInt32 blockIndex = 0xFFFFFFFF; // can have any value if outBuffer = 0
  size_t outBufferSize = 0;  // can have any value if outBuffer = 0

  for (unsigned int i = 0; i < db.Database.NumFiles; i++) {
    DWORD written;
    size_t offset;
    size_t outSizeProcessed;
    CFileItem *f = db.Database.Files + i;

    if ((ret = SzExtract(&archiveStream.InStream, &db, i, &blockIndex,
                         &outBuffer, &outBufferSize, &offset, &outSizeProcessed,
                         &allocImp, &allocTempImp)) != SZ_OK) {
      LOG(ERROR) << L"Error returned by SzExtract: " << ret;
      break;
    }

    FilePath file_path = FilePath(location).Append(UTF8ToWide(f->Name));
    if (output_file)
      *output_file = file_path.value();

    // If archive entry is directory create it and move on to the next entry.
    if (f->IsDirectory) {
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

    if (f->IsLastWriteTimeDefined) {
      if (!SetFileTime(hFile, NULL, NULL,
                       (const FILETIME *)&(f->LastWriteTime))) {
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

  allocImp.Free(outBuffer);
  SzArDbExFree(&db, allocImp.Free);
  return ret;
}

void LzmaUtil::CloseArchive() {
  if (archive_handle_) {
    CloseHandle(archive_handle_);
    archive_handle_ = NULL;
  }
}

bool LzmaUtil::CreateDirectory(const FilePath& dir) {
  bool ret = true;
  if (directories_created_.find(dir.value()) == directories_created_.end()) {
    ret = file_util::CreateDirectory(dir);
    if (ret)
      directories_created_.insert(dir.value());
  }
  return ret;
}
