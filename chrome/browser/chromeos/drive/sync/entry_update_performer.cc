// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/entry_update_performer.h"

#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "chrome/browser/chromeos/drive/change_list_loader.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_change.h"
#include "chrome/browser/chromeos/drive/file_system/operation_delegate.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/sync/entry_revert_performer.h"
#include "chrome/browser/chromeos/drive/sync/remove_performer.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"

using content::BrowserThread;

namespace drive {
namespace internal {

struct EntryUpdatePerformer::LocalState {
  LocalState() : should_content_update(false) {
  }

  ResourceEntry entry;
  ResourceEntry parent_entry;
  base::FilePath drive_file_path;
  base::FilePath cache_file_path;
  bool should_content_update;
};

namespace {

// Looks up ResourceEntry for source entry and its parent.
FileError PrepareUpdate(ResourceMetadata* metadata,
                        FileCache* cache,
                        const std::string& local_id,
                        EntryUpdatePerformer::LocalState* local_state) {
  FileError error = metadata->GetResourceEntryById(local_id,
                                                   &local_state->entry);
  if (error != FILE_ERROR_OK)
    return error;

  error = metadata->GetResourceEntryById(local_state->entry.parent_local_id(),
                                         &local_state->parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  error = metadata->GetFilePath(local_id, &local_state->drive_file_path);
  if (error != FILE_ERROR_OK)
    return error;

  if (!local_state->entry.file_info().is_directory() &&
      !local_state->entry.file_specific_info().cache_state().is_present() &&
      local_state->entry.resource_id().empty()) {
    // Locally created file with no cache file, store an empty file.
    base::FilePath empty_file;
    if (!base::CreateTemporaryFile(&empty_file))
      return FILE_ERROR_FAILED;
    error = cache->Store(local_id, std::string(), empty_file,
                         FileCache::FILE_OPERATION_MOVE);
    if (error != FILE_ERROR_OK)
      return error;
    error = metadata->GetResourceEntryById(local_id, &local_state->entry);
    if (error != FILE_ERROR_OK)
      return error;
  }

  // Check if content update is needed or not.
  if (local_state->entry.file_specific_info().cache_state().is_dirty() &&
      !cache->IsOpenedForWrite(local_id)) {
    // Update cache entry's MD5 if needed.
    if (local_state->entry.file_specific_info().cache_state().md5().empty()) {
      error = cache->UpdateMd5(local_id);
      if (error != FILE_ERROR_OK)
        return error;
      error = metadata->GetResourceEntryById(local_id, &local_state->entry);
      if (error != FILE_ERROR_OK)
        return error;
    }

    if (local_state->entry.file_specific_info().cache_state().md5() ==
        local_state->entry.file_specific_info().md5()) {
      error = cache->ClearDirty(local_id);
      if (error != FILE_ERROR_OK)
        return error;
    } else {
      error = cache->GetFile(local_id, &local_state->cache_file_path);
      if (error != FILE_ERROR_OK)
        return error;

      local_state->should_content_update = true;
    }
  }

  // Update metadata_edit_state.
  switch (local_state->entry.metadata_edit_state()) {
    case ResourceEntry::CLEAN:  // Nothing to do.
    case ResourceEntry::SYNCING:  // Error during the last update. Go ahead.
      break;

    case ResourceEntry::DIRTY:
      local_state->entry.set_metadata_edit_state(ResourceEntry::SYNCING);
      error = metadata->RefreshEntry(local_state->entry);
      if (error != FILE_ERROR_OK)
        return error;
      break;
  }
  return FILE_ERROR_OK;
}

FileError FinishUpdate(ResourceMetadata* metadata,
                       FileCache* cache,
                       const std::string& local_id,
                       scoped_ptr<google_apis::FileResource> file_resource,
                       FileChange* changed_files) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryById(local_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  // When creating new entries, update check may add a new entry with the same
  // resource ID before us. If such an entry exists, remove it.
  std::string existing_local_id;
  error =
      metadata->GetIdByResourceId(file_resource->file_id(), &existing_local_id);

  switch (error) {
    case FILE_ERROR_OK:
      if (existing_local_id != local_id) {
        base::FilePath existing_entry_path;
        error = metadata->GetFilePath(existing_local_id, &existing_entry_path);
        if (error != FILE_ERROR_OK)
          return error;
        error = metadata->RemoveEntry(existing_local_id);
        if (error != FILE_ERROR_OK)
          return error;
        changed_files->Update(existing_entry_path, entry, FileChange::DELETE);
      }
      break;
    case FILE_ERROR_NOT_FOUND:
      break;
    default:
      return error;
  }

  // Update metadata_edit_state and MD5.
  switch (entry.metadata_edit_state()) {
    case ResourceEntry::CLEAN:  // Nothing to do.
    case ResourceEntry::DIRTY:  // Entry was edited again during the update.
      break;

    case ResourceEntry::SYNCING:
      entry.set_metadata_edit_state(ResourceEntry::CLEAN);
      break;
  }
  if (!entry.file_info().is_directory())
    entry.mutable_file_specific_info()->set_md5(file_resource->md5_checksum());
  entry.set_resource_id(file_resource->file_id());
  error = metadata->RefreshEntry(entry);
  if (error != FILE_ERROR_OK)
    return error;
  base::FilePath entry_path;
  error = metadata->GetFilePath(local_id, &entry_path);
  if (error != FILE_ERROR_OK)
    return error;
  changed_files->Update(entry_path, entry, FileChange::ADD_OR_UPDATE);

  // Clear dirty bit unless the file has been edited during update.
  if (entry.file_specific_info().cache_state().is_dirty() &&
      entry.file_specific_info().cache_state().md5() ==
      entry.file_specific_info().md5()) {
    error = cache->ClearDirty(local_id);
    if (error != FILE_ERROR_OK)
      return error;
  }
  return FILE_ERROR_OK;
}

}  // namespace

EntryUpdatePerformer::EntryUpdatePerformer(
    base::SequencedTaskRunner* blocking_task_runner,
    file_system::OperationDelegate* delegate,
    JobScheduler* scheduler,
    ResourceMetadata* metadata,
    FileCache* cache,
    LoaderController* loader_controller)
    : blocking_task_runner_(blocking_task_runner),
      delegate_(delegate),
      scheduler_(scheduler),
      metadata_(metadata),
      cache_(cache),
      loader_controller_(loader_controller),
      remove_performer_(new RemovePerformer(blocking_task_runner,
                                            delegate,
                                            scheduler,
                                            metadata)),
      entry_revert_performer_(new EntryRevertPerformer(blocking_task_runner,
                                                       delegate,
                                                       scheduler,
                                                       metadata)),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

EntryUpdatePerformer::~EntryUpdatePerformer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void EntryUpdatePerformer::UpdateEntry(const std::string& local_id,
                                       const ClientContext& context,
                                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<LocalState> local_state(new LocalState);
  LocalState* local_state_ptr = local_state.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&PrepareUpdate, metadata_, cache_, local_id, local_state_ptr),
      base::Bind(&EntryUpdatePerformer::UpdateEntryAfterPrepare,
                 weak_ptr_factory_.GetWeakPtr(), context, callback,
                 base::Passed(&local_state)));
}

void EntryUpdatePerformer::UpdateEntryAfterPrepare(
    const ClientContext& context,
    const FileOperationCallback& callback,
    scoped_ptr<LocalState> local_state,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Trashed entry should be removed.
  if (local_state->entry.parent_local_id() == util::kDriveTrashDirLocalId) {
    remove_performer_->Remove(local_state->entry.local_id(), context, callback);
    return;
  }

  // Parent was locally created and needs update. Just return for now.
  // This entry should be updated again after the parent update completes.
  if (local_state->parent_entry.resource_id().empty() &&
      local_state->parent_entry.metadata_edit_state() != ResourceEntry::CLEAN) {
    callback.Run(FILE_ERROR_OK);
    return;
  }

  base::Time last_modified = base::Time::FromInternalValue(
      local_state->entry.file_info().last_modified());
  base::Time last_accessed = base::Time::FromInternalValue(
      local_state->entry.file_info().last_accessed());

  // Perform content update.
  if (local_state->should_content_update) {
    if (local_state->entry.resource_id().empty()) {
      // Not locking the loader intentionally here to avoid making the UI
      // unresponsive while uploading large files.
      // FinishUpdate() is responsible to resolve conflicts caused by this.
      scoped_ptr<base::ScopedClosureRunner> null_loader_lock;

      DriveUploader::UploadNewFileOptions options;
      options.modified_date = last_modified;
      options.last_viewed_by_me_date = last_accessed;
      scheduler_->UploadNewFile(
          local_state->parent_entry.resource_id(),
          local_state->drive_file_path,
          local_state->cache_file_path,
          local_state->entry.title(),
          local_state->entry.file_specific_info().content_mime_type(),
          options,
          context,
          base::Bind(&EntryUpdatePerformer::UpdateEntryAfterUpdateResource,
                     weak_ptr_factory_.GetWeakPtr(),
                     context,
                     callback,
                     local_state->entry.local_id(),
                     base::Passed(&null_loader_lock)));
    } else {
      DriveUploader::UploadExistingFileOptions options;
      options.title = local_state->entry.title();
      options.parent_resource_id = local_state->parent_entry.resource_id();
      options.modified_date = last_modified;
      options.last_viewed_by_me_date = last_accessed;
      scheduler_->UploadExistingFile(
          local_state->entry.resource_id(),
          local_state->drive_file_path,
          local_state->cache_file_path,
          local_state->entry.file_specific_info().content_mime_type(),
          options,
          context,
          base::Bind(&EntryUpdatePerformer::UpdateEntryAfterUpdateResource,
                     weak_ptr_factory_.GetWeakPtr(),
                     context,
                     callback,
                     local_state->entry.local_id(),
                     base::Passed(scoped_ptr<base::ScopedClosureRunner>())));
    }
    return;
  }

  // Create directory.
  if (local_state->entry.file_info().is_directory() &&
      local_state->entry.resource_id().empty()) {
    // Lock the loader to avoid race conditions.
    scoped_ptr<base::ScopedClosureRunner> loader_lock =
        loader_controller_->GetLock();

    DriveServiceInterface::AddNewDirectoryOptions options;
    options.modified_date = last_modified;
    options.last_viewed_by_me_date = last_accessed;
    scheduler_->AddNewDirectory(
        local_state->parent_entry.resource_id(),
        local_state->entry.title(),
        options,
        context,
        base::Bind(&EntryUpdatePerformer::UpdateEntryAfterUpdateResource,
                   weak_ptr_factory_.GetWeakPtr(),
                   context,
                   callback,
                   local_state->entry.local_id(),
                   base::Passed(&loader_lock)));
    return;
  }

  // No need to perform update.
  if (local_state->entry.metadata_edit_state() == ResourceEntry::CLEAN ||
      local_state->entry.resource_id().empty()) {
    callback.Run(FILE_ERROR_OK);
    return;
  }

  // Perform metadata update.
  scheduler_->UpdateResource(
      local_state->entry.resource_id(), local_state->parent_entry.resource_id(),
      local_state->entry.title(), last_modified, last_accessed,
      context,
      base::Bind(&EntryUpdatePerformer::UpdateEntryAfterUpdateResource,
                 weak_ptr_factory_.GetWeakPtr(),
                 context, callback, local_state->entry.local_id(),
                 base::Passed(scoped_ptr<base::ScopedClosureRunner>())));
}

void EntryUpdatePerformer::UpdateEntryAfterUpdateResource(
    const ClientContext& context,
    const FileOperationCallback& callback,
    const std::string& local_id,
    scoped_ptr<base::ScopedClosureRunner> loader_lock,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::FileResource> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (status == google_apis::HTTP_FORBIDDEN) {
    // Editing this entry is not allowed, revert local changes.
    entry_revert_performer_->RevertEntry(local_id, context, callback);
    return;
  }

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  FileChange* changed_files = new FileChange;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FinishUpdate,
                 metadata_,
                 cache_,
                 local_id,
                 base::Passed(&entry),
                 changed_files),
      base::Bind(&EntryUpdatePerformer::UpdateEntryAfterFinish,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(changed_files)));
}

void EntryUpdatePerformer::UpdateEntryAfterFinish(
    const FileOperationCallback& callback,
    const FileChange* changed_files,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  delegate_->OnFileChangedByOperation(*changed_files);
  callback.Run(error);
}

}  // namespace internal
}  // namespace drive
