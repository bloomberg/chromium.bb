// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/file_system/remove_operation.h"

#include <math.h>

#include "base/rand_util.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {

namespace {
int kMaxRetries = 5;
}

namespace file_system {

RemoveOperation::RemoveOperation(DriveServiceInterface* drive_service,
                                         DriveFileSystem* file_system,
                                         DriveCache* cache)
  : drive_service_(drive_service),
    file_system_(file_system),
    cache_(cache),
    weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

RemoveOperation::~RemoveOperation() {
}

void RemoveOperation::Remove(
    const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Get the edit URL of an entry at |file_path|.
  file_system_->ResourceMetadata()->GetEntryInfoByPath(
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

  // The edit URL can be empty for some reason.
  if (entry_proto->edit_url().empty()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_FOUND);
    return;
  }

  DoDelete(callback, 0, entry_proto.Pass());
}

void RemoveOperation::DoDelete(
    const FileOperationCallback& callback,
    int retry_count,
    scoped_ptr<DriveEntryProto> entry_proto) {
  GURL edit_url(entry_proto->edit_url());
  drive_service_->DeleteDocument(
      edit_url,
      base::Bind(&RemoveOperation::RetryIfNeeded,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 retry_count + 1,
                 base::Passed(&entry_proto)));
}

void RemoveOperation::RetryIfNeeded(
    const FileOperationCallback& callback,
    int retry_count,
    scoped_ptr<DriveEntryProto> entry_proto,
    GDataErrorCode status,
    const GURL& /* document_url */) {
  // If the call failed due to flooding the server, and we haven't hit the
  // maximum number of retries, throttle and retry.
  //
  // TODO(zork): Move this retry logic into the scheduler when it is completed.
  if ((status == HTTP_SERVICE_UNAVAILABLE ||
       status == HTTP_INTERNAL_SERVER_ERROR) &&
      retry_count < kMaxRetries) {
    base::TimeDelta delay =
        base::TimeDelta::FromSeconds(pow(2, retry_count)) +
        base::TimeDelta::FromMilliseconds(base::RandInt(0, 1000));

    VLOG(1) << "Throttling for " << delay.InMillisecondsF();
    const bool posted = base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&RemoveOperation::DoDelete,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   retry_count,
                   base::Passed(&entry_proto)),
        delay);
    DCHECK(posted);
  } else {
    // If the operation completed, or we hit max retries, continue.
    RemoveResourceLocally(callback,
                          entry_proto->resource_id(),
                          status);
  }
}

void RemoveOperation::RemoveResourceLocally(
    const FileOperationCallback& callback,
    const std::string& resource_id,
    GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  file_system_->ResourceMetadata()->RemoveEntryFromParent(
      resource_id,
      base::Bind(&RemoveOperation::NotifyDirectoryChanged,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));

  cache_->RemoveOnUIThread(resource_id, CacheOperationCallback());
}

void RemoveOperation::NotifyDirectoryChanged(
    const FileOperationCallback& callback,
    DriveFileError error,
    const FilePath& directory_path) {
  if (error == DRIVE_FILE_OK)
    file_system_->OnDirectoryChanged(directory_path);

  if (!callback.is_null())
    callback.Run(error);
}

}  // namespace file_system
}  // namespace gdata
