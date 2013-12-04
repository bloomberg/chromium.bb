// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/entry_revert_performer.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace internal {
namespace {

FileError FinishRevert(ResourceMetadata* metadata,
                       const std::string& local_id,
                       google_apis::GDataErrorCode status,
                       scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  FileError error = GDataToFileError(status);
  if (error == FILE_ERROR_NOT_FOUND)
    return metadata->RemoveEntry(local_id);

  if (error != FILE_ERROR_OK)
    return error;

  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id))
    return FILE_ERROR_NOT_A_FILE;

  if (entry.deleted())
    return metadata->RemoveEntry(local_id);

  std::string parent_local_id;
  error = metadata->GetIdByResourceId(parent_resource_id, &parent_local_id);
  if (error != FILE_ERROR_OK)
    return error;

  entry.set_local_id(local_id);
  entry.set_parent_local_id(parent_local_id);

  return metadata->RefreshEntry(entry);
}

}  // namespace

EntryRevertPerformer::EntryRevertPerformer(
    base::SequencedTaskRunner* blocking_task_runner,
    JobScheduler* scheduler,
    ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      scheduler_(scheduler),
      metadata_(metadata),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

EntryRevertPerformer::~EntryRevertPerformer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void EntryRevertPerformer::RevertEntry(const std::string& local_id,
                                       const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetResourceEntryById,
                 base::Unretained(metadata_), local_id, entry_ptr),
      base::Bind(&EntryRevertPerformer::RevertEntryAfterPrepare,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Passed(&entry)));
}

void EntryRevertPerformer::RevertEntryAfterPrepare(
    const FileOperationCallback& callback,
    scoped_ptr<ResourceEntry> entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  scheduler_->GetResourceEntry(
      entry->resource_id(),
      ClientContext(BACKGROUND),
      base::Bind(&EntryRevertPerformer::RevertEntryAfterGetResourceEntry,
                 weak_ptr_factory_.GetWeakPtr(), callback, entry->local_id()));
}

void EntryRevertPerformer::RevertEntryAfterGetResourceEntry(
    const FileOperationCallback& callback,
    const std::string& local_id,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(hashimoto): Notify OnDirectoryChanged event for affected directories.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FinishRevert, metadata_, local_id, status,
                 base::Passed(&resource_entry)),
      callback);
}

}  // namespace internal
}  // namespace drive
