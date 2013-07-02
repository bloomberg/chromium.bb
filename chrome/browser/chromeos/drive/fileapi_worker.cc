// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/fileapi_worker.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

// Runs |callback| with the PlatformFileError converted from |error|.
void RunStatusCallbackByFileError(
    const FileApiWorker::StatusCallback& callback,
    FileError error) {
  callback.Run(FileErrorToPlatformError(error));
}

}  // namespace

FileApiWorker::FileApiWorker(FileSystemInterface* file_system)
    : file_system_(file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(file_system);
}

FileApiWorker::~FileApiWorker() {
}

void FileApiWorker::Copy(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->Copy(src_file_path, dest_file_path,
                     base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::Move(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->Move(src_file_path, dest_file_path,
                     base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::Remove(const base::FilePath& file_path,
                           bool is_recursive,
                           const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->Remove(file_path, is_recursive,
                       base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::CreateDirectory(const base::FilePath& file_path,
                                    bool is_exclusive,
                                    bool is_recursive,
                                    const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->CreateDirectory(
      file_path, is_exclusive, is_recursive,
      base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::CreateFile(const base::FilePath& file_path,
                               bool is_exclusive,
                               const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->CreateFile(file_path, is_exclusive,
                           base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::Truncate(const base::FilePath& file_path,
                             int64 length,
                             const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->TruncateFile(
      file_path, length,
      base::Bind(&RunStatusCallbackByFileError, callback));
}

void FileApiWorker::TouchFile(const base::FilePath& file_path,
                              const base::Time& last_access_time,
                              const base::Time& last_modified_time,
                              const StatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  file_system_->TouchFile(file_path, last_access_time, last_modified_time,
                          base::Bind(&RunStatusCallbackByFileError, callback));

}

}  // namespace internal
}  // namespace drive
