// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/entry_update_performer.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
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

  return metadata->GetResourceEntryById(entry->parent_local_id(), parent_entry);
}

}  // namespace

EntryUpdatePerformer::EntryUpdatePerformer(
    base::SequencedTaskRunner* blocking_task_runner,
    JobScheduler* scheduler,
    ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      scheduler_(scheduler),
      metadata_(metadata),
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

  base::Time last_modified =
      base::Time::FromInternalValue(entry->file_info().last_modified());
  base::Time last_accessed =
      base::Time::FromInternalValue(entry->file_info().last_accessed());
  scheduler_->UpdateResource(
      entry->resource_id(), parent_entry->resource_id(),
      entry->title(), last_modified, last_accessed,
      base::Bind(&EntryUpdatePerformer::UpdateEntryAfterUpdateResource,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void EntryUpdatePerformer::UpdateEntryAfterUpdateResource(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  callback.Run(GDataToFileError(status));
}

}  // namespace internal
}  // namespace drive
