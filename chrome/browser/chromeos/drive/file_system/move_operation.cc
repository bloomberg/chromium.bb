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
                      ResourceEntry* dest_parent_entry) {
  FileError error = metadata->GetResourceEntryByPath(src_path, src_entry);
  if (error != FILE_ERROR_OK)
    return error;

  return metadata->GetResourceEntryByPath(dest_parent_path, dest_parent_entry);
}

// Applies renaming to the local metadata.
FileError RenameLocally(internal::ResourceMetadata* metadata,
                        const std::string& resource_id,
                        const std::string& new_title) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryById(resource_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  entry.set_title(new_title);
  return metadata->RefreshEntry(resource_id, entry);
}

// Applies directory-moving to the local metadata.
FileError MoveDirectoryLocally(internal::ResourceMetadata* metadata,
                               const std::string& resource_id,
                               const std::string& parent_resource_id) {
  ResourceEntry entry;
  FileError error = metadata->GetResourceEntryById(resource_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  // TODO(hidehiko,hashimoto): Set local id, instead of resource id.
  entry.set_parent_local_id(parent_resource_id);
  return metadata->RefreshEntry(resource_id, entry);
}

}  // namespace

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
                         const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<ResourceEntry> src_entry(new ResourceEntry);
  scoped_ptr<ResourceEntry> dest_parent_entry(new ResourceEntry);
  ResourceEntry* src_entry_ptr = src_entry.get();
  ResourceEntry* dest_parent_entry_ptr = dest_parent_entry.get();
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&PrepareMove,
                 metadata_, src_file_path, dest_file_path.DirName(),
                 src_entry_ptr, dest_parent_entry_ptr),
      base::Bind(&MoveOperation::MoveAfterPrepare,
                 weak_ptr_factory_.GetWeakPtr(),
                 src_file_path, dest_file_path, callback,
                 base::Passed(&src_entry),
                 base::Passed(&dest_parent_entry)));
}

void MoveOperation::MoveAfterPrepare(
    const base::FilePath& src_file_path,
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<ResourceEntry> src_entry,
    scoped_ptr<ResourceEntry> dest_parent_entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  if (!dest_parent_entry->file_info().is_directory()) {
    // The parent of the destination is not a directory.
    callback.Run(FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  // Strip the extension for a hosted document if necessary.
  const bool has_hosted_document_extension =
      src_entry->has_file_specific_info() &&
      src_entry->file_specific_info().is_hosted_document() &&
      dest_file_path.Extension() ==
      src_entry->file_specific_info().document_extension();
  const std::string new_title =
      has_hosted_document_extension ?
      dest_file_path.BaseName().RemoveExtension().AsUTF8Unsafe() :
      dest_file_path.BaseName().AsUTF8Unsafe();

  // If Drive API v2 is enabled, we can move the file on server side by one
  // request.
  if (util::IsDriveV2ApiEnabled()) {
    scheduler_->MoveResource(
        src_entry->resource_id(), dest_parent_entry->resource_id(), new_title,
        base::Bind(&MoveOperation::MoveAfterMoveResource,
                   weak_ptr_factory_.GetWeakPtr(),
                   src_file_path, dest_file_path, callback));
    return;
  }

  ResourceEntry* src_entry_ptr = src_entry.get();
  Rename(*src_entry_ptr, new_title,
         base::Bind(&MoveOperation::MoveAfterRename,
                    weak_ptr_factory_.GetWeakPtr(),
                    src_file_path, dest_file_path, callback,
                    base::Passed(&src_entry),
                    base::Passed(&dest_parent_entry)));
}

void MoveOperation::MoveAfterMoveResource(
    const base::FilePath& src_file_path,
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id)) {
    callback.Run(FILE_ERROR_FAILED);
    return;
  }

  // TODO(hashimoto): Resolve local ID before use. crbug.com/260514
  entry.set_parent_local_id(parent_resource_id);

  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&internal::ResourceMetadata::RefreshEntry,
                 base::Unretained(metadata_), entry.resource_id(), entry),
      base::Bind(&MoveOperation::MoveAfterRefreshEntry,
                 weak_ptr_factory_.GetWeakPtr(),
                 src_file_path, dest_file_path, callback));
}

void MoveOperation::MoveAfterRefreshEntry(
    const base::FilePath& src_file_path,
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error == FILE_ERROR_OK) {
    // Notify the change of directory.
    observer_->OnDirectoryChangedByOperation(src_file_path.DirName());
    observer_->OnDirectoryChangedByOperation(dest_file_path.DirName());
  }

  callback.Run(error);
}

void MoveOperation::MoveAfterRename(
    const base::FilePath& src_file_path,
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<ResourceEntry> src_entry,
    scoped_ptr<ResourceEntry> dest_parent_entry,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // The source and the destination directory are the same. Nothing more to do.
  // TODO(hidehiko,hashimoto): Replace resource_id to local_id.
  if (src_entry->parent_local_id() == dest_parent_entry->resource_id()) {
    observer_->OnDirectoryChangedByOperation(dest_file_path.DirName());
    callback.Run(FILE_ERROR_OK);
    return;
  }

  // TODO(hidehiko,hashimoto): For MoveAfterAddToDirectory, it will be
  // necessary to resolve local id to resource id.
  AddToDirectory(src_entry->resource_id(),
                 dest_parent_entry->resource_id(),
                 base::Bind(&MoveOperation::MoveAfterAddToDirectory,
                            weak_ptr_factory_.GetWeakPtr(),
                            src_file_path, dest_file_path, callback,
                            src_entry->resource_id(),
                            src_entry->parent_local_id()));
}

void MoveOperation::MoveAfterAddToDirectory(
    const base::FilePath& src_file_path,
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    const std::string& resource_id,
    const std::string& parent_resource_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // Notify to the observers.
  observer_->OnDirectoryChangedByOperation(src_file_path.DirName());
  observer_->OnDirectoryChangedByOperation(dest_file_path.DirName());

  RemoveFromDirectory(resource_id, parent_resource_id, callback);
}

void MoveOperation::Rename(const ResourceEntry& entry,
                           const std::string& new_title,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

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
                 entry.resource_id(), new_title, callback));
}

void MoveOperation::RenameAfterRenameResource(
    const std::string& resource_id,
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
      base::Bind(&RenameLocally, metadata_, resource_id, new_title),
      callback);
}

void MoveOperation::AddToDirectory(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scheduler_->AddResourceToDirectory(
      parent_resource_id, resource_id,
      base::Bind(&MoveOperation::AddToDirectoryAfterAddResourceToDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id, parent_resource_id, callback));
}

void MoveOperation::AddToDirectoryAfterAddResourceToDirectory(
    const std::string& resource_id,
    const std::string& parent_resource_id,
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
                 metadata_, resource_id, parent_resource_id),
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
