// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/mac/mtp_device_delegate_impl_mac.h"

#include <algorithm>

#include "base/mac/scoped_nsobject.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/storage_monitor/image_capture_device.h"
#include "components/storage_monitor/image_capture_device_manager.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/async_file_util.h"

namespace {

int kReadDirectoryTimeLimitSeconds = 20;

typedef MTPDeviceAsyncDelegate::CreateSnapshotFileSuccessCallback
    CreateSnapshotFileSuccessCallback;
typedef MTPDeviceAsyncDelegate::ErrorCallback ErrorCallback;
typedef MTPDeviceAsyncDelegate::GetFileInfoSuccessCallback
    GetFileInfoSuccessCallback;
typedef MTPDeviceAsyncDelegate::ReadDirectorySuccessCallback
    ReadDirectorySuccessCallback;

}  // namespace

// This class handles the UI-thread hand-offs needed to interface
// with the ImageCapture library. It will forward callbacks to
// its delegate on the task runner with which it is created. All
// interactions with it are done on the UI thread, but it may be
// created/destroyed on another thread.
class MTPDeviceDelegateImplMac::DeviceListener
    : public storage_monitor::ImageCaptureDeviceListener,
      public base::SupportsWeakPtr<DeviceListener> {
 public:
  DeviceListener(MTPDeviceDelegateImplMac* delegate)
      : delegate_(delegate) {}
  virtual ~DeviceListener() {}

  void OpenCameraSession(const std::string& device_id);
  void CloseCameraSessionAndDelete();

  void DownloadFile(const std::string& name, const base::FilePath& local_path);

  // ImageCaptureDeviceListener
  virtual void ItemAdded(const std::string& name,
                         const base::File::Info& info) OVERRIDE;
  virtual void NoMoreItems() OVERRIDE;
  virtual void DownloadedFile(const std::string& name,
                              base::File::Error error) OVERRIDE;
  virtual void DeviceRemoved() OVERRIDE;

  // Used during delegate destruction to ensure there are no more calls
  // to the delegate by the listener.
  virtual void ResetDelegate();

 private:
  base::scoped_nsobject<ImageCaptureDevice> camera_device_;

  // Weak pointer
  MTPDeviceDelegateImplMac* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeviceListener);
};

void MTPDeviceDelegateImplMac::DeviceListener::OpenCameraSession(
    const std::string& device_id) {
  camera_device_.reset(
      [storage_monitor::ImageCaptureDeviceManager::deviceForUUID(device_id)
          retain]);
  [camera_device_ setListener:AsWeakPtr()];
  [camera_device_ open];
}

void MTPDeviceDelegateImplMac::DeviceListener::CloseCameraSessionAndDelete() {
  [camera_device_ close];
  [camera_device_ setListener:base::WeakPtr<DeviceListener>()];

  delete this;
}

void MTPDeviceDelegateImplMac::DeviceListener::DownloadFile(
    const std::string& name,
    const base::FilePath& local_path) {
  [camera_device_ downloadFile:name localPath:local_path];
}

void MTPDeviceDelegateImplMac::DeviceListener::ItemAdded(
    const std::string& name,
    const base::File::Info& info) {
  if (delegate_)
    delegate_->ItemAdded(name, info);
}

void MTPDeviceDelegateImplMac::DeviceListener::NoMoreItems() {
  if (delegate_)
    delegate_->NoMoreItems();
}

void MTPDeviceDelegateImplMac::DeviceListener::DownloadedFile(
    const std::string& name,
    base::File::Error error) {
  if (delegate_)
    delegate_->DownloadedFile(name, error);
}

void MTPDeviceDelegateImplMac::DeviceListener::DeviceRemoved() {
  [camera_device_ close];
  camera_device_.reset();
  if (delegate_)
    delegate_->NoMoreItems();
}

void MTPDeviceDelegateImplMac::DeviceListener::ResetDelegate() {
  delegate_ = NULL;
}

MTPDeviceDelegateImplMac::MTPDeviceDelegateImplMac(
    const std::string& device_id,
    const base::FilePath::StringType& synthetic_path)
    : device_id_(device_id),
      root_path_(synthetic_path),
      received_all_files_(false),
      weak_factory_(this) {

  // Make a synthetic entry for the root of the filesystem.
  base::File::Info info;
  info.is_directory = true;
  file_paths_.push_back(root_path_);
  file_info_[root_path_.value()] = info;

  camera_interface_.reset(new DeviceListener(this));
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DeviceListener::OpenCameraSession,
                 base::Unretained(camera_interface_.get()),
                 device_id_));
}

MTPDeviceDelegateImplMac::~MTPDeviceDelegateImplMac() {
}

namespace {

void ForwardGetFileInfo(
    base::File::Info* info,
    base::File::Error* error,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  if (*error == base::File::FILE_OK)
    success_callback.Run(*info);
  else
    error_callback.Run(*error);
}

}  // namespace

void MTPDeviceDelegateImplMac::GetFileInfo(
    const base::FilePath& file_path,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  base::File::Info* info = new base::File::Info;
  base::File::Error* error = new base::File::Error;
  // Note: ownership of these objects passed into the reply callback.
  content::BrowserThread::PostTaskAndReply(content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&MTPDeviceDelegateImplMac::GetFileInfoImpl,
                 base::Unretained(this), file_path, info, error),
      base::Bind(&ForwardGetFileInfo,
                 base::Owned(info), base::Owned(error),
                 success_callback, error_callback));
}

void MTPDeviceDelegateImplMac::ReadDirectory(
      const base::FilePath& root,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&MTPDeviceDelegateImplMac::ReadDirectoryImpl,
                 base::Unretained(this),
                 root, success_callback, error_callback));
}

void MTPDeviceDelegateImplMac::CreateSnapshotFile(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      const CreateSnapshotFileSuccessCallback& success_callback,
      const ErrorCallback& error_callback) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&MTPDeviceDelegateImplMac::DownloadFile,
                 base::Unretained(this),
                 device_file_path, local_path,
                 success_callback, error_callback));
}

bool MTPDeviceDelegateImplMac::IsStreaming() {
  return false;
}

void MTPDeviceDelegateImplMac::ReadBytes(
    const base::FilePath& device_file_path,
    net::IOBuffer* buf, int64 offset, int buf_len,
    const ReadBytesSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  NOTREACHED();
}

void MTPDeviceDelegateImplMac::CancelPendingTasksAndDeleteDelegate() {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&MTPDeviceDelegateImplMac::CancelAndDelete,
                 base::Unretained(this)));
}

void MTPDeviceDelegateImplMac::GetFileInfoImpl(
    const base::FilePath& file_path,
    base::File::Info* file_info,
    base::File::Error* error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::hash_map<base::FilePath::StringType,
                 base::File::Info>::const_iterator i =
      file_info_.find(file_path.value());
  if (i == file_info_.end()) {
    *error = base::File::FILE_ERROR_NOT_FOUND;
    return;
  }
  *file_info = i->second;
  *error = base::File::FILE_OK;
}

void MTPDeviceDelegateImplMac::ReadDirectoryImpl(
      const base::FilePath& root,
      const ReadDirectorySuccessCallback& success_callback,
      const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  read_dir_transactions_.push_back(ReadDirectoryRequest(
      root, success_callback, error_callback));

  if (received_all_files_) {
    NotifyReadDir();
    return;
  }

  // Schedule a timeout in case the directory read doesn't complete.
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&MTPDeviceDelegateImplMac::ReadDirectoryTimeout,
                 weak_factory_.GetWeakPtr(), root),
      base::TimeDelta::FromSeconds(kReadDirectoryTimeLimitSeconds));
}

void MTPDeviceDelegateImplMac::ReadDirectoryTimeout(
    const base::FilePath& root) {
  if (received_all_files_)
    return;

  for (ReadDirTransactionList::iterator iter = read_dir_transactions_.begin();
       iter != read_dir_transactions_.end();) {
    if (iter->directory != root) {
      ++iter;
      continue;
    }
    iter->error_callback.Run(base::File::FILE_ERROR_ABORT);
    iter = read_dir_transactions_.erase(iter);
  }
}

void MTPDeviceDelegateImplMac::DownloadFile(
      const base::FilePath& device_file_path,
      const base::FilePath& local_path,
      const CreateSnapshotFileSuccessCallback& success_callback,
      const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::File::Error error;
  base::File::Info info;
  GetFileInfoImpl(device_file_path, &info, &error);
  if (error != base::File::FILE_OK) {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                     base::Bind(error_callback,
                                                error));
    return;
  }

  base::FilePath relative_path;
  root_path_.AppendRelativePath(device_file_path, &relative_path);

  read_file_transactions_.push_back(
      ReadFileRequest(relative_path.value(), local_path,
                      success_callback, error_callback));

  camera_interface_->DownloadFile(relative_path.value(), local_path);
}

void MTPDeviceDelegateImplMac::CancelAndDelete() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Artificially pretend that we have already gotten all items we're going
  // to get.
  NoMoreItems();

  CancelDownloads();

  // Schedule the camera session to be closed and the interface deleted.
  // This will cancel any downloads in progress.
  camera_interface_->ResetDelegate();
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DeviceListener::CloseCameraSessionAndDelete,
                 base::Unretained(camera_interface_.release())));

  delete this;
}

void MTPDeviceDelegateImplMac::CancelDownloads() {
  // Cancel any outstanding callbacks.
  for (ReadFileTransactionList::iterator iter = read_file_transactions_.begin();
       iter != read_file_transactions_.end(); ++iter) {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
        base::Bind(iter->error_callback,
                   base::File::FILE_ERROR_ABORT));
  }
  read_file_transactions_.clear();

  for (ReadDirTransactionList::iterator iter = read_dir_transactions_.begin();
       iter != read_dir_transactions_.end(); ++iter) {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
        base::Bind(iter->error_callback, base::File::FILE_ERROR_ABORT));
  }
  read_dir_transactions_.clear();
}

// Called on the UI thread by the listener
void MTPDeviceDelegateImplMac::ItemAdded(
    const std::string& name, const base::File::Info& info) {
  if (received_all_files_)
    return;

  // This kinda should go in a Join method in FilePath...
  base::FilePath relative_path(name);
  std::vector<base::FilePath::StringType> components;
  relative_path.GetComponents(&components);
  base::FilePath item_filename = root_path_;
  for (std::vector<base::FilePath::StringType>::iterator iter =
           components.begin();
       iter != components.end(); ++iter) {
    item_filename = item_filename.Append(*iter);
  }

  file_info_[item_filename.value()] = info;
  file_paths_.push_back(item_filename);

  // TODO(gbillock): Should we send new files to
  // read_dir_transactions_ callbacks?
}

// Called in the UI thread by delegate.
void MTPDeviceDelegateImplMac::NoMoreItems() {
  received_all_files_ = true;
  std::sort(file_paths_.begin(), file_paths_.end());

  NotifyReadDir();
}

void MTPDeviceDelegateImplMac::NotifyReadDir() {
  for (ReadDirTransactionList::iterator iter = read_dir_transactions_.begin();
       iter != read_dir_transactions_.end(); ++iter) {
    base::FilePath read_path = iter->directory;
    // This code assumes that the list of paths is sorted, so we skip to
    // where we find the entry for the directory, then read out all first-level
    // children. We then break when the DirName is greater than the read_path,
    // as that means we've passed the subdir we're reading.
    storage::AsyncFileUtil::EntryList entry_list;
    bool found_path = false;
    for (size_t i = 0; i < file_paths_.size(); ++i) {
      if (file_paths_[i] == read_path) {
        found_path = true;
        continue;
      }
      if (!read_path.IsParent(file_paths_[i])) {
        if (read_path < file_paths_[i].DirName())
          break;
        continue;
      }
      if (file_paths_[i].DirName() != read_path)
        continue;

      base::FilePath relative_path;
      read_path.AppendRelativePath(file_paths_[i], &relative_path);
      base::File::Info info = file_info_[file_paths_[i].value()];
      storage::DirectoryEntry entry;
      entry.name = relative_path.value();
      entry.is_directory = info.is_directory;
      entry.size = info.size;
      entry.last_modified_time = info.last_modified;
      entry_list.push_back(entry);
    }

    if (found_path) {
      content::BrowserThread::PostTask(content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(iter->success_callback, entry_list, false));
    } else {
      content::BrowserThread::PostTask(content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(iter->error_callback,
                     base::File::FILE_ERROR_NOT_FOUND));
    }
  }

  read_dir_transactions_.clear();
}

// Invoked on UI thread from the listener.
void MTPDeviceDelegateImplMac::DownloadedFile(
    const std::string& name, base::File::Error error) {
  // If we're cancelled and deleting, we may have deleted the camera.
  if (!camera_interface_.get())
    return;

  bool found = false;
  ReadFileTransactionList::iterator iter = read_file_transactions_.begin();
  for (; iter != read_file_transactions_.end(); ++iter) {
    if (iter->request_file == name) {
      found = true;
      break;
    }
  }
  if (!found)
    return;

  if (error != base::File::FILE_OK) {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
        base::Bind(iter->error_callback, error));
    read_file_transactions_.erase(iter);
    return;
  }

  base::FilePath relative_path(name);
  std::vector<base::FilePath::StringType> components;
  relative_path.GetComponents(&components);
  base::FilePath item_filename = root_path_;
  for (std::vector<base::FilePath::StringType>::iterator i =
           components.begin();
       i != components.end(); ++i) {
    item_filename = item_filename.Append(*i);
  }

  base::File::Info info = file_info_[item_filename.value()];
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(iter->success_callback, info, iter->snapshot_file));
  read_file_transactions_.erase(iter);
}

MTPDeviceDelegateImplMac::ReadFileRequest::ReadFileRequest(
    const std::string& file,
    const base::FilePath& snapshot_filename,
    CreateSnapshotFileSuccessCallback success_cb,
    ErrorCallback error_cb)
    : request_file(file),
      snapshot_file(snapshot_filename),
      success_callback(success_cb),
      error_callback(error_cb) {}

MTPDeviceDelegateImplMac::ReadFileRequest::ReadFileRequest() {}

MTPDeviceDelegateImplMac::ReadFileRequest::~ReadFileRequest() {}

MTPDeviceDelegateImplMac::ReadDirectoryRequest::ReadDirectoryRequest(
    const base::FilePath& dir,
    ReadDirectorySuccessCallback success_cb,
    ErrorCallback error_cb)
    : directory(dir),
      success_callback(success_cb),
      error_callback(error_cb) {}

MTPDeviceDelegateImplMac::ReadDirectoryRequest::~ReadDirectoryRequest() {}

void CreateMTPDeviceAsyncDelegate(
    const base::FilePath::StringType& device_location,
    const CreateMTPDeviceAsyncDelegateCallback& cb) {
  std::string device_name = base::FilePath(device_location).BaseName().value();
  std::string device_id;
  storage_monitor::StorageInfo::Type type;
  bool cracked = storage_monitor::StorageInfo::CrackDeviceId(
      device_name, &type, &device_id);
  DCHECK(cracked);
  DCHECK_EQ(storage_monitor::StorageInfo::MAC_IMAGE_CAPTURE, type);

  cb.Run(new MTPDeviceDelegateImplMac(device_id, device_location));
}
