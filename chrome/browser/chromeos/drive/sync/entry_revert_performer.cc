// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/sync/entry_revert_performer.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
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
                       scoped_ptr<google_apis::ResourceEntry> resource_entry,
                       std::set<base::FilePath>* changed_directories) {
  ResourceEntry entry;
  std::string parent_resource_id;
  FileError error = GDataToFileError(status);
  switch (error) {
    case FILE_ERROR_OK:
      if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id))
        return FILE_ERROR_NOT_A_FILE;
      break;

    case FILE_ERROR_NOT_FOUND:
      entry.set_deleted(true);
      break;

    default:
      return error;
  }

  const base::FilePath original_path = metadata->GetFilePath(local_id);

  if (entry.deleted()) {
    error = metadata->RemoveEntry(local_id);
    if (error != FILE_ERROR_OK)
      return error;

    changed_directories->insert(original_path.DirName());
  } else {
    std::string parent_local_id;
    error = metadata->GetIdByResourceId(parent_resource_id, &parent_local_id);
    if (error != FILE_ERROR_OK)
      return error;

    entry.set_local_id(local_id);
    entry.set_parent_local_id(parent_local_id);
    error = metadata->RefreshEntry(entry);
    if (error != FILE_ERROR_OK)
      return error;

    changed_directories->insert(metadata->GetFilePath(entry.parent_local_id()));
    changed_directories->insert(original_path.DirName());
  }
  return FILE_ERROR_OK;
}

}  // namespace

EntryRevertPerformer::EntryRevertPerformer(
    base::SequencedTaskRunner* blocking_task_runner,
    file_system::OperationObserver* observer,
    JobScheduler* scheduler,
    ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
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
  DCHECK(!callback.is_null());

  std::set<base::FilePath>* changed_directories = new std::set<base::FilePath>;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FinishRevert, metadata_, local_id, status,
                 base::Passed(&resource_entry), changed_directories),
      base::Bind(&EntryRevertPerformer::RevertEntryAfterFinishRevert,
                 weak_ptr_factory_.GetWeakPtr(), callback,
                 base::Owned(changed_directories)));
}

void EntryRevertPerformer::RevertEntryAfterFinishRevert(
    const FileOperationCallback& callback,
    const std::set<base::FilePath>* changed_directories,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  for (std::set<base::FilePath>::const_iterator it =
           changed_directories->begin(); it != changed_directories->end(); ++it)
    observer_->OnDirectoryChangedByOperation(*it);

  callback.Run(error);
}

}  // namespace internal
}  // namespace drive
