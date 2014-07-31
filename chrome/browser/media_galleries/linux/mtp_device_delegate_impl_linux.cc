// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/mtp_device_delegate_impl_linux.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media_galleries/linux/mtp_device_task_helper.h"
#include "chrome/browser/media_galleries/linux/mtp_device_task_helper_map_service.h"
#include "chrome/browser/media_galleries/linux/snapshot_file_details.h"
#include "net/base/io_buffer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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
    const MTPDeviceTaskHelper::OpenStorageCallback& reply_callback) {
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

// Enumerates the |dir_id| directory file entries.
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
    uint32 dir_id,
    const MTPDeviceTaskHelper::ReadDirectorySuccessCallback& success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper)
    return;
  task_helper->ReadDirectoryById(dir_id, success_callback, error_callback);
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
    uint32 file_id,
    const MTPDeviceTaskHelper::GetFileInfoSuccessCallback& success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name);
  if (!task_helper)
    return;
  task_helper->GetFileInfoById(file_id, success_callback, error_callback);
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
    const base::FilePath& path,
    content::BrowserThread::ID thread_id,
    const tracked_objects::Location& location,
    const base::Closure& task)
    : path(path),
      thread_id(thread_id),
      location(location),
      task(task) {
}

MTPDeviceDelegateImplLinux::PendingTaskInfo::~PendingTaskInfo() {
}

// Represents a file on the MTP device.
// Lives on the IO thread.
class MTPDeviceDelegateImplLinux::MTPFileNode {
 public:
  MTPFileNode(uint32 file_id,
              MTPFileNode* parent,
              FileIdToMTPFileNodeMap* file_id_to_node_map);
  ~MTPFileNode();

  const MTPFileNode* GetChild(const std::string& name) const;

  void EnsureChildExists(const std::string& name, uint32 id);

  // Clears all the children, except those in |children_to_keep|.
  void ClearNonexistentChildren(
      const std::set<std::string>& children_to_keep);

  bool DeleteChild(uint32 file_id);

  uint32 file_id() const { return file_id_; }
  MTPFileNode* parent() { return parent_; }

 private:
  // Container for holding a node's children.
  typedef base::ScopedPtrHashMap<std::string, MTPFileNode> ChildNodes;

  const uint32 file_id_;
  ChildNodes children_;
  MTPFileNode* const parent_;
  FileIdToMTPFileNodeMap* file_id_to_node_map_;

  DISALLOW_COPY_AND_ASSIGN(MTPFileNode);
};

MTPDeviceDelegateImplLinux::MTPFileNode::MTPFileNode(
    uint32 file_id,
    MTPFileNode* parent,
    FileIdToMTPFileNodeMap* file_id_to_node_map)
    : file_id_(file_id),
      parent_(parent),
      file_id_to_node_map_(file_id_to_node_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_id_to_node_map_);
  DCHECK(!ContainsKey(*file_id_to_node_map_, file_id_));
  (*file_id_to_node_map_)[file_id_] = this;
}

MTPDeviceDelegateImplLinux::MTPFileNode::~MTPFileNode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  size_t erased = file_id_to_node_map_->erase(file_id_);
  DCHECK_EQ(1U, erased);
}

const MTPDeviceDelegateImplLinux::MTPFileNode*
MTPDeviceDelegateImplLinux::MTPFileNode::GetChild(
    const std::string& name) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return children_.get(name);
}

void MTPDeviceDelegateImplLinux::MTPFileNode::EnsureChildExists(
    const std::string& name,
    uint32 id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const MTPFileNode* child = GetChild(name);
  if (child && child->file_id() == id)
    return;

  children_.set(
      name,
      make_scoped_ptr(new MTPFileNode(id, this, file_id_to_node_map_)));
}

void MTPDeviceDelegateImplLinux::MTPFileNode::ClearNonexistentChildren(
    const std::set<std::string>& children_to_keep) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::set<std::string> children_to_erase;
  for (ChildNodes::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    if (ContainsKey(children_to_keep, it->first))
      continue;
    children_to_erase.insert(it->first);
  }
  for (std::set<std::string>::iterator it = children_to_erase.begin();
       it != children_to_erase.end(); ++it) {
    children_.take_and_erase(*it);
  }
}

bool MTPDeviceDelegateImplLinux::MTPFileNode::DeleteChild(uint32 file_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  for (ChildNodes::iterator it = children_.begin();
       it != children_.end(); ++it) {
    if (it->second->file_id() == file_id) {
      children_.erase(it);
      return true;
    }
  }
  return false;
}

MTPDeviceDelegateImplLinux::MTPDeviceDelegateImplLinux(
    const std::string& device_location)
    : init_state_(UNINITIALIZED),
      task_in_progress_(false),
      device_path_(device_location),
      root_node_(new MTPFileNode(mtpd::kRootFileId,
                                 NULL,
                                 &file_id_to_node_map_)),
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
  base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::GetFileInfoInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 success_callback,
                 error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(file_path,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
}

void MTPDeviceDelegateImplLinux::ReadDirectory(
    const base::FilePath& root,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!root.empty());
  base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::ReadDirectoryInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 root,
                 success_callback,
                 error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(root,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
}

void MTPDeviceDelegateImplLinux::CreateSnapshotFile(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    const CreateSnapshotFileSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  DCHECK(!local_path.empty());
  base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::CreateSnapshotFileInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_file_path,
                 local_path,
                 success_callback,
                 error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(device_file_path,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
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
  base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::ReadBytesInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_file_path,
                 make_scoped_refptr(buf),
                 offset,
                 buf_len,
                 success_callback,
                 error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(device_file_path,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
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

void MTPDeviceDelegateImplLinux::GetFileInfoInternal(
    const base::FilePath& file_path,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  uint32 file_id;
  if (CachedPathToId(file_path, &file_id)) {
    GetFileInfoSuccessCallback success_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::OnDidGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr(),
                   success_callback);
    ErrorCallback error_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback,
                   file_id);


    base::Closure closure = base::Bind(&GetFileInfoOnUIThread,
                                       storage_name_,
                                       file_id,
                                       success_callback_wrapper,
                                       error_callback_wrapper);
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI,
                                         FROM_HERE,
                                         closure));
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::ReadDirectoryInternal(
    const base::FilePath& root,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  uint32 dir_id;
  if (CachedPathToId(root, &dir_id)) {
    GetFileInfoSuccessCallback success_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::OnDidGetFileInfoToReadDirectory,
                   weak_ptr_factory_.GetWeakPtr(),
                   dir_id,
                   success_callback,
                   error_callback);
    ErrorCallback error_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback,
                   dir_id);
    base::Closure closure = base::Bind(&GetFileInfoOnUIThread,
                                       storage_name_,
                                       dir_id,
                                       success_callback_wrapper,
                                       error_callback_wrapper);
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI,
                                         FROM_HERE,
                                         closure));
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::CreateSnapshotFileInternal(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    const CreateSnapshotFileSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  uint32 file_id;
  if (CachedPathToId(device_file_path, &file_id)) {
    scoped_ptr<SnapshotRequestInfo> request_info(
        new SnapshotRequestInfo(file_id,
                                local_path,
                                success_callback,
                                error_callback));
    GetFileInfoSuccessCallback success_callback_wrapper =
        base::Bind(
            &MTPDeviceDelegateImplLinux::OnDidGetFileInfoToCreateSnapshotFile,
            weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&request_info));
    ErrorCallback error_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback,
                   file_id);
    base::Closure closure = base::Bind(&GetFileInfoOnUIThread,
                                       storage_name_,
                                       file_id,
                                       success_callback_wrapper,
                                       error_callback_wrapper);
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI,
                                         FROM_HERE,
                                         closure));
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::ReadBytesInternal(
    const base::FilePath& device_file_path,
    net::IOBuffer* buf, int64 offset, int buf_len,
    const ReadBytesSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  uint32 file_id;
  if (CachedPathToId(device_file_path, &file_id)) {
    ReadBytesRequest request(
        file_id, buf, offset, buf_len,
        base::Bind(&MTPDeviceDelegateImplLinux::OnDidReadBytes,
                   weak_ptr_factory_.GetWeakPtr(),
                   success_callback),
        base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback,
                   file_id));

    base::Closure closure =
        base::Bind(base::Bind(&ReadBytesOnUIThread, storage_name_, request));
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI,
                                         FROM_HERE,
                                         closure));
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::EnsureInitAndRunTask(
    const PendingTaskInfo& task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if ((init_state_ == INITIALIZED) && !task_in_progress_) {
    RunTask(task_info);
    return;
  }

  // Only *Internal functions have empty paths. Since they are the continuation
  // of the current running task, they get to cut in line.
  if (task_info.path.empty())
    pending_tasks_.push_front(task_info);
  else
    pending_tasks_.push_back(task_info);

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

void MTPDeviceDelegateImplLinux::RunTask(const PendingTaskInfo& task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, init_state_);
  DCHECK(!task_in_progress_);
  task_in_progress_ = true;

  bool need_to_check_cache = !task_info.path.empty();
  if (need_to_check_cache) {
    base::FilePath uncached_path =
        NextUncachedPathComponent(task_info.path, task_info.cached_path);
    if (!uncached_path.empty()) {
      // Save the current task and do a cache lookup first.
      pending_tasks_.push_front(task_info);
      FillFileCache(uncached_path);
      return;
    }
  }

  content::BrowserThread::PostTask(task_info.thread_id,
                          task_info.location,
                          task_info.task);
}

void MTPDeviceDelegateImplLinux::WriteDataIntoSnapshotFile(
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  DCHECK_GT(file_info.size, 0);
  DCHECK(task_in_progress_);
  SnapshotRequestInfo request_info(
      current_snapshot_request_info_->file_id,
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

  PendingTaskInfo task_info = pending_tasks_.front();
  pending_tasks_.pop_front();
  RunTask(task_info);
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
    uint32 dir_id,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);
  if (!file_info.is_directory) {
    return HandleDeviceFileError(error_callback,
                                 dir_id,
                                 base::File::FILE_ERROR_NOT_A_DIRECTORY);
  }

  base::Closure task_closure =
      base::Bind(&ReadDirectoryOnUIThread,
                 storage_name_,
                 dir_id,
                 base::Bind(&MTPDeviceDelegateImplLinux::OnDidReadDirectory,
                            weak_ptr_factory_.GetWeakPtr(),
                            dir_id,
                            success_callback),
                 base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                            weak_ptr_factory_.GetWeakPtr(),
                            error_callback,
                            dir_id));
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
    return HandleDeviceFileError(snapshot_request_info->error_callback,
                                 snapshot_request_info->file_id,
                                 error);

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
    uint32 dir_id,
    const ReadDirectorySuccessCallback& success_callback,
    const fileapi::AsyncFileUtil::EntryList& file_list,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  FileIdToMTPFileNodeMap::iterator it = file_id_to_node_map_.find(dir_id);
  DCHECK(it != file_id_to_node_map_.end());
  MTPFileNode* dir_node = it->second;

  fileapi::AsyncFileUtil::EntryList normalized_file_list;
  for (size_t i = 0; i < file_list.size(); ++i) {
    normalized_file_list.push_back(file_list[i]);
    fileapi::DirectoryEntry& entry = normalized_file_list.back();

    // |entry.name| has the file id encoded in it. Decode here.
    size_t separator_idx = entry.name.find_last_of(',');
    DCHECK_NE(std::string::npos, separator_idx);
    std::string file_id_str = entry.name.substr(separator_idx);
    file_id_str = file_id_str.substr(1);  // Get rid of the comma.
    uint32 file_id = 0;
    bool ret = base::StringToUint(file_id_str, &file_id);
    DCHECK(ret);
    entry.name = entry.name.substr(0, separator_idx);

    // Refresh the in memory tree.
    dir_node->EnsureChildExists(entry.name, file_id);
    child_nodes_seen_.insert(entry.name);
  }

  success_callback.Run(normalized_file_list, has_more);
  if (has_more)
    return;  // Wait to be called again.

  // Last call, finish book keeping and continue with the next request.
  dir_node->ClearNonexistentChildren(child_nodes_seen_);
  child_nodes_seen_.clear();

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

void MTPDeviceDelegateImplLinux::OnDidFillFileCache(
    const base::FilePath& path,
    const fileapi::AsyncFileUtil::EntryList& /* file_list */,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(path.IsParent(pending_tasks_.front().path));
  if (has_more)
    return;  // Wait until all entries have been read.
  pending_tasks_.front().cached_path = path;
}

void MTPDeviceDelegateImplLinux::OnFillFileCacheFailed(
    base::File::Error /* error */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // When filling the cache fails for the task at the front of the queue, clear
  // the path of the task so it will not try to do any more caching. Instead,
  // the task will just run and fail the CachedPathToId() lookup.
  pending_tasks_.front().path.clear();
}

void MTPDeviceDelegateImplLinux::HandleDeviceFileError(
    const ErrorCallback& error_callback,
    uint32 file_id,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  FileIdToMTPFileNodeMap::iterator it = file_id_to_node_map_.find(file_id);
  if (it != file_id_to_node_map_.end()) {
    MTPFileNode* parent = it->second->parent();
    if (parent) {
      bool ret = parent->DeleteChild(file_id);
      DCHECK(ret);
    }
  }
  error_callback.Run(error);
  PendingRequestDone();
}

base::FilePath MTPDeviceDelegateImplLinux::NextUncachedPathComponent(
    const base::FilePath& path,
    const base::FilePath& cached_path) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(cached_path.empty() || cached_path.IsParent(path));

  base::FilePath uncached_path;
  std::string device_relpath = GetDeviceRelativePath(device_path_, path);
  if (!device_relpath.empty() && device_relpath != kRootPath) {
    uncached_path = device_path_;
    std::vector<std::string> device_relpath_components;
    base::SplitString(device_relpath, '/', &device_relpath_components);
    DCHECK(!device_relpath_components.empty());
    bool all_components_cached = true;
    const MTPFileNode* current_node = root_node_.get();
    for (size_t i = 0; i < device_relpath_components.size(); ++i) {
      current_node = current_node->GetChild(device_relpath_components[i]);
      if (!current_node) {
        // With a cache miss, check if it is a genuine failure. If so, pretend
        // the entire |path| is cached, so there is no further attempt to do
        // more caching. The actual operation will then fail.
        all_components_cached =
            !cached_path.empty() && (uncached_path == cached_path);
        break;
      }
      uncached_path = uncached_path.Append(device_relpath_components[i]);
    }
    if (all_components_cached)
      uncached_path.clear();
  }
  return uncached_path;
}

void MTPDeviceDelegateImplLinux::FillFileCache(
    const base::FilePath& uncached_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);

  ReadDirectorySuccessCallback success_callback =
      base::Bind(&MTPDeviceDelegateImplLinux::OnDidFillFileCache,
                 weak_ptr_factory_.GetWeakPtr(),
                 uncached_path);
  ErrorCallback error_callback =
      base::Bind(&MTPDeviceDelegateImplLinux::OnFillFileCacheFailed,
                 weak_ptr_factory_.GetWeakPtr());
  ReadDirectoryInternal(uncached_path, success_callback, error_callback);
}


bool MTPDeviceDelegateImplLinux::CachedPathToId(const base::FilePath& path,
                                                uint32* id) const {
  DCHECK(id);

  std::string device_relpath = GetDeviceRelativePath(device_path_, path);
  if (device_relpath.empty())
    return false;
  std::vector<std::string> device_relpath_components;
  if (device_relpath != kRootPath)
    base::SplitString(device_relpath, '/', &device_relpath_components);
  const MTPFileNode* current_node = root_node_.get();
  for (size_t i = 0; i < device_relpath_components.size(); ++i) {
    current_node = current_node->GetChild(device_relpath_components[i]);
    if (!current_node)
      return false;
  }
  *id = current_node->file_id();
  return true;
}

void CreateMTPDeviceAsyncDelegate(
    const std::string& device_location,
    const CreateMTPDeviceAsyncDelegateCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(new MTPDeviceDelegateImplLinux(device_location));
}
