// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_FILE_SYSTEM_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/file_system_provider/abort_callback.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/watcher.h"
#include "chromeos/dbus/smb_provider_client.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/watcher_manager.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace chromeos {
namespace smb_client {

class RequestManager;

// Smb provided file system implementation. For communication with Smb
// filesystems.
// Smb is an application level protocol used by Windows and Samba fileservers.
// Allows Files App to mount smb filesystems.
class SmbFileSystem : public file_system_provider::ProvidedFileSystemInterface {
 public:
  explicit SmbFileSystem(
      const file_system_provider::ProvidedFileSystemInfo& file_system_info);
  ~SmbFileSystem() override;

  static base::File::Error TranslateError(smbprovider::ErrorType);

  // ProvidedFileSystemInterface overrides.
  file_system_provider::AbortCallback RequestUnmount(
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback GetMetadata(
      const base::FilePath& entry_path,
      ProvidedFileSystemInterface::MetadataFieldMask fields,
      const ProvidedFileSystemInterface::GetMetadataCallback& callback)
      override;

  file_system_provider::AbortCallback GetActions(
      const std::vector<base::FilePath>& entry_paths,
      const GetActionsCallback& callback) override;

  file_system_provider::AbortCallback ExecuteAction(
      const std::vector<base::FilePath>& entry_paths,
      const std::string& action_id,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback ReadDirectory(
      const base::FilePath& directory_path,
      const storage::AsyncFileUtil::ReadDirectoryCallback& callback) override;

  file_system_provider::AbortCallback OpenFile(
      const base::FilePath& file_path,
      file_system_provider::OpenFileMode mode,
      const OpenFileCallback& callback) override;

  file_system_provider::AbortCallback CloseFile(
      int file_handle,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback ReadFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      const ReadChunkReceivedCallback& callback) override;

  file_system_provider::AbortCallback CreateDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback CreateFile(
      const base::FilePath& file_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback CopyEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback MoveEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback Truncate(
      const base::FilePath& file_path,
      int64_t length,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback WriteFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  file_system_provider::AbortCallback AddWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      bool persistent,
      const storage::AsyncFileUtil::StatusCallback& callback,
      const storage::WatcherManager::NotificationCallback&
          notification_callback) override;

  void RemoveWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  const file_system_provider::ProvidedFileSystemInfo& GetFileSystemInfo()
      const override;

  file_system_provider::RequestManager* GetRequestManager() override;

  file_system_provider::Watchers* GetWatchers() override;

  const file_system_provider::OpenedFiles& GetOpenedFiles() const override;

  void AddObserver(
      file_system_provider::ProvidedFileSystemObserver* observer) override;

  void RemoveObserver(
      file_system_provider::ProvidedFileSystemObserver* observer) override;

  void Notify(
      const base::FilePath& entry_path,
      bool recursive,
      storage::WatcherManager::ChangeType change_type,
      std::unique_ptr<file_system_provider::ProvidedFileSystemObserver::Changes>
          changes,
      const std::string& tag,
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  void Configure(
      const storage::AsyncFileUtil::StatusCallback& callback) override;

  base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() override;

 private:
  void HandleRequestUnmountCallback(
      const storage::AsyncFileUtil::StatusCallback& callback,
      smbprovider::ErrorType smb_error) const;

  void HandleRequestReadDirectoryCallback(
      const storage::AsyncFileUtil::ReadDirectoryCallback& callback,
      smbprovider::ErrorType error,
      const smbprovider::DirectoryEntryList& entries) const;

  int32_t GetMountId() const;

  SmbProviderClient* GetSmbProviderClient() const;

  file_system_provider::ProvidedFileSystemInfo file_system_info_;
  file_system_provider::OpenedFiles opened_files_;
  storage::AsyncFileUtil::EntryList entry_list_;
  file_system_provider::Watchers watchers_;

  base::WeakPtrFactory<SmbFileSystem> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SmbFileSystem);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_FILE_SYSTEM_H_
