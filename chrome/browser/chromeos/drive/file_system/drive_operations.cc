// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/drive_operations.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"
#include "chrome/browser/chromeos/drive/file_system/search_operation.h"
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

void DriveOperations::Init(OperationObserver* observer,
                           JobScheduler* scheduler,
                           internal::ResourceMetadata* metadata,
                           internal::FileCache* cache,
                           FileSystemInterface* file_system,
                           base::SequencedTaskRunner* blocking_task_runner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  copy_operation_.reset(new file_system::CopyOperation(blocking_task_runner,
                                                       observer,
                                                       scheduler,
                                                       metadata,
                                                       cache,
                                                       file_system));
  create_directory_operation_.reset(
      new CreateDirectoryOperation(observer, scheduler, metadata));
  create_file_operation_.reset(new CreateFileOperation(blocking_task_runner,
                                                       observer,
                                                       scheduler,
                                                       metadata,
                                                       cache));
  move_operation_.reset(
      new MoveOperation(observer, scheduler, metadata));
  remove_operation_.reset(
      new RemoveOperation(observer, scheduler, metadata, cache));
  update_operation_.reset(
      new UpdateOperation(observer, scheduler, metadata, cache));
  search_operation_.reset(
      new SearchOperation(blocking_task_runner, scheduler, metadata));
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

void DriveOperations::CreateDirectory(const base::FilePath& directory_path,
                                      bool is_exclusive,
                                      bool is_recursive,
                                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  create_directory_operation_->CreateDirectory(
      directory_path, is_exclusive, is_recursive, callback);
}

void DriveOperations::CreateFile(const base::FilePath& remote_file_path,
                                 bool is_exclusive,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  create_file_operation_->CreateFile(remote_file_path, is_exclusive, callback);
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

void DriveOperations::Search(const std::string& search_query,
                             const GURL& next_feed,
                             const SearchOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  search_operation_->Search(search_query, next_feed, callback);
}

}  // namespace file_system
}  // namespace drive
