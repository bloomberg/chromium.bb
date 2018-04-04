// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_file_system.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "net/base/io_buffer.h"

namespace chromeos {

namespace {

// This is a bogus data URI.
// The Files app will attempt to download a whole image to create a thumbnail
// any time you visit a folder. A bug (crbug.com/548050) tracks not doing that
// for NETWORK providers. This work around is to supply an icon but make it
// bogus so it falls back to the generic icon.
constexpr char kUnknownImageDataUri[] = "data:image/png;base64,X";

// Initial number of entries to send during read directory. This number is
// smaller than kReadDirectoryMaxBatchSize since we want the initial page to
// load as quickly as possible.
constexpr uint32_t kReadDirectoryInitialBatchSize = 64;

// Maximum number of entries to send at a time for read directory,
constexpr uint32_t kReadDirectoryMaxBatchSize = 2048;

using file_system_provider::ProvidedFileSystemInterface;

bool RequestedIsDirectory(
    ProvidedFileSystemInterface::MetadataFieldMask fields) {
  return fields & ProvidedFileSystemInterface::MetadataField::
                      METADATA_FIELD_IS_DIRECTORY;
}

bool RequestedName(ProvidedFileSystemInterface::MetadataFieldMask fields) {
  return fields &
         ProvidedFileSystemInterface::MetadataField::METADATA_FIELD_NAME;
}

bool RequestedSize(ProvidedFileSystemInterface::MetadataFieldMask fields) {
  return fields &
         ProvidedFileSystemInterface::MetadataField::METADATA_FIELD_SIZE;
}

bool RequestedModificationTime(
    ProvidedFileSystemInterface::MetadataFieldMask fields) {
  return fields & ProvidedFileSystemInterface::MetadataField::
                      METADATA_FIELD_MODIFICATION_TIME;
}

bool RequestedThumbnail(ProvidedFileSystemInterface::MetadataFieldMask fields) {
  return fields &
         ProvidedFileSystemInterface::MetadataField::METADATA_FIELD_THUMBNAIL;
}

}  // namespace

namespace smb_client {

namespace {

filesystem::mojom::FsFileType MapEntryType(bool is_directory) {
  return is_directory ? filesystem::mojom::FsFileType::DIRECTORY
                      : filesystem::mojom::FsFileType::REGULAR_FILE;
}

constexpr size_t kTaskQueueCapacity = 2;

}  // namespace

using file_system_provider::AbortCallback;

SmbFileSystem::SmbFileSystem(
    const file_system_provider::ProvidedFileSystemInfo& file_system_info,
    UnmountCallback unmount_callback)
    : file_system_info_(file_system_info),
      unmount_callback_(std::move(unmount_callback)),
      task_queue_(kTaskQueueCapacity) {}

SmbFileSystem::~SmbFileSystem() {}

// static
base::File::Error SmbFileSystem::TranslateError(smbprovider::ErrorType error) {
  DCHECK_NE(smbprovider::ERROR_NONE, error);

  switch (error) {
    case smbprovider::ERROR_OK:
      return base::File::FILE_OK;
    case smbprovider::ERROR_FAILED:
      return base::File::FILE_ERROR_FAILED;
    case smbprovider::ERROR_IN_USE:
      return base::File::FILE_ERROR_IN_USE;
    case smbprovider::ERROR_EXISTS:
      return base::File::FILE_ERROR_EXISTS;
    case smbprovider::ERROR_NOT_FOUND:
      return base::File::FILE_ERROR_NOT_FOUND;
    case smbprovider::ERROR_ACCESS_DENIED:
      return base::File::FILE_ERROR_ACCESS_DENIED;
    case smbprovider::ERROR_TOO_MANY_OPENED:
      return base::File::FILE_ERROR_TOO_MANY_OPENED;
    case smbprovider::ERROR_NO_MEMORY:
      return base::File::FILE_ERROR_NO_MEMORY;
    case smbprovider::ERROR_NO_SPACE:
      return base::File::FILE_ERROR_NO_SPACE;
    case smbprovider::ERROR_NOT_A_DIRECTORY:
      return base::File::FILE_ERROR_NOT_A_DIRECTORY;
    case smbprovider::ERROR_INVALID_OPERATION:
      return base::File::FILE_ERROR_INVALID_OPERATION;
    case smbprovider::ERROR_SECURITY:
      return base::File::FILE_ERROR_SECURITY;
    case smbprovider::ERROR_ABORT:
      return base::File::FILE_ERROR_ABORT;
    case smbprovider::ERROR_NOT_A_FILE:
      return base::File::FILE_ERROR_NOT_A_FILE;
    case smbprovider::ERROR_NOT_EMPTY:
      return base::File::FILE_ERROR_NOT_EMPTY;
    case smbprovider::ERROR_INVALID_URL:
      return base::File::FILE_ERROR_INVALID_URL;
    case smbprovider::ERROR_IO:
      return base::File::FILE_ERROR_IO;
    case smbprovider::ERROR_DBUS_PARSE_FAILED:
      // DBUS_PARSE_FAILED maps to generic ERROR_FAILED
      LOG(ERROR) << "DBUS PARSE FAILED";
      return base::File::FILE_ERROR_FAILED;
    default:
      break;
  }

  NOTREACHED();
  return base::File::FILE_ERROR_FAILED;
}

int32_t SmbFileSystem::GetMountId() const {
  int32_t mount_id;
  bool result =
      base::StringToInt(file_system_info_.file_system_id(), &mount_id);
  DCHECK(result);
  return mount_id;
}

SmbProviderClient* SmbFileSystem::GetSmbProviderClient() const {
  return chromeos::DBusThreadManager::Get()->GetSmbProviderClient();
}

base::WeakPtr<SmbProviderClient> SmbFileSystem::GetWeakSmbProviderClient()
    const {
  return GetSmbProviderClient()->AsWeakPtr();
}

void SmbFileSystem::EnqueueTask(SmbTask task, OperationId operation_id) {
  task_queue_.AddTask(std::move(task), operation_id);
}

OperationId SmbFileSystem::EnqueueTaskAndGetOperationId(SmbTask task) {
  OperationId operation_id = task_queue_.GetNextOperationId();
  EnqueueTask(std::move(task), operation_id);
  return operation_id;
}

AbortCallback SmbFileSystem::EnqueueTaskAndGetCallback(SmbTask task) {
  OperationId operation_id = EnqueueTaskAndGetOperationId(std::move(task));
  return CreateAbortCallback(operation_id);
}

void SmbFileSystem::Abort(OperationId operation_id) {
  task_queue_.AbortOperation(operation_id);
}

AbortCallback SmbFileSystem::CreateAbortCallback(OperationId operation_id) {
  return base::BindRepeating(&SmbFileSystem::Abort, AsWeakPtr(), operation_id);
}

AbortCallback SmbFileSystem::CreateAbortCallback() {
  return base::DoNothing();
}

AbortCallback SmbFileSystem::RequestUnmount(
    storage::AsyncFileUtil::StatusCallback callback) {
  auto reply = base::BindOnce(&SmbFileSystem::HandleRequestUnmountCallback,
                              AsWeakPtr(), std::move(callback));
  SmbTask task =
      base::BindOnce(&SmbProviderClient::Unmount, GetWeakSmbProviderClient(),
                     GetMountId(), std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

void SmbFileSystem::HandleRequestUnmountCallback(
    storage::AsyncFileUtil::StatusCallback callback,
    smbprovider::ErrorType error) {
  task_queue_.TaskFinished();
  base::File::Error result = TranslateError(error);
  if (result == base::File::FILE_OK) {
    result =
        RunUnmountCallback(file_system_info_.file_system_id(),
                           file_system_provider::Service::UNMOUNT_REASON_USER);
  }
  std::move(callback).Run(result);
}

AbortCallback SmbFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    ProvidedFileSystemInterface::MetadataFieldMask fields,
    ProvidedFileSystemInterface::GetMetadataCallback callback) {
  auto reply =
      base::BindOnce(&SmbFileSystem::HandleRequestGetMetadataEntryCallback,
                     AsWeakPtr(), fields, callback);
  SmbTask task = base::BindOnce(&SmbProviderClient::GetMetadataEntry,
                                GetWeakSmbProviderClient(), GetMountId(),
                                entry_path, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    GetActionsCallback callback) {
  const std::vector<file_system_provider::Action> actions;
  // No actions are currently supported.
  std::move(callback).Run(actions, base::File::FILE_OK);
  return CreateAbortCallback();
}

AbortCallback SmbFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED();
  return CreateAbortCallback();
}

AbortCallback SmbFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  auto reply =
      base::BindOnce(&SmbFileSystem::HandleRequestReadDirectoryCallback,
                     AsWeakPtr(), callback);
  SmbTask task = base::BindOnce(&SmbProviderClient::ReadDirectory,
                                GetWeakSmbProviderClient(), GetMountId(),
                                directory_path, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::OpenFile(const base::FilePath& file_path,
                                      file_system_provider::OpenFileMode mode,
                                      OpenFileCallback callback) {
  bool writeable =
      mode == file_system_provider::OPEN_FILE_MODE_WRITE ? true : false;

  auto reply = base::BindOnce(&SmbFileSystem::HandleRequestOpenFileCallback,
                              AsWeakPtr(), callback);
  SmbTask task =
      base::BindOnce(&SmbProviderClient::OpenFile, GetWeakSmbProviderClient(),
                     GetMountId(), file_path, writeable, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

void SmbFileSystem::HandleRequestOpenFileCallback(
    OpenFileCallback callback,
    smbprovider::ErrorType error,
    int32_t file_id) const {
  task_queue_.TaskFinished();
  callback.Run(file_id, TranslateError(error));
}

AbortCallback SmbFileSystem::CloseFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback, AsWeakPtr(),
                              std::move(callback));
  SmbTask task =
      base::BindOnce(&SmbProviderClient::CloseFile, GetWeakSmbProviderClient(),
                     GetMountId(), file_handle, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::ReadFile(int file_handle,
                                      net::IOBuffer* buffer,
                                      int64_t offset,
                                      int length,
                                      ReadChunkReceivedCallback callback) {
  auto reply =
      base::BindOnce(&SmbFileSystem::HandleRequestReadFileCallback, AsWeakPtr(),
                     length, scoped_refptr<net::IOBuffer>(buffer), callback);

  SmbTask task = base::BindOnce(&SmbProviderClient::ReadFile,
                                GetWeakSmbProviderClient(), GetMountId(),
                                file_handle, offset, length, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback, AsWeakPtr(),
                              std::move(callback));

  SmbTask task = base::BindOnce(&SmbProviderClient::CreateDirectory,
                                GetWeakSmbProviderClient(), GetMountId(),
                                directory_path, recursive, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback, AsWeakPtr(),
                              std::move(callback));
  SmbTask task =
      base::BindOnce(&SmbProviderClient::CreateFile, GetWeakSmbProviderClient(),
                     GetMountId(), file_path, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  OperationId operation_id = task_queue_.GetNextOperationId();

  auto reply = base::BindOnce(&SmbFileSystem::HandleGetDeleteListCallback,
                              AsWeakPtr(), std::move(callback), operation_id);
  SmbTask task = base::BindOnce(&SmbProviderClient::GetDeleteList,
                                GetWeakSmbProviderClient(), GetMountId(),
                                entry_path, std::move(reply));

  EnqueueTask(std::move(task), operation_id);
  return CreateAbortCallback(operation_id);
}

AbortCallback SmbFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback, AsWeakPtr(),
                              std::move(callback));
  SmbTask task =
      base::BindOnce(&SmbProviderClient::CopyEntry, GetWeakSmbProviderClient(),
                     GetMountId(), source_path, target_path, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback, AsWeakPtr(),
                              std::move(callback));
  SmbTask task =
      base::BindOnce(&SmbProviderClient::MoveEntry, GetWeakSmbProviderClient(),
                     GetMountId(), source_path, target_path, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback, AsWeakPtr(),
                              std::move(callback));
  SmbTask task =
      base::BindOnce(&SmbProviderClient::Truncate, GetWeakSmbProviderClient(),
                     GetMountId(), file_path, length, std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    storage::AsyncFileUtil::StatusCallback callback) {
  const std::vector<uint8_t> data(buffer->data(), buffer->data() + length);
  base::ScopedFD temp_fd = temp_file_manager_.CreateTempFile(data);

  auto reply = base::BindOnce(&SmbFileSystem::HandleStatusCallback, AsWeakPtr(),
                              std::move(callback));
  SmbTask task = base::BindOnce(
      &SmbProviderClient::WriteFile, GetWeakSmbProviderClient(), GetMountId(),
      file_handle, offset, length, std::move(temp_fd), std::move(reply));

  return EnqueueTaskAndGetCallback(std::move(task));
}

AbortCallback SmbFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    const storage::WatcherManager::NotificationCallback&
        notification_callback) {
  // Watchers are not supported.
  NOTREACHED();
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
  return CreateAbortCallback();
}

void SmbFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  // Watchers are not supported.
  NOTREACHED();
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

const file_system_provider::ProvidedFileSystemInfo&
SmbFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

file_system_provider::RequestManager* SmbFileSystem::GetRequestManager() {
  NOTREACHED();
  return NULL;
}

file_system_provider::Watchers* SmbFileSystem::GetWatchers() {
  // Watchers are not supported.
  NOTREACHED();
  return nullptr;
}

const file_system_provider::OpenedFiles& SmbFileSystem::GetOpenedFiles() const {
  NOTREACHED();
  return opened_files_;
}

void SmbFileSystem::AddObserver(
    file_system_provider::ProvidedFileSystemObserver* observer) {
  NOTREACHED();
}

void SmbFileSystem::RemoveObserver(
    file_system_provider::ProvidedFileSystemObserver* observer) {
  NOTREACHED();
}

void SmbFileSystem::SmbFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<file_system_provider::ProvidedFileSystemObserver::Changes>
        changes,
    const std::string& tag,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED();
}

void SmbFileSystem::Configure(storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED();
}

void SmbFileSystem::HandleRequestReadDirectoryCallback(
    storage::AsyncFileUtil::ReadDirectoryCallback callback,
    smbprovider::ErrorType error,
    const smbprovider::DirectoryEntryListProto& entries) const {
  task_queue_.TaskFinished();
  uint32_t batch_size = kReadDirectoryInitialBatchSize;
  storage::AsyncFileUtil::EntryList entry_list;

  // Loop through the entries and send when the desired batch size is hit.
  for (const smbprovider::DirectoryEntryProto& entry : entries.entries()) {
    entry_list.emplace_back(base::FilePath(entry.name()),
                            MapEntryType(entry.is_directory()));

    if (entry_list.size() == batch_size) {
      callback.Run(base::File::FILE_OK, entry_list, true /* has_more */);
      entry_list.clear();

      // Double the batch size until it gets to the maximum size.
      batch_size = std::min(batch_size * 2, kReadDirectoryMaxBatchSize);
    }
  }

  callback.Run(TranslateError(error), entry_list, false /* has_more */);
}

void SmbFileSystem::HandleGetDeleteListCallback(
    storage::AsyncFileUtil::StatusCallback callback,
    OperationId operation_id,
    smbprovider::ErrorType list_error,
    const smbprovider::DeleteListProto& delete_list) {
  task_queue_.TaskFinished();
  if (delete_list.entries_size() == 0) {
    // There are no entries to delete.
    DCHECK_NE(smbprovider::ERROR_OK, list_error);
    std::move(callback).Run(TranslateError(list_error));
    return;
  }

  for (int i = 0; i < delete_list.entries_size(); ++i) {
    const base::FilePath entry_path(delete_list.entries(i));
    bool is_last_entry = (i == delete_list.entries_size() - 1);

    auto reply =
        base::BindOnce(&SmbFileSystem::HandleDeleteEntryCallback, AsWeakPtr(),
                       std::move(callback), list_error, is_last_entry);

    SmbTask task = base::BindOnce(
        &SmbProviderClient::DeleteEntry, GetWeakSmbProviderClient(),
        GetMountId(), entry_path, false /* recursive */, std::move(reply));
    EnqueueTask(std::move(task), operation_id);
  }
}

void SmbFileSystem::HandleDeleteEntryCallback(
    storage::AsyncFileUtil::StatusCallback callback,
    smbprovider::ErrorType list_error,
    bool is_last_entry,
    smbprovider::ErrorType delete_error) const {
  task_queue_.TaskFinished();
  if (is_last_entry) {
    // Only run the callback once.
    if (list_error != smbprovider::ERROR_OK) {
      delete_error = list_error;
    }
    std::move(callback).Run(TranslateError(delete_error));
  }
}

void SmbFileSystem::HandleRequestGetMetadataEntryCallback(
    ProvidedFileSystemInterface::MetadataFieldMask fields,
    ProvidedFileSystemInterface::GetMetadataCallback callback,
    smbprovider::ErrorType error,
    const smbprovider::DirectoryEntryProto& entry) const {
  task_queue_.TaskFinished();
  if (error != smbprovider::ERROR_OK) {
    std::move(callback).Run(nullptr, TranslateError(error));
    return;
  }
  std::unique_ptr<file_system_provider::EntryMetadata> metadata =
      std::make_unique<file_system_provider::EntryMetadata>();
  if (RequestedIsDirectory(fields)) {
    metadata->is_directory = std::make_unique<bool>(entry.is_directory());
  }
  if (RequestedName(fields)) {
    metadata->name = std::make_unique<std::string>(entry.name());
  }
  if (RequestedSize(fields)) {
    metadata->size = std::make_unique<int64_t>(entry.size());
  }
  if (RequestedModificationTime(fields)) {
    metadata->modification_time = std::make_unique<base::Time>(
        base::Time::FromTimeT(entry.last_modified_time()));
  }
  if (RequestedThumbnail(fields)) {
    metadata->thumbnail = std::make_unique<std::string>(kUnknownImageDataUri);
  }
  // Mime types are not supported.
  std::move(callback).Run(std::move(metadata), base::File::FILE_OK);
}

base::File::Error SmbFileSystem::RunUnmountCallback(
    const std::string& file_system_id,
    file_system_provider::Service::UnmountReason reason) {
  base::File::Error error =
      std::move(unmount_callback_).Run(file_system_id, reason);
  return error;
}

void SmbFileSystem::HandleRequestReadFileCallback(
    int32_t length,
    scoped_refptr<net::IOBuffer> buffer,
    ReadChunkReceivedCallback callback,
    smbprovider::ErrorType error,
    const base::ScopedFD& fd) const {
  task_queue_.TaskFinished();

  if (error != smbprovider::ERROR_OK) {
    callback.Run(0 /* chunk_length */, false /* has_more */,
                 TranslateError(error));
    return;
  }

  int32_t total_read = 0;
  while (total_read < length) {
    // read() may return less than the requested amount of bytes. If this
    // happens, try to read the remaining bytes.
    const int32_t bytes_read = HANDLE_EINTR(
        read(fd.get(), buffer->data() + total_read, length - total_read));
    if (bytes_read < 0) {
      // This is an error case, return an error immediately.
      callback.Run(0 /* chunk_length */, false /* has_more */,
                   base::File::FILE_ERROR_IO);
      return;
    }
    if (bytes_read == 0) {
      break;
    }
    total_read += bytes_read;
  }
  callback.Run(total_read, false /* has_more */, base::File::FILE_OK);
}

void SmbFileSystem::HandleStatusCallback(
    storage::AsyncFileUtil::StatusCallback callback,
    smbprovider::ErrorType error) const {
  task_queue_.TaskFinished();

  std::move(callback).Run(TranslateError(error));
}

base::WeakPtr<file_system_provider::ProvidedFileSystemInterface>
SmbFileSystem::GetWeakPtr() {
  return AsWeakPtr();
}

}  // namespace smb_client
}  // namespace chromeos
