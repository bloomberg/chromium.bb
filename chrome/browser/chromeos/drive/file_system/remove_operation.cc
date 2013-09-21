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
// |parent_resource_id| is set to the resource ID of the parent directory of
// |path|. If it is a special folder like drive/other, empty string is set.
// |local_id| is set to the local ID of the entry located at |path|.
// |entry| is the resource entry for the |path|.
FileError CheckLocalState(internal::ResourceMetadata* metadata,
                          const base::FilePath& path,
                          bool is_recursive,
                          std::string* parent_resource_id,
                          std::string* local_id,
                          ResourceEntry* entry) {
  FileError error = metadata->GetIdByPath(path, local_id);
  if (error != FILE_ERROR_OK)
    return error;

  error = metadata->GetResourceEntryById(*local_id, entry);
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

  // Get the resource_id of the parent folder. If it is a special folder that
  // does not have the server side ID, returns an empty string (not an error).
  if (util::IsSpecialResourceId(entry->parent_local_id())) {
    *parent_resource_id = "";
  } else {
    ResourceEntry parent_entry;
    error = metadata->GetResourceEntryById(entry->parent_local_id(),
                                           &parent_entry);
    if (error != FILE_ERROR_OK)
        return error;
    *parent_resource_id = parent_entry.resource_id();
  }

  return FILE_ERROR_OK;
}

// Updates local metadata and cache state after remote delete.
FileError UpdateLocalStateAfterDelete(internal::ResourceMetadata* metadata,
                                      internal::FileCache* cache,
                                      const std::string& local_id,
                                      base::FilePath* changed_directory_path) {
  *changed_directory_path = metadata->GetFilePath(local_id).DirName();
  FileError error = metadata->RemoveEntry(local_id);
  if (error != FILE_ERROR_OK)
    return error;

  error = cache->Remove(local_id);
  DLOG_IF(ERROR, error != FILE_ERROR_OK) << "Failed to remove: " << local_id;

  return FILE_ERROR_OK;
}

// Updates local metadata and after remote unparenting.
FileError UpdateLocalStateAfterUnparent(
    internal::ResourceMetadata* metadata,
    const std::string& local_id,
    base::FilePath* changed_directory_path) {
  *changed_directory_path = metadata->GetFilePath(local_id).DirName();

  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryById(local_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;
  entry.set_parent_local_id(util::kDriveOtherDirSpecialResourceId);
  return metadata->RefreshEntry(local_id, entry);
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

  std::string* parent_resource_id = new std::string;
  std::string* local_id = new std::string;
  ResourceEntry* entry = new ResourceEntry;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&CheckLocalState,
                 metadata_,
                 path,
                 is_recursive,
                 parent_resource_id,
                 local_id,
                 entry),
      base::Bind(&RemoveOperation::RemoveAfterCheckLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(parent_resource_id),
                 base::Owned(local_id),
                 base::Owned(entry)));
}

void RemoveOperation::RemoveAfterCheckLocalState(
    const FileOperationCallback& callback,
    const std::string* parent_resource_id,
    const std::string* local_id,
    const ResourceEntry* entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // To match with the behavior of drive.google.com:
  // Removal of shared entries under MyDrive is just removing from the parent.
  // The entry will stay in shared-with-me (in other words, in "drive/other".)
  //
  // TODO(kinaba): to be more precise, we might be better to branch by whether
  // or not the current account is an owner of the file. The code below is
  // written under the assumption that |shared_with_me| coincides with that.
  if (entry->shared_with_me() && !parent_resource_id->empty()) {
    scheduler_->RemoveResourceFromDirectory(
        *parent_resource_id,
        entry->resource_id(),
        base::Bind(&RemoveOperation::RemoveAfterUpdateRemoteState,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   base::Bind(&UpdateLocalStateAfterUnparent,
                              metadata_,
                              *local_id)));
  } else {
    // Otherwise try sending the entry to trash.
    scheduler_->DeleteResource(
        entry->resource_id(),
        base::Bind(&RemoveOperation::RemoveAfterUpdateRemoteState,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   base::Bind(&UpdateLocalStateAfterDelete,
                              metadata_,
                              cache_,
                              *local_id)));
  }
}

void RemoveOperation::RemoveAfterUpdateRemoteState(
    const FileOperationCallback& callback,
    const base::Callback<FileError(base::FilePath*)>& local_update_task,
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
      base::Bind(local_update_task, changed_directory_path),
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
