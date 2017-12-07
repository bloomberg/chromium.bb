// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_file_system.h"

#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/smb_provider_client.h"

namespace chromeos {
namespace smb_client {

using file_system_provider::AbortCallback;

SmbFileSystem::SmbFileSystem(
    const file_system_provider::ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info), weak_ptr_factory_(this) {}

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

AbortCallback SmbFileSystem::RequestUnmount(
    const storage::AsyncFileUtil::StatusCallback& callback) {
  GetSmbProviderClient()->Unmount(
      GetMountId(), base::BindOnce(&SmbFileSystem::HandleRequestUnmountCallback,
                                   weak_ptr_factory_.GetWeakPtr(), callback));
  return AbortCallback();
}

void SmbFileSystem::HandleRequestUnmountCallback(
    const storage::AsyncFileUtil::StatusCallback& callback,
    smbprovider::ErrorType smb_error) const {
  callback.Run(TranslateError(smb_error));
}

AbortCallback SmbFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    ProvidedFileSystemInterface::MetadataFieldMask fields,
    const ProvidedFileSystemInterface::GetMetadataCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    const GetActionsCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::OpenFile(const base::FilePath& file_path,
                                      file_system_provider::OpenFileMode mode,
                                      const OpenFileCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::CloseFile(
    int file_handle,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::ReadFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    const ReadChunkReceivedCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::CreateFile(
    const base::FilePath& file_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

AbortCallback SmbFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    const storage::AsyncFileUtil::StatusCallback& callback,
    const storage::WatcherManager::NotificationCallback&
        notification_callback) {
  NOTIMPLEMENTED();
  return AbortCallback();
}

void SmbFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
}

const file_system_provider::ProvidedFileSystemInfo&
SmbFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

file_system_provider::RequestManager* SmbFileSystem::GetRequestManager() {
  NOTIMPLEMENTED();
  return NULL;
}

file_system_provider::Watchers* SmbFileSystem::GetWatchers() {
  NOTIMPLEMENTED();
  return &watchers_;
}

const file_system_provider::OpenedFiles& SmbFileSystem::GetOpenedFiles() const {
  NOTIMPLEMENTED();
  return opened_files_;
}

void SmbFileSystem::AddObserver(
    file_system_provider::ProvidedFileSystemObserver* observer) {
  NOTIMPLEMENTED();
}

void SmbFileSystem::RemoveObserver(
    file_system_provider::ProvidedFileSystemObserver* observer) {
  NOTIMPLEMENTED();
}

void SmbFileSystem::SmbFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<file_system_provider::ProvidedFileSystemObserver::Changes>
        changes,
    const std::string& tag,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
}

void SmbFileSystem::Configure(
    const storage::AsyncFileUtil::StatusCallback& callback) {
  NOTIMPLEMENTED();
}

base::WeakPtr<file_system_provider::ProvidedFileSystemInterface>
SmbFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace smb_client
}  // namespace chromeos
