// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/update_operation.h"

#include "base/platform_file.h"
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

struct UpdateOperation::LocalState {
  LocalState() : content_is_same(false) {
  }

  std::string local_id;
  ResourceEntry entry;
  base::FilePath drive_file_path;
  base::FilePath cache_file_path;
  bool content_is_same;
};

namespace {

// Gets locally stored information about the specified file.
FileError GetFileLocalState(internal::ResourceMetadata* metadata,
                            internal::FileCache* cache,
                            UpdateOperation::ContentCheckMode check,
                            UpdateOperation::LocalState* local_state) {
  FileError error = metadata->GetResourceEntryById(local_state->local_id,
                                                   &local_state->entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (local_state->entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_FILE;

  local_state->drive_file_path = metadata->GetFilePath(local_state->local_id);
  if (local_state->drive_file_path.empty())
    return FILE_ERROR_NOT_FOUND;

  error = cache->GetFile(local_state->local_id, &local_state->cache_file_path);
  if (error != FILE_ERROR_OK)
    return error;

  if (check == UpdateOperation::RUN_CONTENT_CHECK) {
    const std::string& md5 = util::GetMd5Digest(local_state->cache_file_path);
    local_state->content_is_same =
        (md5 == local_state->entry.file_specific_info().md5());
    if (local_state->content_is_same)
      cache->ClearDirty(local_state->local_id, md5);
  } else {
    local_state->content_is_same = false;
  }

  return FILE_ERROR_OK;
}

// Updates locally stored information about the specified file.
FileError UpdateFileLocalState(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const std::string& local_id,
    scoped_ptr<google_apis::ResourceEntry> resource_entry,
    base::FilePath* drive_file_path) {
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

  *drive_file_path = metadata->GetFilePath(local_id);
  if (drive_file_path->empty())
    return FILE_ERROR_NOT_FOUND;

  // Clear the dirty bit if we have updated an existing file.
  return cache->ClearDirty(local_id, entry.file_specific_info().md5());
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

void UpdateOperation::UpdateFileByLocalId(
    const std::string& local_id,
    const ClientContext& context,
    ContentCheckMode check,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  LocalState* local_state = new LocalState;
  local_state->local_id = local_id;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&GetFileLocalState,
                 metadata_,
                 cache_,
                 check,
                 local_state),
      base::Bind(&UpdateOperation::UpdateFileAfterGetLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 callback,
                 base::Owned(local_state)));
}

void UpdateOperation::UpdateFileAfterGetLocalState(
    const ClientContext& context,
    const FileOperationCallback& callback,
    const LocalState* local_state,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK || local_state->content_is_same) {
    callback.Run(error);
    return;
  }

  scheduler_->UploadExistingFile(
      local_state->entry.resource_id(),
      local_state->drive_file_path,
      local_state->cache_file_path,
      local_state->entry.file_specific_info().content_mime_type(),
      "",  // etag
      context,
      base::Bind(&UpdateOperation::UpdateFileAfterUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 local_state->local_id));
}

void UpdateOperation::UpdateFileAfterUpload(
    const FileOperationCallback& callback,
    const std::string& local_id,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError drive_error = GDataToFileError(error);
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
                 local_id,
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
