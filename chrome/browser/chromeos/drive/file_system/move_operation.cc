// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/move_operation.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {
namespace {

// Looks up ResourceEntry for source entry and the destination directory.
FileError PrepareMove(internal::ResourceMetadata* metadata,
                      const base::FilePath& src_path,
                      const base::FilePath& dest_parent_path,
                      ResourceEntry* src_entry,
                      ResourceEntry* src_parent_entry,
                      ResourceEntry* dest_parent_entry) {
  FileError error = metadata->GetResourceEntryByPath(src_path, src_entry);
  if (error != FILE_ERROR_OK)
    return error;

  error = metadata->GetResourceEntryById(src_entry->parent_local_id(),
                                         src_parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  return metadata->GetResourceEntryByPath(dest_parent_path, dest_parent_entry);
}

// Applies renaming to the local metadata.
FileError RenameLocally(internal::ResourceMetadata* metadata,
                        const std::string& local_id,
                        const std::string& new_title) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryById(local_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  entry.set_title(new_title);
  return metadata->RefreshEntry(entry);
}

// Applies directory-moving to the local metadata.
FileError MoveDirectoryLocally(internal::ResourceMetadata* metadata,
                               const std::string& local_id,
                               const std::string& parent_local_id) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryById(local_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  entry.set_parent_local_id(parent_local_id);
  return metadata->RefreshEntry(entry);
}

// Refreshes the corresponding entry in the metadata with the given one.
FileError RefreshEntry(internal::ResourceMetadata* metadata,
                       scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id))
    return FILE_ERROR_FAILED;

  std::string parent_local_id;
  FileError error = metadata->GetIdByResourceId(parent_resource_id,
                                                &parent_local_id);
  if (error != FILE_ERROR_OK)
    return error;
  entry.set_parent_local_id(parent_local_id);

  std::string local_id;
  error = metadata->GetIdByResourceId(entry.resource_id(), &local_id);
  if (error != FILE_ERROR_OK)
    return error;
  entry.set_local_id(local_id);

  return metadata->RefreshEntry(entry);
}

}  // namespace

struct MoveOperation::MoveParams {
  base::FilePath src_file_path;
  base::FilePath dest_file_path;
  bool preserve_last_modified;
  FileOperationCallback callback;
};

MoveOperation::MoveOperation(base::SequencedTaskRunner* blocking_task_runner,
                             OperationObserver* observer,
                             JobScheduler* scheduler,
                             internal::ResourceMetadata* metadata)
    : blocking_task_runner_(blocking_task_runner),
      observer_(observer),
      scheduler_(scheduler),
      metadata_(metadata),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

MoveOperation::~MoveOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void MoveOperation::Move(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         bool preserve_last_modified,
                         const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  MoveParams params;
  params.src_file_path = src_file_path;
  params.dest_file_path = dest_file_path;
  params.preserve_last_modified = preserve_last_modified;
  params.callback = callback;

  scoped_ptr<ResourceEntry> src_entry(new ResourceEntry);
  scoped_ptr<ResourceEntry> src_parent_entry(new ResourceEntry);
  scoped_ptr<ResourceEntry> dest_parent_entry(new ResourceEntry);
  ResourceEntry* src_entry_ptr = src_entry.get();
  ResourceEntry* src_parent_entry_ptr = src_parent_entry.get();
  ResourceEntry* dest_parent_entry_ptr = dest_parent_entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&PrepareMove,
                 metadata_, src_file_path, dest_file_path.DirName(),
                 src_entry_ptr, src_parent_entry_ptr, dest_parent_entry_ptr),
      base::Bind(&MoveOperation::MoveAfterPrepare,
                 weak_ptr_factory_.GetWeakPtr(), params,
                 base::Passed(&src_entry),
                 base::Passed(&src_parent_entry),
                 base::Passed(&dest_parent_entry)));
}

void MoveOperation::MoveAfterPrepare(
    const MoveParams& params,
    scoped_ptr<ResourceEntry> src_entry,
    scoped_ptr<ResourceEntry> src_parent_entry,
    scoped_ptr<ResourceEntry> dest_parent_entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != FILE_ERROR_OK) {
    params.callback.Run(error);
    return;
  }

  if (!dest_parent_entry->file_info().is_directory()) {
    // The parent of the destination is not a directory.
    params.callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  // Strip the extension for a hosted document if necessary.
  const bool has_hosted_document_extension =
      src_entry->has_file_specific_info() &&
      src_entry->file_specific_info().is_hosted_document() &&
      params.dest_file_path.Extension() ==
      src_entry->file_specific_info().document_extension();
  const std::string new_title =
      has_hosted_document_extension ?
      params.dest_file_path.BaseName().RemoveExtension().AsUTF8Unsafe() :
      params.dest_file_path.BaseName().AsUTF8Unsafe();

  // If Drive API v2 is enabled, we can move the file on server side by one
  // request.
  if (util::IsDriveV2ApiEnabled()) {
    base::Time last_modified =
        params.preserve_last_modified ?
        base::Time::FromInternalValue(src_entry->file_info().last_modified()) :
        base::Time();

    scheduler_->MoveResource(
        src_entry->resource_id(), dest_parent_entry->resource_id(),
        new_title, last_modified,
        base::Bind(&MoveOperation::MoveAfterMoveResource,
                   weak_ptr_factory_.GetWeakPtr(), params));
    return;
  }

  ResourceEntry* src_entry_ptr = src_entry.get();
  Rename(*src_entry_ptr, new_title,
         base::Bind(&MoveOperation::MoveAfterRename,
                    weak_ptr_factory_.GetWeakPtr(),
                    params,
                    base::Passed(&src_entry),
                    base::Passed(&src_parent_entry),
                    base::Passed(&dest_parent_entry)));
}

void MoveOperation::MoveAfterMoveResource(
    const MoveParams& params,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    params.callback.Run(error);
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&RefreshEntry, metadata_, base::Passed(&resource_entry)),
      base::Bind(&MoveOperation::MoveAfterRefreshEntry,
                 weak_ptr_factory_.GetWeakPtr(), params));
}

void MoveOperation::MoveAfterRefreshEntry(const MoveParams& params,
                                          FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error == FILE_ERROR_OK) {
    // Notify the change of directory.
    observer_->OnDirectoryChangedByOperation(params.src_file_path.DirName());
    observer_->OnDirectoryChangedByOperation(params.dest_file_path.DirName());
  }

  params.callback.Run(error);
}

void MoveOperation::MoveAfterRename(const MoveParams& params,
                                    scoped_ptr<ResourceEntry> src_entry,
                                    scoped_ptr<ResourceEntry> src_parent_entry,
                                    scoped_ptr<ResourceEntry> dest_parent_entry,
                                    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != FILE_ERROR_OK) {
    params.callback.Run(error);
    return;
  }

  // The source and the destination directory are the same. Nothing more to do.
  if (src_entry->parent_local_id() == dest_parent_entry->local_id()) {
    observer_->OnDirectoryChangedByOperation(params.dest_file_path.DirName());
    params.callback.Run(FILE_ERROR_OK);
    return;
  }

  const std::string& src_entry_resource_id = src_entry->resource_id();
  const std::string& src_parent_entry_resource_id =
      src_parent_entry->resource_id();
  AddToDirectory(src_entry.Pass(),
                 dest_parent_entry.Pass(),
                 base::Bind(&MoveOperation::MoveAfterAddToDirectory,
                            weak_ptr_factory_.GetWeakPtr(),
                            params,
                            src_entry_resource_id,
                            src_parent_entry_resource_id));
}

void MoveOperation::MoveAfterAddToDirectory(
    const MoveParams& params,
    const std::string& resource_id,
    const std::string& old_parent_resource_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.callback.is_null());

  if (error != FILE_ERROR_OK) {
    params.callback.Run(error);
    return;
  }

  // Notify to the observers.
  observer_->OnDirectoryChangedByOperation(params.src_file_path.DirName());
  observer_->OnDirectoryChangedByOperation(params.dest_file_path.DirName());

  RemoveFromDirectory(resource_id, old_parent_resource_id, params.callback);
}

void MoveOperation::Rename(const ResourceEntry& entry,
                           const std::string& new_title,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!entry.local_id().empty());

  if (entry.title() == new_title) {
    // We have nothing to do.
    callback.Run(FILE_ERROR_OK);
    return;
  }

  // Send a rename request to the server.
  scheduler_->RenameResource(
      entry.resource_id(),
      new_title,
      base::Bind(&MoveOperation::RenameAfterRenameResource,
                 weak_ptr_factory_.GetWeakPtr(),
                 entry.local_id(), new_title, callback));
}

void MoveOperation::RenameAfterRenameResource(
    const std::string& local_id,
    const std::string& new_title,
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  const FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Server side renaming is done. Update the local metadata.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&RenameLocally, metadata_, local_id, new_title),
      callback);
}

void MoveOperation::AddToDirectory(scoped_ptr<ResourceEntry> entry,
                                   scoped_ptr<ResourceEntry> directory,
                                   const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scheduler_->AddResourceToDirectory(
      directory->resource_id(), entry->resource_id(),
      base::Bind(&MoveOperation::AddToDirectoryAfterAddResourceToDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 entry->local_id(), directory->local_id(), callback));
}

void MoveOperation::AddToDirectoryAfterAddResourceToDirectory(
    const std::string& local_id,
    const std::string& parent_local_id,
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  const FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Server side moving is done. Update the local metadata.
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&MoveDirectoryLocally,
                 metadata_, local_id, parent_local_id),
      callback);
}

void MoveOperation::RemoveFromDirectory(
    const std::string& resource_id,
    const std::string& directory_resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Moving files out from "drive/other" special folder for storing orphan
  // files has no meaning in the server. Just skip the step.
  if (util::IsSpecialResourceId(directory_resource_id)) {
    callback.Run(FILE_ERROR_OK);
    return;
  }

  scheduler_->RemoveResourceFromDirectory(
      directory_resource_id,
      resource_id,
      ClientContext(USER_INITIATED),
      base::Bind(
          &MoveOperation::RemoveFromDirectoryAfterRemoveResourceFromDirectory,
          weak_ptr_factory_.GetWeakPtr(), callback));
}

void MoveOperation::RemoveFromDirectoryAfterRemoveResourceFromDirectory(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  callback.Run(GDataToFileError(status));
}

}  // namespace file_system
}  // namespace drive
