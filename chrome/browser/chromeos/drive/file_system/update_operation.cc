// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/update_operation.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

// Gets locally stored information about the specified file.
FileError GetFileLocalState(internal::ResourceMetadata* metadata,
                            internal::FileCache* cache,
                            const std::string& resource_id,
                            ResourceEntry* entry,
                            base::FilePath* drive_file_path,
                            base::FilePath* cache_file_path) {
  FileError error = metadata->GetResourceEntryById(resource_id, entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (entry->file_info().is_directory())
    return FILE_ERROR_NOT_A_FILE;

  *drive_file_path = metadata->GetFilePath(resource_id);
  if (drive_file_path->empty())
    return FILE_ERROR_NOT_FOUND;

  return cache->GetFile(resource_id,
                        entry->file_specific_info().md5(),
                        cache_file_path);
}

// Updates locally stored information about the specified file.
FileError UpdateFileLocalState(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    scoped_ptr<google_apis::ResourceEntry> resource_entry,
    base::FilePath* drive_file_path) {
  const ResourceEntry& entry = ConvertToResourceEntry(*resource_entry);
  if (entry.resource_id().empty())
    return FILE_ERROR_NOT_A_FILE;

  FileError error = metadata->RefreshEntry(entry);
  if (error != FILE_ERROR_OK)
    return error;

  *drive_file_path = metadata->GetFilePath(entry.resource_id());
  if (drive_file_path->empty())
    return FILE_ERROR_NOT_FOUND;

  // Clear the dirty bit if we have updated an existing file.
  return cache->ClearDirty(entry.resource_id(),
                           entry.file_specific_info().md5());
}

}  // namespace

UpdateOperation::UpdateOperation(
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

UpdateOperation::~UpdateOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void UpdateOperation::UpdateFileByResourceId(
    const std::string& resource_id,
    const ClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  ResourceEntry* entry = new ResourceEntry;
  base::FilePath* drive_file_path = new base::FilePath;
  base::FilePath* cache_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&GetFileLocalState,
                 metadata_,
                 cache_,
                 resource_id,
                 entry,
                 drive_file_path,
                 cache_file_path),
      base::Bind(&UpdateOperation::UpdateFileAfterGetLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 callback,
                 base::Owned(entry),
                 base::Owned(drive_file_path),
                 base::Owned(cache_file_path)));
}

void UpdateOperation::UpdateFileAfterGetLocalState(
    const ClientContext& context,
    const FileOperationCallback& callback,
    const ResourceEntry* entry,
    const base::FilePath* drive_file_path,
    const base::FilePath* cache_file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  scheduler_->UploadExistingFile(
      entry->resource_id(),
      *drive_file_path,
      *cache_file_path,
      entry->file_specific_info().content_mime_type(),
      "",  // etag
      context,
      base::Bind(&UpdateOperation::UpdateFileAfterUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void UpdateOperation::UpdateFileAfterUpload(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError drive_error = util::GDataToFileError(error);
  if (drive_error != FILE_ERROR_OK) {
    callback.Run(drive_error);
    return;
  }

  base::FilePath* drive_file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateFileLocalState,
                 metadata_,
                 cache_,
                 base::Passed(&resource_entry),
                 drive_file_path),
      base::Bind(&UpdateOperation::UpdateFileAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(drive_file_path)));
}

void UpdateOperation::UpdateFileAfterUpdateLocalState(
    const FileOperationCallback& callback,
    const base::FilePath* drive_file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }
  observer_->OnDirectoryChangedByOperation(drive_file_path->DirName());
  callback.Run(FILE_ERROR_OK);
}

}  // namespace file_system
}  // namespace drive
