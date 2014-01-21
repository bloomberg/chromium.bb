// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/content_update_performer.h"

#include "base/platform_file.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

struct ContentUpdatePerformer::LocalState {
  LocalState() : should_upload(false) {
  }

  std::string local_id;
  ResourceEntry entry;
  base::FilePath drive_file_path;
  base::FilePath cache_file_path;
  bool should_upload;
};

namespace {

// Gets locally stored information about the specified file.
FileError GetFileLocalState(internal::ResourceMetadata* metadata,
                            internal::FileCache* cache,
                            ContentUpdatePerformer::LocalState* local_state) {
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

  // Do not upload the file if it's still opened.
  if (cache->IsOpenedForWrite(local_state->local_id)) {
    local_state->should_upload = false;
    return FILE_ERROR_OK;
  }

  FileCacheEntry cache_entry;
  if (!cache->GetCacheEntry(local_state->local_id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  // Update cache entry's MD5 if needed.
  if (cache_entry.md5().empty()) {
    error = cache->UpdateMd5(local_state->local_id);
    if (error != FILE_ERROR_OK)
      return error;
    if (!cache->GetCacheEntry(local_state->local_id, &cache_entry))
      return FILE_ERROR_NOT_FOUND;
  }

  local_state->should_upload =
      (cache_entry.md5() != local_state->entry.file_specific_info().md5());
  if (!local_state->should_upload)
    return cache->ClearDirty(local_state->local_id);
  return FILE_ERROR_OK;
}

// Updates locally stored information about the specified file.
FileError UpdateFileLocalState(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const std::string& local_id,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  ResourceEntry existing_entry;
  FileError error = metadata->GetResourceEntryById(local_id, &existing_entry);
  if (error != FILE_ERROR_OK)
    return error;

  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id))
    return FILE_ERROR_NOT_A_FILE;

  entry.set_local_id(local_id);
  entry.set_parent_local_id(existing_entry.parent_local_id());

  error = metadata->RefreshEntry(entry);
  if (error != FILE_ERROR_OK)
    return error;

  FileCacheEntry cache_entry;
  if (!cache->GetCacheEntry(local_id, &cache_entry))
    return FILE_ERROR_NOT_FOUND;

  // Do not clear dirty bit if the file has been edited during update.
  if (cache_entry.md5() != entry.file_specific_info().md5())
    return FILE_ERROR_OK;
  return cache->ClearDirty(local_id);
}

}  // namespace

ContentUpdatePerformer::ContentUpdatePerformer(
    base::SequencedTaskRunner* blocking_task_runner,
    JobScheduler* scheduler,
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache)
    : blocking_task_runner_(blocking_task_runner),
      scheduler_(scheduler),
      metadata_(metadata),
      cache_(cache),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ContentUpdatePerformer::~ContentUpdatePerformer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ContentUpdatePerformer::UpdateFileByLocalId(
    const std::string& local_id,
    const ClientContext& context,
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
                 local_state),
      base::Bind(&ContentUpdatePerformer::UpdateFileAfterGetLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 context,
                 callback,
                 base::Owned(local_state)));
}

void ContentUpdatePerformer::UpdateFileAfterGetLocalState(
    const ClientContext& context,
    const FileOperationCallback& callback,
    const LocalState* local_state,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK || !local_state->should_upload) {
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
      base::Bind(&ContentUpdatePerformer::UpdateFileAfterUpload,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 local_state->local_id));
}

void ContentUpdatePerformer::UpdateFileAfterUpload(
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

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateFileLocalState,
                 metadata_,
                 cache_,
                 local_id,
                 base::Passed(&resource_entry)),
      callback);
}

}  // namespace file_system
}  // namespace drive
