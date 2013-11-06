// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/get_file_for_saving_operation.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
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
      file_write_watcher_(new internal::FileWriteWatcher),
      blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      metadata_(metadata),
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

  const std::string& local_id = entry->local_id();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::FileCache::MarkDirty,
                 base::Unretained(cache_),
                 local_id),
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

  const std::string& local_id = entry->local_id();
  file_write_watcher_->StartWatch(
      cache_path,
      base::Bind(&GetFileForSavingOperation::GetFileForSavingAfterWatch,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 cache_path,
                 base::Passed(&entry)),
      base::Bind(&GetFileForSavingOperation::OnWriteEvent,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_id));
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

void GetFileForSavingOperation::OnWriteEvent(const std::string& local_id) {
  observer_->OnCacheFileUploadNeededByOperation(local_id);

  // Clients may have enlarged the file. By FreeDiskpSpaceIfNeededFor(0),
  // we try to ensure (0 + the-minimum-safe-margin = 512MB as of now) space.
  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(
          base::Bind(&internal::FileCache::FreeDiskSpaceIfNeededFor,
                     base::Unretained(cache_),
                     0))));
}

}  // namespace file_system
}  // namespace drive
