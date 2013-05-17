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

UpdateOperation::UpdateOperation(OperationObserver* observer,
                                 JobScheduler* scheduler,
                                 internal::ResourceMetadata* metadata,
                                 internal::FileCache* cache)
    : observer_(observer),
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
    DriveClientContext context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  metadata_->GetResourceEntryByIdOnUIThread(
      resource_id,
      base::Bind(&UpdateOperation::UpdateFileAfterGetEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 callback));
}

void UpdateOperation::UpdateFileAfterGetEntryInfo(
    DriveClientContext context,
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(entry);
  if (entry->file_info().is_directory()) {
    callback.Run(FILE_ERROR_NOT_A_FILE);
    return;
  }

  // Extract a pointer before we call Pass() so we can use it below.
  ResourceEntry* entry_ptr = entry.get();
  cache_->GetFileOnUIThread(
      entry_ptr->resource_id(),
      entry_ptr->file_specific_info().file_md5(),
      base::Bind(&UpdateOperation::UpdateFileAfterGetFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 callback,
                 drive_file_path,
                 base::Passed(&entry)));
}

void UpdateOperation::UpdateFileAfterGetFile(
    DriveClientContext context,
    const FileOperationCallback& callback,
    const base::FilePath& drive_file_path,
    scoped_ptr<ResourceEntry> entry,
    FileError error,
    const base::FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  scheduler_->UploadExistingFile(
      entry->resource_id(),
      drive_file_path,
      cache_file_path,
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

  metadata_->RefreshEntryOnUIThread(
      ConvertToResourceEntry(*resource_entry),
      base::Bind(&UpdateOperation::UpdateFileAfterRefresh,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void UpdateOperation::UpdateFileAfterRefresh(
    const FileOperationCallback& callback,
    FileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(entry);
  DCHECK(entry->has_resource_id());
  DCHECK(entry->has_file_specific_info());
  DCHECK(entry->file_specific_info().has_file_md5());

  observer_->OnDirectoryChangedByOperation(drive_file_path.DirName());

  // Clear the dirty bit if we have updated an existing file.
  cache_->ClearDirtyOnUIThread(entry->resource_id(),
                               entry->file_specific_info().file_md5(),
                               callback);
}

}  // namespace file_system
}  // namespace drive
