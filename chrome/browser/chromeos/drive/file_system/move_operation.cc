// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/move_operation.h"

#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_scheduler.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

MoveOperation::MoveOperation(DriveScheduler* drive_scheduler,
                             DriveResourceMetadata* metadata,
                             OperationObserver* observer)
  : drive_scheduler_(drive_scheduler),
    metadata_(metadata),
    observer_(observer),
    weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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

  metadata_->GetEntryInfoPairByPaths(
      src_file_path,
      dest_file_path.DirName(),
      base::Bind(&MoveOperation::MoveAfterGetEntryInfoPair,
                 weak_ptr_factory_.GetWeakPtr(),
                 dest_file_path,
                 callback));
}

void MoveOperation::MoveAfterGetEntryInfoPair(
    const base::FilePath& dest_file_path,
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> src_dest_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(src_dest_info.get());

  if (src_dest_info->first.error != DRIVE_FILE_OK) {
    callback.Run(src_dest_info->first.error);
    return;
  }
  if (src_dest_info->second.error != DRIVE_FILE_OK) {
    callback.Run(src_dest_info->second.error);
    return;
  }
  if (!src_dest_info->second.proto->file_info().is_directory()) {
    callback.Run(DRIVE_FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  const std::string& src_id = src_dest_info->first.proto->resource_id();
  const base::FilePath& src_path = src_dest_info->first.path;
  const base::FilePath new_name = dest_file_path.BaseName();
  const bool new_name_has_hosted_extension =
      src_dest_info->first.proto->has_file_specific_info() &&
      src_dest_info->first.proto->file_specific_info().is_hosted_document() &&
      new_name.Extension() ==
          src_dest_info->first.proto->file_specific_info().document_extension();

  Rename(src_id, src_path, new_name, new_name_has_hosted_extension,
         base::Bind(&MoveOperation::MoveAfterRename,
                    weak_ptr_factory_.GetWeakPtr(),
                    callback,
                    base::Passed(&src_dest_info)));
}

void MoveOperation::MoveAfterRename(
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> src_dest_info,
    DriveFileError error,
    const base::FilePath& src_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  const std::string& src_id = src_dest_info->first.proto->resource_id();
  const std::string& dest_dir_id = src_dest_info->second.proto->resource_id();
  const base::FilePath& dest_dir_path = src_dest_info->second.path;

  // The source and the destination directory are the same. Nothing more to do.
  if (src_path.DirName() == dest_dir_path) {
    observer_->OnDirectoryChangedByOperation(dest_dir_path);
    callback.Run(DRIVE_FILE_OK);
    return;
  }

  AddToDirectory(src_id, dest_dir_id, src_path, dest_dir_path,
                 base::Bind(&MoveOperation::MoveAfterAddToDirectory,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback,
                            base::Passed(&src_dest_info)));
}

void MoveOperation::MoveAfterAddToDirectory(
    const FileOperationCallback& callback,
    scoped_ptr<EntryInfoPairResult> src_dest_info,
    DriveFileError error,
    const base::FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK) {
    callback.Run(error);
    return;
  }

  const base::FilePath& src_path = src_dest_info->first.path;
  observer_->OnDirectoryChangedByOperation(src_path.DirName());
  observer_->OnDirectoryChangedByOperation(new_path.DirName());

  RemoveFromDirectory(src_dest_info->first.proto->resource_id(),
                      src_dest_info->first.proto->parent_resource_id(),
                      callback);
}

void MoveOperation::Rename(const std::string& src_id,
                           const base::FilePath& src_path,
                           const base::FilePath& new_name,
                           bool new_name_has_hosted_extension,
                           const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // It is a no-op if the file is renamed to the same name.
  if (src_path.BaseName() == new_name) {
    callback.Run(DRIVE_FILE_OK, src_path);
    return;
  }

  // Drop the .g<something> extension from |new_name| if the file being
  // renamed is a hosted document and |new_name| has the same .g<something>
  // extension as the file.
  const base::FilePath& new_name_arg(
      new_name_has_hosted_extension ? new_name.RemoveExtension() : new_name);

  // Rename on the server.
  drive_scheduler_->RenameResource(src_id,
                                   new_name_arg.AsUTF8Unsafe(),
                                   base::Bind(&MoveOperation::RenameLocally,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              src_path,
                                              new_name_arg,
                                              callback));
}

void MoveOperation::RenameLocally(const base::FilePath& src_path,
                                  const base::FilePath& new_name,
                                  const FileMoveCallback& callback,
                                  google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error, base::FilePath());
    return;
  }
  metadata_->RenameEntry(src_path, new_name.value(), callback);
}


void MoveOperation::AddToDirectory(const std::string& src_id,
                                   const std::string& dest_dir_id,
                                   const base::FilePath& src_path,
                                   const base::FilePath& dest_dir_path,
                                   const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_scheduler_->AddResourceToDirectory(
      dest_dir_id, src_id,
      base::Bind(&MoveOperation::AddToDirectoryLocally,
                 weak_ptr_factory_.GetWeakPtr(),
                 src_path,
                 dest_dir_path,
                 callback));
}

void MoveOperation::AddToDirectoryLocally(const base::FilePath& src_path,
                                          const base::FilePath& dest_dir_path,
                                          const FileMoveCallback& callback,
                                          google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const DriveFileError error = util::GDataToDriveFileError(status);
  if (error != DRIVE_FILE_OK) {
    callback.Run(error, base::FilePath());
    return;
  }
  metadata_->MoveEntryToDirectory(src_path, dest_dir_path, callback);
}

void MoveOperation::RemoveFromDirectory(
    const std::string& resource_id,
    const std::string& directory_resource_id,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive_scheduler_->RemoveResourceFromDirectory(
      directory_resource_id,
      resource_id,
      base::Bind(&MoveOperation::RemoveFromDirectoryCompleted,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void MoveOperation::RemoveFromDirectoryCompleted(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(util::GDataToDriveFileError(status));
}

}  // namespace file_system
}  // namespace drive
