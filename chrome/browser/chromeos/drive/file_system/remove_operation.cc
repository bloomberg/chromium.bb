// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

namespace {

void EmptyFileOperationCallback(DriveFileError error) {}

}  // namespace

RemoveOperation::RemoveOperation(
    DriveScheduler* drive_scheduler,
    DriveCache* cache,
    DriveResourceMetadata* metadata,
    OperationObserver* observer)
  : drive_scheduler_(drive_scheduler),
    cache_(cache),
    metadata_(metadata),
    observer_(observer),
    weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

RemoveOperation::~RemoveOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void RemoveOperation::Remove(
    const base::FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Get the edit URL of an entry at |file_path|.
  metadata_->GetEntryInfoByPath(
      file_path,
      base::Bind(
          &RemoveOperation::RemoveAfterGetEntryInfo,
          weak_ptr_factory_.GetWeakPtr(),
          callback));
}

void RemoveOperation::RemoveAfterGetEntryInfo(
    const FileOperationCallback& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }
  DCHECK(entry_proto.get());

  drive_scheduler_->DeleteResource(
      entry_proto->resource_id(),
      base::Bind(&RemoveOperation::RemoveResourceLocally,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 entry_proto->resource_id()));
}

void RemoveOperation::RemoveResourceLocally(
    const FileOperationCallback& callback,
    const std::string& resource_id,
    google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  metadata_->RemoveEntry(resource_id,
                         base::Bind(&RemoveOperation::NotifyDirectoryChanged,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback));

  cache_->Remove(resource_id, base::Bind(&EmptyFileOperationCallback));
}

void RemoveOperation::NotifyDirectoryChanged(
    const FileOperationCallback& callback,
    DriveFileError error,
    const base::FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == DRIVE_FILE_OK)
    observer_->OnDirectoryChangedByOperation(directory_path);

  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
