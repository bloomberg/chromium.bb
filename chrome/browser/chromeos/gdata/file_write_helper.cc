// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/file_write_helper.h"

#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {

// Emits debug log when DriveFileSystem::CloseFile() is complete.
void EmitDebugLogForCloseFile(const FilePath& file_path,
                              DriveFileError file_error) {
  if (file_error != DRIVE_FILE_OK) {
    LOG(WARNING) << "CloseFile failed: " << file_path.AsUTF8Unsafe() << ": "
                 << file_error;
  }
}

}  // namespace

FileWriteHelper::FileWriteHelper(DriveFileSystemInterface* file_system)
    : file_system_(file_system),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  // Must be created in DriveSystemService.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

FileWriteHelper::~FileWriteHelper() {
  // Must be destroyed in DriveSystemService.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileWriteHelper::PrepareWritableFileAndRun(
    const FilePath& file_path,
    const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_system_->CreateFile(
      file_path,
      false,  // it is not an error, even if the path already exists.
      base::Bind(&FileWriteHelper::PrepareWritableFileAndRunAfterCreateFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileWriteHelper::PrepareWritableFileAndRunAfterCreateFile(
    const FilePath& file_path,
    const OpenFileCallback& callback,
    DriveFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != gdata::DRIVE_FILE_OK) {
    if (!callback.is_null()) {
      content::BrowserThread::GetBlockingPool()->PostTask(
          FROM_HERE,
          base::Bind(callback, error, FilePath()));
    }
    return;
  }
  file_system_->OpenFile(
      file_path,
      base::Bind(&FileWriteHelper::PrepareWritableFileAndRunAfterOpenFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void FileWriteHelper::PrepareWritableFileAndRunAfterOpenFile(
    const FilePath& file_path,
    const OpenFileCallback& callback,
    DriveFileError error,
    const FilePath& local_cache_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != gdata::DRIVE_FILE_OK) {
    if (!callback.is_null()) {
      content::BrowserThread::GetBlockingPool()->PostTask(
          FROM_HERE,
          base::Bind(callback, error, FilePath()));
    }
    return;
  }

  if (!callback.is_null()) {
    content::BrowserThread::GetBlockingPool()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(callback, DRIVE_FILE_OK, local_cache_path),
        base::Bind(&FileWriteHelper::PrepareWritableFileAndRunAfterCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   file_path));
  } else {
    PrepareWritableFileAndRunAfterCallback(file_path);
  }
}

void FileWriteHelper::PrepareWritableFileAndRunAfterCallback(
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->CloseFile(file_path,
                          base::Bind(&EmitDebugLogForCloseFile, file_path));
}

}  // namespace gdata
