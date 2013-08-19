// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/get_file_for_saving_operation.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_write_watcher.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

GetFileForSavingOperation::GetFileForSavingOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    JobScheduler* scheduler,
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& temporary_file_directory)
    : create_file_operation_(new CreateFileOperation(blocking_task_runner,
                                                     observer,
                                                     scheduler,
                                                     metadata,
                                                     cache)),
      download_operation_(new DownloadOperation(blocking_task_runner,
                                                observer,
                                                scheduler,
                                                metadata,
                                                cache,
                                                temporary_file_directory)),
      file_write_watcher_(new internal::FileWriteWatcher(observer)),
      cache_(cache),
      weak_ptr_factory_(this) {
}

GetFileForSavingOperation::~GetFileForSavingOperation() {
}

void GetFileForSavingOperation::GetFileForSaving(
    const base::FilePath& file_path,
    const GetFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  create_file_operation_->CreateFile(
      file_path,
      false,  // error_if_already_exists
      std::string(),  // no specific mime type
      base::Bind(&GetFileForSavingOperation::GetFileForSavingAfterCreate,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void GetFileForSavingOperation::GetFileForSavingAfterCreate(
    const base::FilePath& file_path,
    const GetFileCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<ResourceEntry>());
    return;
  }

  download_operation_->EnsureFileDownloadedByPath(
      file_path,
      ClientContext(USER_INITIATED),
      GetFileContentInitializedCallback(),
      google_apis::GetContentCallback(),
      base::Bind(&GetFileForSavingOperation::GetFileForSavingAfterDownload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void GetFileForSavingOperation::GetFileForSavingAfterDownload(
    const GetFileCallback& callback,
    FileError error,
    const base::FilePath& cache_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<ResourceEntry>());
    return;
  }

  const std::string& resource_id = entry->resource_id();
  cache_->MarkDirtyOnUIThread(
      resource_id,
      base::Bind(&GetFileForSavingOperation::GetFileForSavingAfterMarkDirty,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 cache_path,
                 base::Passed(&entry)));
}

void GetFileForSavingOperation::GetFileForSavingAfterMarkDirty(
    const GetFileCallback& callback,
    const base::FilePath& cache_path,
    scoped_ptr<ResourceEntry> entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error, base::FilePath(), scoped_ptr<ResourceEntry>());
    return;
  }

  const std::string& resource_id = entry->resource_id();
  file_write_watcher_->StartWatch(
      cache_path,
      resource_id,
      base::Bind(&GetFileForSavingOperation::GetFileForSavingAfterWatch,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 cache_path,
                 base::Passed(&entry)));
}

void GetFileForSavingOperation::GetFileForSavingAfterWatch(
    const GetFileCallback& callback,
    const base::FilePath& cache_path,
    scoped_ptr<ResourceEntry> entry,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!success) {
    callback.Run(FILE_ERROR_FAILED,
                 base::FilePath(), scoped_ptr<ResourceEntry>());
    return;
  }

  callback.Run(FILE_ERROR_OK, cache_path, entry.Pass());
}

}  // namespace file_system
}  // namespace drive
