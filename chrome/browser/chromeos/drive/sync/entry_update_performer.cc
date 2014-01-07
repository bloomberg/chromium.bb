// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/entry_update_performer.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/chromeos/drive/sync/entry_revert_performer.h"
#include "chrome/browser/chromeos/drive/sync/remove_performer.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

// Looks up ResourceEntry for source entry and its parent.
FileError PrepareUpdate(ResourceMetadata* metadata,
                        const std::string& local_id,
                        ResourceEntry* entry,
                        ResourceEntry* parent_entry) {
  FileError error = metadata->GetResourceEntryById(local_id, entry);
  if (error != FILE_ERROR_OK)
    return error;

  error = metadata->GetResourceEntryById(entry->parent_local_id(),
                                         parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  switch (entry->metadata_edit_state()) {
    case ResourceEntry::CLEAN:  // Nothing to do.
    case ResourceEntry::SYNCING:  // Error during the last update. Go ahead.
      break;

    case ResourceEntry::DIRTY:
      entry->set_metadata_edit_state(ResourceEntry::SYNCING);
      error = metadata->RefreshEntry(*entry);
      if (error != FILE_ERROR_OK)
        return error;
      break;
  }
  return FILE_ERROR_OK;
}

FileError FinishUpdate(ResourceMetadata* metadata,
                       const std::string& local_id) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryById(local_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  switch (entry.metadata_edit_state()) {
    case ResourceEntry::CLEAN:  // Nothing to do.
    case ResourceEntry::DIRTY:  // Entry was edited again during the update.
      break;

    case ResourceEntry::SYNCING:
      entry.set_metadata_edit_state(ResourceEntry::CLEAN);
      error = metadata->RefreshEntry(entry);
      if (error != FILE_ERROR_OK)
        return error;
      break;
  }
  return FILE_ERROR_OK;
}

}  // namespace

EntryUpdatePerformer::EntryUpdatePerformer(
    base::SequencedTaskRunner* blocking_task_runner,
    file_system::OperationObserver* observer,
    JobScheduler* scheduler,
    ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      scheduler_(scheduler),
      metadata_(metadata),
      remove_performer_(new RemovePerformer(blocking_task_runner,
                                            observer,
                                            scheduler,
                                            metadata)),
      entry_revert_performer_(new EntryRevertPerformer(blocking_task_runner,
                                                       observer,
                                                       scheduler,
                                                       metadata)),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

EntryUpdatePerformer::~EntryUpdatePerformer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void EntryUpdatePerformer::UpdateEntry(const std::string& local_id,
                                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  scoped_ptr<ResourceEntry> parent_entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  ResourceEntry* parent_entry_ptr = parent_entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&PrepareUpdate,
                 metadata_, local_id, entry_ptr, parent_entry_ptr),
      base::Bind(&EntryUpdatePerformer::UpdateEntryAfterPrepare,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Passed(&entry),
                 base::Passed(&parent_entry)));
}

void EntryUpdatePerformer::UpdateEntryAfterPrepare(
    const FileOperationCallback& callback,
    scoped_ptr<ResourceEntry> entry,
    scoped_ptr<ResourceEntry> parent_entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Trashed entry should be removed.
  if (entry->parent_local_id() == util::kDriveTrashDirLocalId) {
    remove_performer_->Remove(entry->local_id(), callback);
    return;
  }

  if (entry->metadata_edit_state() == ResourceEntry::CLEAN) {
    callback.Run(FILE_ERROR_OK);
    return;
  }

  std::string parent_resource_id;
  if (!util::IsSpecialResourceId(parent_entry->resource_id()))
    parent_resource_id = parent_entry->resource_id();

  base::Time last_modified =
      base::Time::FromInternalValue(entry->file_info().last_modified());
  base::Time last_accessed =
      base::Time::FromInternalValue(entry->file_info().last_accessed());
  scheduler_->UpdateResource(
      entry->resource_id(), parent_resource_id,
      entry->title(), last_modified, last_accessed,
      ClientContext(BACKGROUND),
      base::Bind(&EntryUpdatePerformer::UpdateEntryAfterUpdateResource,
                 weak_ptr_factory_.GetWeakPtr(), callback, entry->local_id()));
}

void EntryUpdatePerformer::UpdateEntryAfterUpdateResource(
    const FileOperationCallback& callback,
    const std::string& local_id,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (status == google_apis::HTTP_FORBIDDEN) {
    // Editing this entry is not allowed, revert local changes.
    entry_revert_performer_->RevertEntry(local_id, callback);
    return;
  }

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FinishUpdate, metadata_, local_id),
      callback);
}

}  // namespace internal
}  // namespace drive
