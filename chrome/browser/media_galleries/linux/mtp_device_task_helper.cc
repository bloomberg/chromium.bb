// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/mtp_device_task_helper.h"

#include "base/logging.h"
#include "chrome/browser/media_galleries/linux/mtp_device_object_enumerator.h"
#include "chrome/browser/media_galleries/linux/mtp_read_file_worker.h"
#include "chrome/browser/media_galleries/linux/snapshot_file_details.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "webkit/fileapi/async_file_util.h"
#include "webkit/fileapi/file_system_util.h"

namespace chrome {

namespace {

// Does nothing.
// This method is used to handle the results of
// MediaTransferProtocolManager::CloseStorage method call.
void DoNothing(bool error) {
}

device::MediaTransferProtocolManager* GetMediaTransferProtocolManager() {
  return StorageMonitor::GetInstance()->media_transfer_protocol_manager();
}

}  // namespace

MTPDeviceTaskHelper::MTPDeviceTaskHelper()
    : weak_ptr_factory_(this) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

MTPDeviceTaskHelper::~MTPDeviceTaskHelper() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void MTPDeviceTaskHelper::OpenStorage(const std::string& storage_name,
                                      const OpenStorageCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!storage_name.empty());
  if (!device_handle_.empty()) {
    content::BrowserThread::PostTask(content::BrowserThread::IO,
                                     FROM_HERE,
                                     base::Bind(callback, true));
    return;
  }
  GetMediaTransferProtocolManager()->OpenStorage(
      storage_name, mtpd::kReadOnlyMode,
      base::Bind(&MTPDeviceTaskHelper::OnDidOpenStorage,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void MTPDeviceTaskHelper::GetFileInfoByPath(
    const std::string& file_path,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::PLATFORM_FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->GetFileInfoByPath(
      device_handle_, file_path,
      base::Bind(&MTPDeviceTaskHelper::OnGetFileInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::ReadDirectoryByPath(
    const std::string& dir_path,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::PLATFORM_FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->ReadDirectoryByPath(
      device_handle_, dir_path,
      base::Bind(&MTPDeviceTaskHelper::OnDidReadDirectoryByPath,
                 weak_ptr_factory_.GetWeakPtr(),
                 success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::WriteDataIntoSnapshotFile(
    const SnapshotRequestInfo& request_info,
    const base::PlatformFileInfo& snapshot_file_info) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (device_handle_.empty()) {
    return HandleDeviceError(request_info.error_callback,
                             base::PLATFORM_FILE_ERROR_FAILED);
  }

  if (!read_file_worker_)
    read_file_worker_.reset(new MTPReadFileWorker(device_handle_));
  read_file_worker_->WriteDataIntoSnapshotFile(request_info,
                                               snapshot_file_info);
}

void MTPDeviceTaskHelper::CloseStorage() const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (device_handle_.empty())
    return;
  GetMediaTransferProtocolManager()->CloseStorage(device_handle_,
                                                  base::Bind(&DoNothing));
}

void MTPDeviceTaskHelper::OnDidOpenStorage(
    const OpenStorageCallback& completion_callback,
    const std::string& device_handle,
    bool error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  device_handle_ = device_handle;
  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(completion_callback, !error));
}

void MTPDeviceTaskHelper::OnGetFileInfo(
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const MtpFileEntry& file_entry,
    bool error) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error) {
    return HandleDeviceError(error_callback,
                             base::PLATFORM_FILE_ERROR_NOT_FOUND);
  }

  base::PlatformFileInfo file_entry_info;
  file_entry_info.size = file_entry.file_size();
  file_entry_info.is_directory =
      file_entry.file_type() == MtpFileEntry::FILE_TYPE_FOLDER;
  file_entry_info.is_symbolic_link = false;
  file_entry_info.last_modified =
      base::Time::FromTimeT(file_entry.modification_time());
  file_entry_info.last_accessed = file_entry_info.last_modified;
  file_entry_info.creation_time = base::Time();
  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(success_callback,
                                              file_entry_info));
}

void MTPDeviceTaskHelper::OnDidReadDirectoryByPath(
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const std::vector<MtpFileEntry>& file_entries,
    bool error) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error)
    return HandleDeviceError(error_callback, base::PLATFORM_FILE_ERROR_FAILED);

  fileapi::AsyncFileUtil::EntryList entries;
  base::FilePath current;
  MTPDeviceObjectEnumerator file_enum(file_entries);
  while (!(current = file_enum.Next()).empty()) {
    fileapi::AsyncFileUtil::Entry entry;
    entry.name = fileapi::VirtualPath::BaseName(current).value();
    entry.is_directory = file_enum.IsDirectory();
    entry.size = file_enum.Size();
    entry.last_modified_time = file_enum.LastModifiedTime();
    entries.push_back(entry);
  }
  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(success_callback, entries));
}

void MTPDeviceTaskHelper::HandleDeviceError(
    const ErrorCallback& error_callback,
    base::PlatformFileError error) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(error_callback, error));
}

}  // namespace chrome
