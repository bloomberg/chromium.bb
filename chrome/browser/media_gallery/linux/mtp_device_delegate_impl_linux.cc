// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/linux/mtp_device_delegate_impl_linux.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/string_util.h"
#include "chrome/browser/media_gallery/linux/mtp_device_object_enumerator.h"
#include "chrome/browser/media_gallery/linux/mtp_device_operations_utils.h"
#include "chrome/browser/media_gallery/linux/mtp_get_file_info_worker.h"
#include "chrome/browser/media_gallery/linux/mtp_open_storage_worker.h"
#include "chrome/browser/media_gallery/linux/mtp_read_directory_worker.h"
#include "chrome/browser/media_gallery/linux/mtp_read_file_worker.h"
#include "chrome/browser/media_gallery/linux/mtp_recursive_device_object_enumerator.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"

using base::Bind;
using base::PlatformFileError;
using base::PlatformFileInfo;
using base::SequencedTaskRunner;
using content::BrowserThread;
using fileapi::FileSystemFileUtil;

namespace chrome {

namespace {

// File path separator constant.
const char kRootPath[] = "/";

// Does nothing.
// This method is used to handle the results of
// MediaTransferProtocolManager::CloseStorage method call.
void DoNothing(bool error) {
}

// Closes the device storage on the UI thread.
void CloseStorageOnUIThread(const std::string& device_handle) {
  DCHECK(!device_handle.empty());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GetMediaTransferProtocolManager()->CloseStorage(device_handle,
                                                  Bind(&DoNothing));
}

// Returns the device relative file path given |file_path|.
// E.g.: If the |file_path| is "/usb:2,2:12345/DCIM" and |registered_dev_path|
// is "/usb:2,2:12345", this function returns the device relative path which is
// "/DCIM".
std::string GetDeviceRelativePath(const std::string& registered_dev_path,
                                  const std::string& file_path) {
  DCHECK(!registered_dev_path.empty());
  DCHECK(!file_path.empty());

  std::string actual_file_path;
  if (registered_dev_path == file_path) {
    actual_file_path = kRootPath;
  } else {
    actual_file_path = file_path;
    ReplaceFirstSubstringAfterOffset(&actual_file_path, 0,
                                     registered_dev_path.c_str(), "");
  }
  DCHECK(!actual_file_path.empty());
  return actual_file_path;
}

}  // namespace

MTPDeviceDelegateImplLinux::MTPDeviceDelegateImplLinux(
    const std::string& device_location,
    base::SequencedTaskRunner* media_task_runner)
    : device_path_(device_location),
      media_task_runner_(media_task_runner),
      on_task_completed_event_(false, false),
      on_shutdown_event_(true, false) {
  CHECK(!device_path_.empty());
}

PlatformFileError MTPDeviceDelegateImplLinux::GetFileInfo(
    const FilePath& file_path,
    PlatformFileInfo* file_info) {
  if (!LazyInit())
    return base::PLATFORM_FILE_ERROR_FAILED;

  scoped_refptr<MTPGetFileInfoWorker> worker(new MTPGetFileInfoWorker(
      device_handle_, GetDeviceRelativePath(device_path_, file_path.value()),
      media_task_runner_, &on_task_completed_event_, &on_shutdown_event_));
  worker->Run();
  return worker->get_file_info(file_info);
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
    MTPDeviceDelegateImplLinux::CreateFileEnumerator(
        const FilePath& root,
        bool recursive) {
  if (root.value().empty() || !LazyInit()) {
    return make_scoped_ptr(new FileSystemFileUtil::EmptyFileEnumerator())
        .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
  }

  scoped_refptr<MTPReadDirectoryWorker> worker(new MTPReadDirectoryWorker(
      device_handle_, GetDeviceRelativePath(device_path_, root.value()),
      media_task_runner_, &on_task_completed_event_, &on_shutdown_event_));
  worker->Run();

  if (worker->get_file_entries().empty()) {
    return make_scoped_ptr(new FileSystemFileUtil::EmptyFileEnumerator())
        .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
  }

  if (recursive) {
    return make_scoped_ptr(new MTPRecursiveDeviceObjectEnumerator(
        device_handle_, media_task_runner_, worker->get_file_entries(),
        &on_task_completed_event_, &on_shutdown_event_))
        .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
  }
  return make_scoped_ptr(
      new MTPDeviceObjectEnumerator(worker->get_file_entries()))
          .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
}

PlatformFileError MTPDeviceDelegateImplLinux::CreateSnapshotFile(
    const FilePath& device_file_path,
    const FilePath& local_path,
    PlatformFileInfo* file_info) {
  if (!LazyInit())
    return base::PLATFORM_FILE_ERROR_FAILED;

  PlatformFileError error = GetFileInfo(device_file_path, file_info);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (file_info->is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  if (file_info->size <= 0 || file_info->size > kuint32max)
    return base::PLATFORM_FILE_ERROR_FAILED;

  scoped_refptr<MTPReadFileWorker> worker(new MTPReadFileWorker(
      device_handle_,
      GetDeviceRelativePath(device_path_, device_file_path.value()),
      file_info->size, local_path, media_task_runner_,
      &on_task_completed_event_, &on_shutdown_event_));
  worker->Run();
  if (!worker->Succeeded())
    return base::PLATFORM_FILE_ERROR_FAILED;

  // Modify the last modified time to null. This prevents the time stamp
  // verfication in LocalFileStreamReader.
  file_info->last_modified = base::Time();
  return base::PLATFORM_FILE_OK;
}

void MTPDeviceDelegateImplLinux::CancelPendingTasksAndDeleteDelegate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Caution: This function is called on the UI thread. Access only the thread
  // safe member variables in this function.
  on_shutdown_event_.Signal();
  on_task_completed_event_.Signal();
  media_task_runner_->DeleteSoon(FROM_HERE, this);
}

MTPDeviceDelegateImplLinux::~MTPDeviceDelegateImplLinux() {
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());
  if (!device_handle_.empty()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            Bind(&CloseStorageOnUIThread, device_handle_));
  }
}

bool MTPDeviceDelegateImplLinux::LazyInit() {
  DCHECK(media_task_runner_);
  DCHECK(media_task_runner_->RunsTasksOnCurrentThread());

  if (!device_handle_.empty())
    return true;  // Already successfully initialized.

  std::string storage_name;
  RemoveChars(device_path_, kRootPath, &storage_name);
  DCHECK(!storage_name.empty());
  scoped_refptr<MTPOpenStorageWorker> worker(new MTPOpenStorageWorker(
      storage_name, media_task_runner_, &on_task_completed_event_,
      &on_shutdown_event_));
  worker->Run();
  device_handle_ = worker->device_handle();
  return !device_handle_.empty();
}

void CreateMTPDeviceDelegate(const std::string& device_location,
                             base::SequencedTaskRunner* media_task_runner,
                             const CreateMTPDeviceDelegateCallback& cb) {
  cb.Run(new MTPDeviceDelegateImplLinux(device_location, media_task_runner));
}

}  // namespace chrome
