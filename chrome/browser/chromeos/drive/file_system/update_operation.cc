// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/update_operation.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

UpdateOperation::UpdateOperation(
    DriveCache* cache,
    DriveResourceMetadata* metadata,
    DriveScheduler* scheduler,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OperationObserver* observer)
    : cache_(cache),
      metadata_(metadata),
      scheduler_(scheduler),
      blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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

  // TODO(satorux): GetEntryInfoByResourceId() is called twice for
  // UpdateFileByResourceId(). crbug.com/143873
  metadata_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&UpdateOperation::UpdateFileByEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 callback));
}

void UpdateOperation::UpdateFileByEntryInfo(
    DriveClientContext context,
    const FileOperationCallback& callback,
    DriveFileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(entry_proto.get());
  if (entry_proto->file_info().is_directory()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  // Extract a pointer before we call Pass() so we can use it below.
  DriveEntryProto* entry_proto_ptr = entry_proto.get();
  cache_->GetFile(entry_proto_ptr->resource_id(),
                  entry_proto_ptr->file_specific_info().file_md5(),
                  base::Bind(&UpdateOperation::OnGetFileCompleteForUpdateFile,
                             weak_ptr_factory_.GetWeakPtr(),
                             context,
                             callback,
                             drive_file_path,
                             base::Passed(&entry_proto)));
}

void UpdateOperation::OnGetFileCompleteForUpdateFile(
    DriveClientContext context,
    const FileOperationCallback& callback,
    const base::FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto,
    DriveFileError error,
    const base::FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  scheduler_->UploadExistingFile(
      entry_proto->resource_id(),
      drive_file_path,
      cache_file_path,
      entry_proto->file_specific_info().content_mime_type(),
      "",  // etag
      context,
      base::Bind(&UpdateOperation::OnUpdatedFileUploaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void UpdateOperation::OnUpdatedFileUploaded(
    const FileOperationCallback& callback,
    google_apis::DriveUploadError error,
    const base::FilePath& drive_path,
    const base::FilePath& file_path,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileError drive_error = DriveUploadErrorToDriveFileError(error);
  if (drive_error != DRIVE_FILE_OK) {
    callback.Run(drive_error);
    return;
  }

  metadata_->RefreshEntry(
      ConvertResourceEntryToDriveEntryProto(*resource_entry),
      base::Bind(&UpdateOperation::OnUpdatedFileRefreshed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void UpdateOperation::OnUpdatedFileRefreshed(
    const FileOperationCallback& callback,
    DriveFileError error,
    const base::FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(entry_proto.get());
  DCHECK(entry_proto->has_resource_id());
  DCHECK(entry_proto->has_file_specific_info());
  DCHECK(entry_proto->file_specific_info().has_file_md5());

  observer_->OnDirectoryChangedByOperation(drive_file_path.DirName());

  // Clear the dirty bit if we have updated an existing file.
  cache_->ClearDirty(entry_proto->resource_id(),
                     entry_proto->file_specific_info().file_md5(),
                     callback);
}

}  // namespace file_system
}  // namespace drive
