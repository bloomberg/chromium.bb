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

namespace {

// Returns ResourceEntry and the local ID of the entry at the given path.
FileError GetResourceEntryAndIdByPath(internal::ResourceMetadata* metadata,
                                      const base::FilePath& file_path,
                                      std::string* local_id,
                                      ResourceEntry* entry) {
  FileError error = metadata->GetIdByPath(file_path, local_id);
  if (error != FILE_ERROR_OK)
    return error;
  return metadata->GetResourceEntryById(*local_id, entry);
}

// Refreshes the entry specified by |local_id| with the contents of
// |resource_entry|.
FileError RefreshEntry(internal::ResourceMetadata* metadata,
                       const std::string& local_id,
                       scoped_ptr<google_apis::ResourceEntry> resource_entry,
                       base::FilePath* file_path) {
  DCHECK(resource_entry);

  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id))
    return FILE_ERROR_NOT_A_FILE;

  std::string parent_local_id;
  FileError error = metadata->GetIdByResourceId(parent_resource_id,
                                                &parent_local_id);
  if (error != FILE_ERROR_OK)
    return error;

  entry.set_local_id(local_id);
  entry.set_parent_local_id(parent_local_id);

  error = metadata->RefreshEntry(entry);
  if (error != FILE_ERROR_OK)
    return error;

  *file_path = metadata->GetFilePath(local_id);
  return file_path->empty() ? FILE_ERROR_FAILED : FILE_ERROR_OK;
}

}  // namespace

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
  std::string* local_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&GetResourceEntryAndIdByPath,
                 base::Unretained(metadata_),
                 file_path,
                 local_id,
                 entry),
      base::Bind(&TouchOperation::TouchFileAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 last_access_time,
                 last_modified_time,
                 callback,
                 base::Owned(local_id),
                 base::Owned(entry)));
}

void TouchOperation::TouchFileAfterGetResourceEntry(
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const FileOperationCallback& callback,
    std::string* local_id,
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
                 *local_id, callback));
}

void TouchOperation::TouchFileAfterServerTimeStampUpdated(
    const std::string& local_id,
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

  base::FilePath* file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&RefreshEntry,
                 base::Unretained(metadata_),
                 local_id,
                 base::Passed(&resource_entry),
                 file_path),
      base::Bind(&TouchOperation::TouchFileAfterRefreshMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(file_path),
                 callback));
}

void TouchOperation::TouchFileAfterRefreshMetadata(
    const base::FilePath* file_path,
    const FileOperationCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    observer_->OnDirectoryChangedByOperation(file_path->DirName());

  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
