// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/mtp_device_delegate_impl_linux.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media_galleries/linux/mtp_device_task_helper.h"
#include "chrome/browser/media_galleries/linux/mtp_device_task_helper_map_service.h"
#include "chrome/browser/media_galleries/linux/snapshot_file_details.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"

namespace {

// File path separator constant.
const char kRootPath[] = "/";

// Returns the device relative file path given |file_path|.
// E.g.: If the |file_path| is "/usb:2,2:12345/DCIM" and |registered_dev_path|
// is "/usb:2,2:12345", this function returns the device relative path which is
// "DCIM".
// In the special case when |registered_dev_path| and |file_path| are the same,
// return |kRootPath|.
std::string GetDeviceRelativePath(const base::FilePath& registered_dev_path,
                                  const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!registered_dev_path.empty());
  DCHECK(!file_path.empty());
  std::string result;
  if (registered_dev_path == file_path) {
    result = kRootPath;
  } else {
    base::FilePath relative_path;
    if (registered_dev_path.AppendRelativePath(file_path, &relative_path)) {
      DCHECK(!relative_path.empty());
      result = relative_path.value();
    }
  }
  return result;
}

// Returns the MTPDeviceTaskHelper object associated with the MTP device
// storage.
//
// |storage_name| specifies the name of the storage device.
// Returns NULL if the |storage_name| is no longer valid (e.g. because the
// corresponding storage device is detached, etc).
MTPDeviceTaskHelper* GetDeviceTaskHelperForStorage(
    const std::string& storage_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return MTPDeviceTaskHelperMapService::GetInstance()->GetDeviceTaskHelper(
      storage_name);
}

// Opens the storage device for communication.
//
// Called on the UI thread to dispatch the request to the
// MediaTransferProtocolManager.
//
// |storage_name| specifies the name of the storage device.
// |reply_callback| is called when the OpenStorage request completes.
// |reply_callback| runs on the IO thread.
void OpenStorageOnUIThread(
    const std::string& storage_name,
    const base::Callback<void(bool)>& reply_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper) {
    task_helper =
        MTPDeviceTaskHelperMapService::GetInstance()->CreateDeviceTaskHelper(
            storage_name);
  }
  task_helper->OpenStorage(storage_name, reply_callback);
}

// Enumerates the |root| directory file entries.
//
// Called on the UI thread to dispatch the request to the
// MediaTransferProtocolManager.
//
// |storage_name| specifies the name of the storage device.
// |success_callback| is called when the ReadDirectory request succeeds.
// |error_callback| is called when the ReadDirectory request fails.
// |success_callback| and |error_callback| runs on the IO thread.
void ReadDirectoryOnUIThread(
    const std::string& storage_name,
    const std::string& root,
    const base::Callback<
        void(const fileapi::AsyncFileUtil::EntryList&)>& success_callback,
    const base::Callback<void(base::File::Error)>& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper)
    return;
  task_helper->ReadDirectoryByPath(root, success_callback, error_callback);
}

// Gets the |file_path| details.
//
// Called on the UI thread to dispatch the request to the
// MediaTransferProtocolManager.
//
// |storage_name| specifies the name of the storage device.
// |success_callback| is called when the GetFileInfo request succeeds.
// |error_callback| is called when the GetFileInfo request fails.
// |success_callback| and |error_callback| runs on the IO thread.
void GetFileInfoOnUIThread(
    const std::string& storage_name,
    const std::string& file_path,
    const base::Callback<void(const base::File::Info&)>& success_callback,
    const base::Callback<void(base::File::Error)>& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper)
    return;
  task_helper->GetFileInfoByPath(file_path, success_callback, error_callback);
}

// Copies the contents of |device_file_path| to |snapshot_file_path|.
//
// Called on the UI thread to dispatch the request to the
// MediaTransferProtocolManager.
//
// |storage_name| specifies the name of the storage device.
// |device_file_path| specifies the media device file path.
// |snapshot_file_path| specifies the platform path of the snapshot file.
// |file_size| specifies the number of bytes that will be written to the
// snapshot file.
// |success_callback| is called when the copy operation succeeds.
// |error_callback| is called when the copy operation fails.
// |success_callback| and |error_callback| runs on the IO thread.
void WriteDataIntoSnapshotFileOnUIThread(
    const std::string& storage_name,
    const SnapshotRequestInfo& request_info,
    const base::File::Info& snapshot_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper)
    return;
  task_helper->WriteDataIntoSnapshotFile(request_info, snapshot_file_info);
}

// Copies the contents of |device_file_path| to |snapshot_file_path|.
//
// Called on the UI thread to dispatch the request to the
// MediaTransferProtocolManager.
//
// |storage_name| specifies the name of the storage device.
// |request| is a struct containing details about the byte read request.
void ReadBytesOnUIThread(
    const std::string& storage_name,
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper)
    return;
  task_helper->ReadBytes(request);
}

// Closes the device storage specified by the |storage_name| and destroys the
// MTPDeviceTaskHelper object associated with the device storage.
//
// Called on the UI thread to dispatch the request to the
// MediaTransferProtocolManager.
void CloseStorageAndDestroyTaskHelperOnUIThread(
    const std::string& storage_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper)
    return;
  task_helper->CloseStorage();
  MTPDeviceTaskHelperMapService::GetInstance()->DestroyDeviceTaskHelper(
      storage_name);
}

}  // namespace

MTPDeviceDelegateImplLinux::PendingTaskInfo::PendingTaskInfo(
    const tracked_objects::Location& location,
    const base::Closure& task)
    : location(location),
      task(task) {
}

MTPDeviceDelegateImplLinux::PendingTaskInfo::~PendingTaskInfo() {
}

MTPDeviceDelegateImplLinux::MTPDeviceDelegateImplLinux(
    const std::string& device_location)
    : init_state_(UNINITIALIZED),
      task_in_progress_(false),
      device_path_(device_location),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_path_.empty());
  base::RemoveChars(device_location, kRootPath, &storage_name_);
  DCHECK(!storage_name_.empty());
}

MTPDeviceDelegateImplLinux::~MTPDeviceDelegateImplLinux() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void MTPDeviceDelegateImplLinux::GetFileInfo(
    const base::FilePath& file_path,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());
  base::Closure call_closure =
      base::Bind(&GetFileInfoOnUIThread,
                 storage_name_,
                 GetDeviceRelativePath(device_path_, file_path),
                 base::Bind(&MTPDeviceDelegateImplLinux::OnDidGetFileInfo,
                            weak_ptr_factory_.GetWeakPtr(),
                            success_callback),
                 base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                            weak_ptr_factory_.GetWeakPtr(),
                            error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(FROM_HERE, call_closure));
}

void MTPDeviceDelegateImplLinux::ReadDirectory(
    const base::FilePath& root,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!root.empty());
  std::string device_file_relative_path = GetDeviceRelativePath(device_path_,
                                                                root);
  base::Closure call_closure =
      base::Bind(
          &GetFileInfoOnUIThread,
          storage_name_,
          device_file_relative_path,
          base::Bind(
              &MTPDeviceDelegateImplLinux::OnDidGetFileInfoToReadDirectory,
              weak_ptr_factory_.GetWeakPtr(),
              device_file_relative_path,
              success_callback,
              error_callback),
          base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                     weak_ptr_factory_.GetWeakPtr(),
                     error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(FROM_HERE, call_closure));
}

void MTPDeviceDelegateImplLinux::CreateSnapshotFile(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    const CreateSnapshotFileSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  DCHECK(!local_path.empty());
  std::string device_file_relative_path =
      GetDeviceRelativePath(device_path_, device_file_path);
  scoped_ptr<SnapshotRequestInfo> request_info(
      new SnapshotRequestInfo(device_file_relative_path,
                              local_path,
                              success_callback,
                              error_callback));
  base::Closure call_closure =
      base::Bind(
          &GetFileInfoOnUIThread,
          storage_name_,
          device_file_relative_path,
          base::Bind(
              &MTPDeviceDelegateImplLinux::OnDidGetFileInfoToCreateSnapshotFile,
              weak_ptr_factory_.GetWeakPtr(),
              base::Passed(&request_info)),
          base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                     weak_ptr_factory_.GetWeakPtr(),
                     error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(FROM_HERE, call_closure));
}

bool MTPDeviceDelegateImplLinux::IsStreaming() {
  return true;
}

void MTPDeviceDelegateImplLinux::ReadBytes(
    const base::FilePath& device_file_path,
    net::IOBuffer* buf, int64 offset, int buf_len,
    const ReadBytesSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  std::string device_file_relative_path =
      GetDeviceRelativePath(device_path_, device_file_path);

  ReadBytesRequest request(
      device_file_relative_path, buf, offset, buf_len,
      base::Bind(&MTPDeviceDelegateImplLinux::OnDidReadBytes,
                 weak_ptr_factory_.GetWeakPtr(), success_callback),
      base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback));

  base::Closure call_closure =
      base::Bind(base::Bind(&ReadBytesOnUIThread, storage_name_, request));

  EnsureInitAndRunTask(PendingTaskInfo(FROM_HERE, call_closure));
}

void MTPDeviceDelegateImplLinux::CancelPendingTasksAndDeleteDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // To cancel all the pending tasks, destroy the MTPDeviceTaskHelper object.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CloseStorageAndDestroyTaskHelperOnUIThread, storage_name_));
  delete this;
}

void MTPDeviceDelegateImplLinux::EnsureInitAndRunTask(
    const PendingTaskInfo& task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if ((init_state_ == INITIALIZED) && !task_in_progress_) {
    task_in_progress_ = true;
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     task_info.location,
                                     task_info.task);
    return;
  }
  pending_tasks_.push(task_info);
  if (init_state_ == UNINITIALIZED) {
    init_state_ = PENDING_INIT;
    task_in_progress_ = true;
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&OpenStorageOnUIThread,
                   storage_name_,
                   base::Bind(&MTPDeviceDelegateImplLinux::OnInitCompleted,
                              weak_ptr_factory_.GetWeakPtr())));
  }
}

void MTPDeviceDelegateImplLinux::WriteDataIntoSnapshotFile(
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  DCHECK_GT(file_info.size, 0);
  DCHECK(task_in_progress_);
  SnapshotRequestInfo request_info(
      current_snapshot_request_info_->device_file_path,
      current_snapshot_request_info_->snapshot_file_path,
      base::Bind(
          &MTPDeviceDelegateImplLinux::OnDidWriteDataIntoSnapshotFile,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &MTPDeviceDelegateImplLinux::OnWriteDataIntoSnapshotFileError,
          weak_ptr_factory_.GetWeakPtr()));

  base::Closure task_closure = base::Bind(&WriteDataIntoSnapshotFileOnUIThread,
                                          storage_name_,
                                          request_info,
                                          file_info);
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   task_closure);
}

void MTPDeviceDelegateImplLinux::PendingRequestDone() {
  DCHECK(task_in_progress_);
  task_in_progress_ = false;
  ProcessNextPendingRequest();
}

void MTPDeviceDelegateImplLinux::ProcessNextPendingRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!task_in_progress_);
  if (pending_tasks_.empty())
    return;

  task_in_progress_ = true;
  const PendingTaskInfo& task_info = pending_tasks_.front();
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   task_info.location,
                                   task_info.task);
  pending_tasks_.pop();
}

void MTPDeviceDelegateImplLinux::OnInitCompleted(bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  init_state_ = succeeded ? INITIALIZED : UNINITIALIZED;
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidGetFileInfo(
    const GetFileInfoSuccessCallback& success_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  success_callback.Run(file_info);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidGetFileInfoToReadDirectory(
    const std::string& root,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);
  if (!file_info.is_directory) {
    return HandleDeviceFileError(error_callback,
                                 base::File::FILE_ERROR_NOT_A_DIRECTORY);
  }

  base::Closure task_closure =
      base::Bind(&ReadDirectoryOnUIThread,
                 storage_name_,
                 root,
                 base::Bind(&MTPDeviceDelegateImplLinux::OnDidReadDirectory,
                            weak_ptr_factory_.GetWeakPtr(),
                            success_callback),
                 base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                            weak_ptr_factory_.GetWeakPtr(),
                            error_callback));
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   task_closure);
}

void MTPDeviceDelegateImplLinux::OnDidGetFileInfoToCreateSnapshotFile(
    scoped_ptr<SnapshotRequestInfo> snapshot_request_info,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!current_snapshot_request_info_.get());
  DCHECK(snapshot_request_info.get());
  DCHECK(task_in_progress_);
  base::File::Error error = base::File::FILE_OK;
  if (file_info.is_directory)
    error = base::File::FILE_ERROR_NOT_A_FILE;
  else if (file_info.size < 0 || file_info.size > kuint32max)
    error = base::File::FILE_ERROR_FAILED;

  if (error != base::File::FILE_OK)
    return HandleDeviceFileError(snapshot_request_info->error_callback, error);

  base::File::Info snapshot_file_info(file_info);
  // Modify the last modified time to null. This prevents the time stamp
  // verfication in LocalFileStreamReader.
  snapshot_file_info.last_modified = base::Time();

  current_snapshot_request_info_.reset(snapshot_request_info.release());
  if (file_info.size == 0) {
    // Empty snapshot file.
    return OnDidWriteDataIntoSnapshotFile(
        snapshot_file_info, current_snapshot_request_info_->snapshot_file_path);
  }
  WriteDataIntoSnapshotFile(snapshot_file_info);
}

void MTPDeviceDelegateImplLinux::OnDidReadDirectory(
    const ReadDirectorySuccessCallback& success_callback,
    const fileapi::AsyncFileUtil::EntryList& file_list) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  success_callback.Run(file_list, false /*no more entries*/);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidWriteDataIntoSnapshotFile(
    const base::File::Info& file_info,
    const base::FilePath& snapshot_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  current_snapshot_request_info_->success_callback.Run(
      file_info, snapshot_file_path);
  current_snapshot_request_info_.reset();
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnWriteDataIntoSnapshotFileError(
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  current_snapshot_request_info_->error_callback.Run(error);
  current_snapshot_request_info_.reset();
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidReadBytes(
    const ReadBytesSuccessCallback& success_callback,
    const base::File::Info& file_info, int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  success_callback.Run(file_info, bytes_read);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::HandleDeviceFileError(
    const ErrorCallback& error_callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  error_callback.Run(error);
  PendingRequestDone();
}

void CreateMTPDeviceAsyncDelegate(
    const std::string& device_location,
    const CreateMTPDeviceAsyncDelegateCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(new MTPDeviceDelegateImplLinux(device_location));
}
