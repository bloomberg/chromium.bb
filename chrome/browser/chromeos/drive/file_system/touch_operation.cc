// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/touch_operation.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

TouchOperation::TouchOperation(base::SequencedTaskRunner* blocking_task_runner,
                               OperationObserver* observer,
                               JobScheduler* scheduler,
                               internal::ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      scheduler_(scheduler),
      metadata_(metadata),
      weak_ptr_factory_(this) {
}

TouchOperation::~TouchOperation() {
}

void TouchOperation::TouchFile(const base::FilePath& file_path,
                               const base::Time& last_access_time,
                               const base::Time& last_modified_time,
                               const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::GetResourceEntryByPath,
                 base::Unretained(metadata_),
                 file_path,
                 entry),
      base::Bind(&TouchOperation::TouchFileAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 last_access_time,
                 last_modified_time,
                 callback,
                 base::Owned(entry)));
}

void TouchOperation::TouchFileAfterGetResourceEntry(
    const base::FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const FileOperationCallback& callback,
    ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(entry);

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Note: |last_modified_time| is mapped to modifiedDate, |last_access_time|
  // is mapped to lastViewedByMeDate. See also ConvertToResourceEntry().
  scheduler_->TouchResource(
      entry->resource_id(), last_modified_time, last_access_time,
      base::Bind(&TouchOperation::TouchFileAfterServerTimeStampUpdated,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path, callback));
}

void TouchOperation::TouchFileAfterServerTimeStampUpdated(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode gdata_error,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(gdata_error);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  DCHECK(resource_entry);
  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id)) {
    callback.Run(FILE_ERROR_NOT_A_FILE);
    return;
  }

  // TODO(hashimoto): Resolve local ID before use. crbug.com/260514
  entry.set_parent_local_id(parent_resource_id);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::RefreshEntry,
                 base::Unretained(metadata_),
                 entry.resource_id(),
                 entry),
      base::Bind(&TouchOperation::TouchFileAfterRefreshMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 callback));
}

void TouchOperation::TouchFileAfterRefreshMetadata(
    const base::FilePath& file_path,
    const FileOperationCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    observer_->OnDirectoryChangedByOperation(file_path.DirName());

  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
