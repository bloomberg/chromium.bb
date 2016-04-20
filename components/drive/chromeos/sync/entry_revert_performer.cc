// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/sync/entry_revert_performer.h"

#include "components/drive/chromeos/change_list_processor.h"
#include "components/drive/chromeos/file_system/operation_delegate.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/file_change.h"
#include "components/drive/job_scheduler.h"
#include "components/drive/resource_entry_conversion.h"
#include "google_apis/drive/drive_api_parser.h"

namespace drive {
namespace internal {
namespace {

FileError FinishRevert(ResourceMetadata* metadata,
                       const std::string& local_id,
                       google_apis::DriveApiErrorCode status,
                       std::unique_ptr<google_apis::FileResource> file_resource,
                       FileChange* changed_files) {
  ResourceEntry entry;
  std::string parent_resource_id;
  FileError error = GDataToFileError(status);
  switch (error) {
    case FILE_ERROR_OK:
      if (!ConvertFileResourceToResourceEntry(*file_resource, &entry,
                                              &parent_resource_id))
        return FILE_ERROR_NOT_A_FILE;
      break;

    case FILE_ERROR_NOT_FOUND:
      entry.set_deleted(true);
      break;

    default:
      return error;
  }

  base::FilePath original_path;
  error = metadata->GetFilePath(local_id, &original_path);
  if (error != FILE_ERROR_OK)
    return error;

  if (entry.deleted()) {
    error = metadata->RemoveEntry(local_id);
    if (error != FILE_ERROR_OK)
      return error;

    changed_files->Update(
        original_path,
        FileChange::FILE_TYPE_NO_INFO,  // Undetermined type for deleted file.
        FileChange::CHANGE_TYPE_DELETE);
  } else {
    changed_files->Update(original_path, entry, FileChange::CHANGE_TYPE_DELETE);

    error = ChangeListProcessor::SetParentLocalIdOfEntry(metadata, &entry,
                                                         parent_resource_id);
    if (error != FILE_ERROR_OK)
      return error;

    entry.set_local_id(local_id);
    error = metadata->RefreshEntry(entry);
    if (error != FILE_ERROR_OK)
      return error;

    base::FilePath new_path;
    error = metadata->GetFilePath(local_id, &new_path);
    if (error != FILE_ERROR_OK)
      return error;

    changed_files->Update(new_path, entry,
                          FileChange::CHANGE_TYPE_ADD_OR_UPDATE);
  }
  return FILE_ERROR_OK;
}

}  // namespace

EntryRevertPerformer::EntryRevertPerformer(
    base::SequencedTaskRunner* blocking_task_runner,
    file_system::OperationDelegate* delegate,
    JobScheduler* scheduler,
    ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      delegate_(delegate),
      scheduler_(scheduler),
      metadata_(metadata),
      weak_ptr_factory_(this) {
}

EntryRevertPerformer::~EntryRevertPerformer() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void EntryRevertPerformer::RevertEntry(const std::string& local_id,
                                       const ClientContext& context,
                                       const FileOperationCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  std::unique_ptr<ResourceEntry> entry(new ResourceEntry);
  ResourceEntry* entry_ptr = entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ResourceMetadata::GetResourceEntryById,
                 base::Unretained(metadata_), local_id, entry_ptr),
      base::Bind(&EntryRevertPerformer::RevertEntryAfterPrepare,
                 weak_ptr_factory_.GetWeakPtr(), context, callback,
                 base::Passed(&entry)));
}

void EntryRevertPerformer::RevertEntryAfterPrepare(
    const ClientContext& context,
    const FileOperationCallback& callback,
    std::unique_ptr<ResourceEntry> entry,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK && entry->resource_id().empty())
    error = FILE_ERROR_INVALID_OPERATION;

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  scheduler_->GetFileResource(
      entry->resource_id(),
      context,
      base::Bind(&EntryRevertPerformer::RevertEntryAfterGetFileResource,
                 weak_ptr_factory_.GetWeakPtr(), callback, entry->local_id()));
}

void EntryRevertPerformer::RevertEntryAfterGetFileResource(
    const FileOperationCallback& callback,
    const std::string& local_id,
    google_apis::DriveApiErrorCode status,
    std::unique_ptr<google_apis::FileResource> entry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FileChange* changed_files = new FileChange;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&FinishRevert,
                 metadata_,
                 local_id,
                 status,
                 base::Passed(&entry),
                 changed_files),
      base::Bind(&EntryRevertPerformer::RevertEntryAfterFinishRevert,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(changed_files)));
}

void EntryRevertPerformer::RevertEntryAfterFinishRevert(
    const FileOperationCallback& callback,
    const FileChange* changed_files,
    FileError error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  delegate_->OnFileChangedByOperation(*changed_files);

  callback.Run(error);
}

}  // namespace internal
}  // namespace drive
