// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"

#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

// Checks local metadata state before requesting remote delete.
FileError CheckLocalState(internal::ResourceMetadata* metadata,
                          const base::FilePath& path,
                          bool is_recursive,
                          ResourceEntry* entry) {
  FileError error = metadata->GetResourceEntryByPath(path, entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (entry->file_info().is_directory() && !is_recursive) {
    // Check emptiness of the directory.
    ResourceEntryVector entries;
    error = metadata->ReadDirectoryByPath(path, &entries);
    if (error != FILE_ERROR_OK)
      return error;
    if (!entries.empty())
      return FILE_ERROR_NOT_EMPTY;
  }

  return FILE_ERROR_OK;
}

// Updates local metadata and cache state after remote delete.
FileError UpdateLocalState(internal::ResourceMetadata* metadata,
                           internal::FileCache* cache,
                           const std::string& resource_id,
                           base::FilePath* changed_directory_path) {
  *changed_directory_path = metadata->GetFilePath(resource_id).DirName();
  FileError error = metadata->RemoveEntry(resource_id);
  if (error != FILE_ERROR_OK)
    return error;

  error = cache->Remove(resource_id);
  DLOG_IF(ERROR, error != FILE_ERROR_OK) << "Failed to remove: " << resource_id;

  return FILE_ERROR_OK;
}

}  // namespace

RemoveOperation::RemoveOperation(
    base::SequencedTaskRunner* blocking_task_runner,
    OperationObserver* observer,
    JobScheduler* scheduler,
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      scheduler_(scheduler),
      metadata_(metadata),
      cache_(cache),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

RemoveOperation::~RemoveOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void RemoveOperation::Remove(const base::FilePath& path,
                             bool is_recursive,
                             const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CheckLocalState, metadata_, path, is_recursive, entry),
      base::Bind(&RemoveOperation::RemoveAfterCheckLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(entry)));
}

void RemoveOperation::RemoveAfterCheckLocalState(
    const FileOperationCallback& callback,
    const ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  scheduler_->DeleteResource(
      entry->resource_id(),
      base::Bind(&RemoveOperation::RemoveAfterDeleteResource,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 entry->resource_id()));
}

void RemoveOperation::RemoveAfterDeleteResource(
    const FileOperationCallback& callback,
    const std::string& resource_id,
    google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  base::FilePath* changed_directory_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalState,
                 metadata_,
                 cache_,
                 resource_id,
                 changed_directory_path),
      base::Bind(&RemoveOperation::RemoveAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(changed_directory_path)));
}

void RemoveOperation::RemoveAfterUpdateLocalState(
    const FileOperationCallback& callback,
    const base::FilePath* changed_directory_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    observer_->OnDirectoryChangedByOperation(*changed_directory_path);

  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
