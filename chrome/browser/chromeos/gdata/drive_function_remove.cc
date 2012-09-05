// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_function_remove.h"

#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

DriveFunctionRemove::DriveFunctionRemove(DriveServiceInterface* drive_service,
                                         DriveFileSystem* file_system,
                                         DriveCache* cache)
  : drive_service_(drive_service),
    file_system_(file_system),
    cache_(cache),
    weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

DriveFunctionRemove::~DriveFunctionRemove() {
}

void DriveFunctionRemove::Remove(
    const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Get the edit URL of an entry at |file_path|.
  file_system_->ResourceMetadata()->GetEntryInfoByPath(
      file_path,
      base::Bind(
          &DriveFunctionRemove::RemoveAfterGetEntryInfo,
          weak_ptr_factory_.GetWeakPtr(),
          callback));
}

void DriveFunctionRemove::RemoveAfterGetEntryInfo(
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

  // The edit URL can be empty for some reason.
  if (entry_proto->edit_url().empty()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  drive_service_->DeleteDocument(
      GURL(entry_proto->edit_url()),
      base::Bind(&DriveFunctionRemove::RemoveResourceLocally,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 entry_proto->resource_id()));
}

void DriveFunctionRemove::RemoveResourceLocally(
    const FileOperationCallback& callback,
    const std::string& resource_id,
    GDataErrorCode status,
    const GURL& /* document_url */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  file_system_->ResourceMetadata()->RemoveEntryFromParent(
      resource_id,
      base::Bind(&DriveFunctionRemove::NotifyDirectoyChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));

  cache_->RemoveOnUIThread(resource_id, CacheOperationCallback());
}

void DriveFunctionRemove::NotifyDirectoyChanged(
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& directory_path) {
  if (error == DRIVE_FILE_OK)
    file_system_->OnDirectoryChanged(directory_path);

  if (!callback.is_null())
    callback.Run(error);
}

}  // namespace gdata
