// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/mac/mtp_device_delegate_impl_mac.h"

#include "base/memory/scoped_nsobject.h"
#include "base/sequenced_task_runner.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"
#include "chrome/browser/storage_monitor/image_capture_device.h"
#include "chrome/browser/storage_monitor/image_capture_device_manager.h"
#include "chrome/browser/storage_monitor/media_storage_util.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

// This class handles the UI-thread hand-offs needed to interface
// with the ImageCapture library. It will forward callbacks to
// its delegate on the task runner with which it is created. All
// interactions with it are done on the UI thread, but it may be
// created/destroyed on another thread.
class MTPDeviceDelegateImplMac::DeviceListener
    : public ImageCaptureDeviceListener,
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
                         const base::PlatformFileInfo& info) OVERRIDE;
  virtual void NoMoreItems() OVERRIDE;
  virtual void DownloadedFile(const std::string& name,
                              base::PlatformFileError error) OVERRIDE;
  virtual void DeviceRemoved() OVERRIDE;

 private:
  scoped_nsobject<ImageCaptureDevice> camera_device_;

  // Weak pointer
  MTPDeviceDelegateImplMac* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeviceListener);
};

void MTPDeviceDelegateImplMac::DeviceListener::OpenCameraSession(
    const std::string& device_id) {
  camera_device_.reset(
      [ImageCaptureDeviceManager::deviceForUUID(device_id) retain]);
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
    const base::PlatformFileInfo& info) {
  delegate_->ItemAdded(name, info);
}

void MTPDeviceDelegateImplMac::DeviceListener::NoMoreItems() {
  delegate_->NoMoreItems();
}

void MTPDeviceDelegateImplMac::DeviceListener::DownloadedFile(
    const std::string& name,
    base::PlatformFileError error) {
delegate_->DownloadedFile(name, error);
}

void MTPDeviceDelegateImplMac::DeviceListener::DeviceRemoved() {
  [camera_device_ close];
  camera_device_.reset();
}

MTPDeviceDelegateImplMac::MTPDeviceDelegateImplMac(
    const std::string& device_id,
    const base::FilePath::StringType& synthetic_path,
    base::SequencedTaskRunner* media_task_runner)
    : device_id_(device_id),
      root_path_(synthetic_path),
      media_task_runner_(media_task_runner),
      enumerator_(NULL),
      file_download_event_(NULL),
      file_download_error_(base::PLATFORM_FILE_OK),
      received_all_files_(false) {

  // Make a synthetic entry for the root of the filesystem.
  base::PlatformFileInfo info;
  info.is_directory = true;
  file_info_[root_path_.value()] = info;

  camera_interface_.reset(new DeviceListener(this));
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DeviceListener::OpenCameraSession,
                 base::Unretained(camera_interface_.get()),
                 device_id_));
}

MTPDeviceDelegateImplMac::~MTPDeviceDelegateImplMac() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(enumerator_ == NULL);
}

base::PlatformFileError MTPDeviceDelegateImplMac::GetFileInfo(
    const base::FilePath& file_path,
    base::PlatformFileInfo* file_info) {
  base::AutoLock lock(mutex_);
  base::hash_map<base::FilePath::StringType,
                 base::PlatformFileInfo>::const_iterator i =
      file_info_.find(file_path.value());
  if (i == file_info_.end())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  *file_info = i->second;
  return base::PLATFORM_FILE_OK;
}

scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
MTPDeviceDelegateImplMac::CreateFileEnumerator(const base::FilePath& root,
                                               bool recursive) {
  base::AutoLock lock(mutex_);
  DCHECK(!enumerator_);
  enumerator_ = new Enumerator(this);
  return make_scoped_ptr(enumerator_)
      .PassAs<fileapi::FileSystemFileUtil::AbstractFileEnumerator>();
}

base::PlatformFileError MTPDeviceDelegateImplMac::CreateSnapshotFile(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    base::PlatformFileInfo* file_info) {
  base::PlatformFileError error = GetFileInfo(device_file_path, file_info);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  // Set up to wait for download. Callers are ensuring this particular function
  // will not be re-entered.
  base::WaitableEvent waiter(true, false);
  {
    base::AutoLock lock(mutex_);
    DCHECK(file_download_event_ == NULL);
    file_download_event_ = &waiter;
  }

  // Start the download in the UI thread.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DeviceListener::DownloadFile,
                 base::Unretained(camera_interface_.get()),
                 device_file_path.BaseName().value(), local_path));
  waiter.Wait();
  {
    base::AutoLock lock(mutex_);
    file_download_event_ = NULL;
    error = file_download_error_;
  }

  // Modify the last modified time to null. This prevents the time stamp
  // verification in LocalFileStreamReader.
  file_info->last_modified = base::Time();

  return error;
}

void MTPDeviceDelegateImplMac::CancelPendingTasksAndDeleteDelegate() {
  // Artificially pretend that we have already gotten all items we're going
  // to get.
  NoMoreItems();

  {
    base::AutoLock lock(mutex_);
    // Artificially wake up any downloads pending with an error code.
    if (file_download_event_) {
      file_download_error_ = base::PLATFORM_FILE_ERROR_FAILED;
      file_download_event_->Signal();
    }
  }

  // Schedule the camera session to be closed and the interface deleted.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&DeviceListener::CloseCameraSessionAndDelete,
                 base::Unretained(camera_interface_.release())));

  media_task_runner_->DeleteSoon(FROM_HERE, this);
}

void MTPDeviceDelegateImplMac::ItemAdded(
    const std::string& name, const base::PlatformFileInfo& info) {
  base::AutoLock lock(mutex_);

  // Make sure that if we're canceled and an enumerator is awake, that
  // it will stay consistent. May need to revisit this if we need
  // notifications of files added after we think we're done.
  if (received_all_files_)
    return;

  // TODO(gbillock): Currently we flatten all files into a single
  // directory. That's pretty much how PTP devices work, but if we want
  // to support ImageCapture for USB, we need to change this.
  if (info.is_directory)
    return;

  base::FilePath item_filename = root_path_.Append(name);
  file_info_[item_filename.value()] = info;
  file_paths_.push_back(item_filename);

  if (enumerator_)
    enumerator_->ItemsChanged();
}

void MTPDeviceDelegateImplMac::NoMoreItems() {
  base::AutoLock lock(mutex_);
  received_all_files_ = true;

  if (enumerator_)
    enumerator_->ItemsChanged();
}

void MTPDeviceDelegateImplMac::DownloadedFile(
    const std::string& name, base::PlatformFileError error) {
  // If we're cancelled and deleting, we have already signaled all enumerators.
  if (!camera_interface_.get())
    return;

  base::AutoLock lock(mutex_);
  file_download_error_ = error;
  file_download_event_->Signal();
}

base::FilePath MTPDeviceDelegateImplMac::GetFile(size_t index) {
  base::AutoLock lock(mutex_);
  if (index >= file_paths_.size())
    return base::FilePath();
  else
    return file_paths_[index];
}

bool MTPDeviceDelegateImplMac::ReceivedAllFiles() {
  base::AutoLock lock(mutex_);
  return received_all_files_;
}

void MTPDeviceDelegateImplMac::RemoveEnumerator(Enumerator* enumerator) {
  base::AutoLock lock(mutex_);
  DCHECK(enumerator_ == enumerator);
  enumerator_ = NULL;
}

MTPDeviceDelegateImplMac::Enumerator::Enumerator(
    MTPDeviceDelegateImplMac* delegate)
    : delegate_(delegate),
      position_(0),
      wait_for_items_(false, false) {}

MTPDeviceDelegateImplMac::Enumerator::~Enumerator() {
  delegate_->RemoveEnumerator(this);
}

base::FilePath MTPDeviceDelegateImplMac::Enumerator::Next() {
  base::FilePath next_file = delegate_->GetFile(position_);
  while (next_file.empty() && !delegate_->ReceivedAllFiles()) {
    wait_for_items_.Wait();
    next_file = delegate_->GetFile(position_);
  }

  position_++;
  return next_file;
}

int64 MTPDeviceDelegateImplMac::Enumerator::Size() {
  base::PlatformFileInfo info;
  delegate_->GetFileInfo(delegate_->GetFile(position_ - 1), &info);
  return info.size;
}

base::Time MTPDeviceDelegateImplMac::Enumerator::LastModifiedTime() {
  base::PlatformFileInfo info;
  delegate_->GetFileInfo(delegate_->GetFile(position_ - 1), &info);
  return info.last_modified;
}

bool MTPDeviceDelegateImplMac::Enumerator::IsDirectory() {
  base::PlatformFileInfo info;
  delegate_->GetFileInfo(delegate_->GetFile(position_ - 1), &info);
  return info.is_directory;
}

void MTPDeviceDelegateImplMac::Enumerator::ItemsChanged() {
  wait_for_items_.Signal();
}

void CreateMTPDeviceDelegate(const std::string& device_location,
                             base::SequencedTaskRunner* media_task_runner,
                             const CreateMTPDeviceDelegateCallback& cb) {
  std::string device_name = base::FilePath(device_location).BaseName().value();
  std::string device_id;
  MediaStorageUtil::Type type;
  bool cracked = MediaStorageUtil::CrackDeviceId(device_name,
                                                 &type, &device_id);
  DCHECK(cracked);
  DCHECK_EQ(MediaStorageUtil::MAC_IMAGE_CAPTURE, type);

  cb.Run(new MTPDeviceDelegateImplMac(device_id, device_location,
                                      media_task_runner));
}

}  // namespace chrome

