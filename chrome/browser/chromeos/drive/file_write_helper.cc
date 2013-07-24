// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_write_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {

FileWriteHelper::FileWriteHelper(FileSystemInterface* file_system)
    : file_system_(file_system),
      weak_ptr_factory_(this) {
  // Must be created in DriveIntegrationService.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

FileWriteHelper::~FileWriteHelper() {
  // Must be destroyed in DriveIntegrationService.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileWriteHelper::PrepareWritableFileAndRun(
    const base::FilePath& file_path,
    const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  file_system_->OpenFile(
      file_path, OPEN_OR_CREATE_FILE,
      base::Bind(&FileWriteHelper::PrepareWritableFileAndRunAfterOpenFile,
                 weak_ptr_factory_.GetWeakPtr(), file_path, callback));
}

void FileWriteHelper::PrepareWritableFileAndRunAfterOpenFile(
    const base::FilePath& file_path,
    const OpenFileCallback& callback,
    FileError error,
    const base::FilePath& local_cache_path,
    const base::Closure& close_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE,
        base::Bind(callback, error, base::FilePath()));
    return;
  }

  content::BrowserThread::GetBlockingPool()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(callback, FILE_ERROR_OK, local_cache_path),
      close_callback);
}

}  // namespace drive
