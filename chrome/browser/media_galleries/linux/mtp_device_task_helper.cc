// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/linux/mtp_device_task_helper.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media_galleries/linux/mtp_device_object_enumerator.h"
#include "chrome/browser/media_galleries/linux/mtp_read_file_worker.h"
#include "chrome/browser/media_galleries/linux/snapshot_file_details.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "net/base/io_buffer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "webkit/browser/fileapi/async_file_util.h"
#include "webkit/common/fileapi/file_system_util.h"

using storage_monitor::StorageMonitor;

namespace {

// Does nothing.
// This method is used to handle the results of
// MediaTransferProtocolManager::CloseStorage method call.
void DoNothing(bool error) {
}

device::MediaTransferProtocolManager* GetMediaTransferProtocolManager() {
  return StorageMonitor::GetInstance()->media_transfer_protocol_manager();
}

base::File::Info FileInfoFromMTPFileEntry(const MtpFileEntry& file_entry) {
  base::File::Info file_entry_info;
  file_entry_info.size = file_entry.file_size();
  file_entry_info.is_directory =
      file_entry.file_type() == MtpFileEntry::FILE_TYPE_FOLDER;
  file_entry_info.is_symbolic_link = false;
  file_entry_info.last_modified =
      base::Time::FromTimeT(file_entry.modification_time());
  file_entry_info.last_accessed = file_entry_info.last_modified;
  file_entry_info.creation_time = base::Time();
  return file_entry_info;
}

}  // namespace

MTPDeviceTaskHelper::MTPDeviceTaskHelper()
    : weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MTPDeviceTaskHelper::~MTPDeviceTaskHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MTPDeviceTaskHelper::OpenStorage(const std::string& storage_name,
                                      const OpenStorageCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

void MTPDeviceTaskHelper::GetFileInfoById(
    uint32 file_id,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->GetFileInfoById(
      device_handle_, file_id,
      base::Bind(&MTPDeviceTaskHelper::OnGetFileInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::ReadDirectoryById(
    uint32 dir_id,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->ReadDirectoryById(
      device_handle_, dir_id,
      base::Bind(&MTPDeviceTaskHelper::OnDidReadDirectoryById,
                 weak_ptr_factory_.GetWeakPtr(),
                 success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::WriteDataIntoSnapshotFile(
    const SnapshotRequestInfo& request_info,
    const base::File::Info& snapshot_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty()) {
    return HandleDeviceError(request_info.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  if (!read_file_worker_)
    read_file_worker_.reset(new MTPReadFileWorker(device_handle_));
  read_file_worker_->WriteDataIntoSnapshotFile(request_info,
                                               snapshot_file_info);
}

void MTPDeviceTaskHelper::ReadBytes(
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty()) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  GetMediaTransferProtocolManager()->GetFileInfoById(
      device_handle_, request.file_id,
      base::Bind(&MTPDeviceTaskHelper::OnGetFileInfoToReadBytes,
                 weak_ptr_factory_.GetWeakPtr(), request));
}

void MTPDeviceTaskHelper::CloseStorage() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return;
  GetMediaTransferProtocolManager()->CloseStorage(device_handle_,
                                                  base::Bind(&DoNothing));
}

void MTPDeviceTaskHelper::OnDidOpenStorage(
    const OpenStorageCallback& completion_callback,
    const std::string& device_handle,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    return HandleDeviceError(error_callback,
                             base::File::FILE_ERROR_NOT_FOUND);
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(success_callback, FileInfoFromMTPFileEntry(file_entry)));
}

void MTPDeviceTaskHelper::OnDidReadDirectoryById(
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const std::vector<MtpFileEntry>& file_entries,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error)
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  fileapi::AsyncFileUtil::EntryList entries;
  base::FilePath current;
  MTPDeviceObjectEnumerator file_enum(file_entries);
  while (!(current = file_enum.Next()).empty()) {
    fileapi::DirectoryEntry entry;
    entry.name = fileapi::VirtualPath::BaseName(current).value();
    uint32 file_id = 0;
    bool ret = file_enum.GetEntryId(&file_id);
    DCHECK(ret);
    entry.name.push_back(',');
    entry.name += base::UintToString(file_id);
    entry.is_directory = file_enum.IsDirectory();
    entry.size = file_enum.Size();
    entry.last_modified_time = file_enum.LastModifiedTime();
    entries.push_back(entry);
  }
  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(success_callback, entries));
}

void MTPDeviceTaskHelper::OnGetFileInfoToReadBytes(
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request,
    const MtpFileEntry& file_entry,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(request.buf);
  DCHECK_GE(request.buf_len, 0);
  DCHECK_GE(request.offset, 0);
  if (error) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  base::File::Info file_info = FileInfoFromMTPFileEntry(file_entry);
  if (file_info.is_directory) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_NOT_A_FILE);
  } else if (file_info.size < 0 || file_info.size > kuint32max ||
             request.offset > file_info.size) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  } else if (request.offset == file_info.size) {
    content::BrowserThread::PostTask(content::BrowserThread::IO,
                                     FROM_HERE,
                                     base::Bind(request.success_callback,
                                                file_info, 0u));
    return;
  }

  uint32 bytes_to_read = std::min(
      base::checked_cast<uint32>(request.buf_len),
      base::saturated_cast<uint32>(file_info.size - request.offset));

  GetMediaTransferProtocolManager()->ReadFileChunkById(
      device_handle_,
      request.file_id,
      base::checked_cast<uint32>(request.offset),
      bytes_to_read,
      base::Bind(&MTPDeviceTaskHelper::OnDidReadBytes,
                 weak_ptr_factory_.GetWeakPtr(), request, file_info));
}

void MTPDeviceTaskHelper::OnDidReadBytes(
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request,
    const base::File::Info& file_info,
    const std::string& data,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  CHECK_LE(base::checked_cast<int>(data.length()), request.buf_len);
  std::copy(data.begin(), data.end(), request.buf->data());

  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(request.success_callback,
                                              file_info, data.length()));
}

void MTPDeviceTaskHelper::HandleDeviceError(
    const ErrorCallback& error_callback,
    base::File::Error error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(content::BrowserThread::IO,
                                   FROM_HERE,
                                   base::Bind(error_callback, error));
}
