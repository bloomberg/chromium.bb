// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/drive_operations.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"
#include "chrome/browser/chromeos/drive/file_system/update_operation.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

DriveOperations::DriveOperations() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

DriveOperations::~DriveOperations() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DriveOperations::Init(
    DriveScheduler* drive_scheduler,
    DriveFileSystemInterface* drive_file_system,
    DriveCache* cache,
    DriveResourceMetadata* metadata,
    google_apis::DriveUploaderInterface* uploader,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OperationObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  copy_operation_.reset(new file_system::CopyOperation(drive_scheduler,
                                                       drive_file_system,
                                                       metadata,
                                                       uploader,
                                                       blocking_task_runner,
                                                       observer));
  create_directory_operation_.reset(
      new CreateDirectoryOperation(drive_scheduler,
                                   metadata,
                                   observer));
  move_operation_.reset(new file_system::MoveOperation(drive_scheduler,
                                                       metadata,
                                                       observer));
  remove_operation_.reset(new file_system::RemoveOperation(drive_scheduler,
                                                           cache,
                                                           metadata,
                                                           observer));
  update_operation_.reset(new file_system::UpdateOperation(cache,
                                                           metadata,
                                                           drive_scheduler,
                                                           blocking_task_runner,
                                                           observer));
}

void DriveOperations::InitForTesting(CopyOperation* copy_operation,
                                     MoveOperation* move_operation,
                                     RemoveOperation* remove_operation,
                                     UpdateOperation* update_operation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  copy_operation_.reset(copy_operation);
  move_operation_.reset(move_operation);
  remove_operation_.reset(remove_operation);
  update_operation_.reset(update_operation);
}

void DriveOperations::Copy(const base::FilePath& src_file_path,
                           const base::FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  copy_operation_->Copy(src_file_path, dest_file_path, callback);
}

void DriveOperations::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  copy_operation_->TransferFileFromRemoteToLocal(remote_src_file_path,
                                                 local_dest_file_path,
                                                 callback);
}

void DriveOperations::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  copy_operation_->TransferFileFromLocalToRemote(local_src_file_path,
                                                 remote_dest_file_path,
                                                 callback);
}

void DriveOperations::TransferRegularFile(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  copy_operation_->TransferRegularFile(local_src_file_path,
                                       remote_dest_file_path,
                                       callback);
}

void DriveOperations::CreateDirectory(
    const base::FilePath& directory_path,
    bool is_exclusive,
    bool is_recursive,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  create_directory_operation_->CreateDirectory(
      directory_path, is_exclusive, is_recursive, callback);
}

void DriveOperations::Move(const base::FilePath& src_file_path,
                           const base::FilePath& dest_file_path,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  move_operation_->Move(src_file_path, dest_file_path, callback);
}

void DriveOperations::Remove(const base::FilePath& file_path,
                             bool is_recursive,
                             const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  remove_operation_->Remove(file_path, is_recursive, callback);
}

void DriveOperations::UpdateFileByResourceId(
    const std::string& resource_id,
    DriveClientContext context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  update_operation_->UpdateFileByResourceId(resource_id, context, callback);
}

}  // namespace file_system
}  // namespace drive
