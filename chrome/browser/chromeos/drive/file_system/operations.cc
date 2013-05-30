// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/operations.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_directory_operation.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/download_operation.h"
#include "chrome/browser/chromeos/drive/file_system/move_operation.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"
#include "chrome/browser/chromeos/drive/file_system/search_operation.h"
#include "chrome/browser/chromeos/drive/file_system/touch_operation.h"
#include "chrome/browser/chromeos/drive/file_system/update_operation.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

Operations::Operations() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

Operations::~Operations() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void Operations::Init(OperationObserver* observer,
                      JobScheduler* scheduler,
                      internal::ResourceMetadata* metadata,
                      internal::FileCache* cache,
                      FileSystemInterface* file_system,
                      google_apis::DriveServiceInterface* drive_service,
                      base::SequencedTaskRunner* blocking_task_runner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  copy_operation_.reset(new file_system::CopyOperation(blocking_task_runner,
                                                       observer,
                                                       scheduler,
                                                       metadata,
                                                       cache,
                                                       file_system,
                                                       drive_service));
  create_directory_operation_.reset(new CreateDirectoryOperation(
      blocking_task_runner,
      observer,
      scheduler,
      metadata));
  create_file_operation_.reset(new CreateFileOperation(blocking_task_runner,
                                                       observer,
                                                       scheduler,
                                                       metadata,
                                                       cache));
  move_operation_.reset(
      new MoveOperation(observer, scheduler, metadata));
  remove_operation_.reset(
      new RemoveOperation(observer, scheduler, metadata, cache));
  touch_operation_.reset(
      new TouchOperation(blocking_task_runner, observer, scheduler, metadata));
  download_operation_.reset(new DownloadOperation(
      blocking_task_runner, observer, scheduler, metadata, cache));
  update_operation_.reset(new UpdateOperation(blocking_task_runner,
                                              observer,
                                              scheduler,
                                              metadata,
                                              cache));
  search_operation_.reset(
      new SearchOperation(blocking_task_runner, scheduler, metadata));
}

void Operations::Copy(const base::FilePath& src_file_path,
                      const base::FilePath& dest_file_path,
                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  copy_operation_->Copy(src_file_path, dest_file_path, callback);
}

void Operations::TransferFileFromRemoteToLocal(
    const base::FilePath& remote_src_file_path,
    const base::FilePath& local_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  copy_operation_->TransferFileFromRemoteToLocal(remote_src_file_path,
                                                 local_dest_file_path,
                                                 callback);
}

void Operations::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_file_path,
    const base::FilePath& remote_dest_file_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  copy_operation_->TransferFileFromLocalToRemote(local_src_file_path,
                                                 remote_dest_file_path,
                                                 callback);
}

void Operations::CreateDirectory(const base::FilePath& directory_path,
                                 bool is_exclusive,
                                 bool is_recursive,
                                 const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  create_directory_operation_->CreateDirectory(
      directory_path, is_exclusive, is_recursive, callback);
}

void Operations::CreateFile(const base::FilePath& remote_file_path,
                            bool is_exclusive,
                            const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  create_file_operation_->CreateFile(remote_file_path, is_exclusive, callback);
}

void Operations::Move(const base::FilePath& src_file_path,
                      const base::FilePath& dest_file_path,
                      const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  move_operation_->Move(src_file_path, dest_file_path, callback);
}

void Operations::Remove(const base::FilePath& file_path,
                        bool is_recursive,
                        const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  remove_operation_->Remove(file_path, is_recursive, callback);
}

void Operations::TouchFile(const base::FilePath& file_path,
                           const base::Time& last_access_time,
                           const base::Time& last_modified_time,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!last_access_time.is_null());
  DCHECK(!last_modified_time.is_null());
  DCHECK(!callback.is_null());

  touch_operation_->TouchFile(
      file_path, last_access_time, last_modified_time, callback);
}

void Operations::EnsureFileDownloadedByResourceId(
    const std::string& resource_id,
    const ClientContext& context,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const GetFileCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!completion_callback.is_null());

  download_operation_->EnsureFileDownloadedByResourceId(
      resource_id, context, initialized_callback, get_content_callback,
      completion_callback);
}

void Operations::EnsureFileDownloadedByPath(
    const base::FilePath& file_path,
    const ClientContext& context,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const GetFileCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!completion_callback.is_null());

  download_operation_->EnsureFileDownloadedByPath(
      file_path, context, initialized_callback, get_content_callback,
      completion_callback);
}

void Operations::UpdateFileByResourceId(
    const std::string& resource_id,
    const ClientContext& context,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  update_operation_->UpdateFileByResourceId(resource_id, context, callback);
}

void Operations::Search(const std::string& search_query,
                        const GURL& next_feed,
                        const SearchOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  search_operation_->Search(search_query, next_feed, callback);
}

}  // namespace file_system
}  // namespace drive
