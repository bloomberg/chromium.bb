// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/update_operation.h"

#include "base/file_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

UpdateOperation::UpdateOperation(
    DriveCache* cache,
    DriveResourceMetadata* metadata,
    google_apis::DriveUploaderInterface* uploader,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OperationObserver* observer)
    : cache_(cache),
      metadata_(metadata),
      uploader_(uploader),
      blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

UpdateOperation::~UpdateOperation() {
}

void UpdateOperation::UpdateFileByResourceId(
    const std::string& resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // TODO(satorux): GetEntryInfoByResourceId() is called twice for
  // UpdateFileByResourceId(). crbug.com/143873
  metadata_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&UpdateOperation::UpdateFileByEntryInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void UpdateOperation::UpdateFileByEntryInfo(
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& drive_file_path,
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
                             callback,
                             drive_file_path,
                             base::Passed(&entry_proto)));
}

void UpdateOperation::OnGetFileCompleteForUpdateFile(
    const FileOperationCallback& callback,
    const FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto,
    DriveFileError error,
    const FilePath& cache_file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  // Gets the size of the cache file. Since the file is locally modified, the
  // file size information stored in DriveEntry is not correct.
  int64* file_size = new int64(0);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_,
      FROM_HERE,
      base::Bind(&file_util::GetFileSize,
                 cache_file_path,
                 file_size),
      base::Bind(&UpdateOperation::OnGetFileSizeCompleteForUpdateFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 drive_file_path,
                 base::Passed(&entry_proto),
                 cache_file_path,
                 base::Owned(file_size)));
}

void UpdateOperation::OnGetFileSizeCompleteForUpdateFile(
    const FileOperationCallback& callback,
    const FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto,
    const FilePath& cache_file_path,
    int64* file_size,
    bool get_file_size_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  // |entry_proto| has been checked in UpdateFileByEntryInfo().
  DCHECK(entry_proto.get());
  DCHECK(!entry_proto->file_info().is_directory());

  if (!get_file_size_result) {
    callback.Run(DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  uploader_->UploadExistingFile(
      GURL(entry_proto->upload_url()),
      drive_file_path,
      cache_file_path,
      entry_proto->file_specific_info().content_mime_type(),
      *file_size,
      base::Bind(&UpdateOperation::OnUpdatedFileUploaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void UpdateOperation::OnUpdatedFileUploaded(
    const FileOperationCallback& callback,
    google_apis::DriveUploadError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileError drive_error = DriveUploadErrorToDriveFileError(error);
  if (drive_error != DRIVE_FILE_OK) {
    callback.Run(drive_error);
    return;
  }

  metadata_->RefreshFile(
      resource_entry.Pass(),
      base::Bind(&UpdateOperation::OnUpdatedFileRefreshed,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void UpdateOperation::OnUpdatedFileRefreshed(
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& drive_file_path,
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
