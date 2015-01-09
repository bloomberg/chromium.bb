// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_THROTTLED_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_THROTTLED_FILE_SYSTEM_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/abort_callback.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_observer.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/watcher_manager.h"
#include "url/gurl.h"

class Profile;

namespace net {
class IOBuffer;
}  // namespace net

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace file_system_provider {

class Queue;
class RequestManager;

// Decorates ProvidedFileSystemInterface with throttling capabilities.
class ThrottledFileSystem : public ProvidedFileSystemInterface {
 public:
  explicit ThrottledFileSystem(
      scoped_ptr<ProvidedFileSystemInterface> file_system);
  virtual ~ThrottledFileSystem();

  // ProvidedFileSystemInterface overrides.
  virtual AbortCallback RequestUnmount(
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback GetMetadata(
      const base::FilePath& entry_path,
      MetadataFieldMask fields,
      const GetMetadataCallback& callback) override;
  virtual AbortCallback ReadDirectory(
      const base::FilePath& directory_path,
      const storage::AsyncFileUtil::ReadDirectoryCallback& callback) override;
  virtual AbortCallback OpenFile(const base::FilePath& file_path,
                                 OpenFileMode mode,
                                 const OpenFileCallback& callback) override;
  virtual AbortCallback CloseFile(
      int file_handle,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback ReadFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64 offset,
      int length,
      const ReadChunkReceivedCallback& callback) override;
  virtual AbortCallback CreateDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback CreateFile(
      const base::FilePath& file_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback CopyEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback MoveEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback Truncate(
      const base::FilePath& file_path,
      int64 length,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback WriteFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64 offset,
      int length,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual AbortCallback AddWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      bool persistent,
      const storage::AsyncFileUtil::StatusCallback& callback,
      const storage::WatcherManager::NotificationCallback&
          notification_callback) override;
  virtual void RemoveWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const override;
  virtual RequestManager* GetRequestManager() override;
  virtual Watchers* GetWatchers() override;
  virtual void AddObserver(ProvidedFileSystemObserver* observer) override;
  virtual void RemoveObserver(ProvidedFileSystemObserver* observer) override;
  virtual void Notify(
      const base::FilePath& entry_path,
      bool recursive,
      storage::WatcherManager::ChangeType change_type,
      scoped_ptr<ProvidedFileSystemObserver::Changes> changes,
      const std::string& tag,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() override;

 private:
  // Called when opening a file is completed with either a success or an error.
  void OnOpenFileCompleted(int queue_token,
                           const OpenFileCallback& callback,
                           int file_handle,
                           base::File::Error result);

  // Called when closing a file is completed with either a success or an error.
  void OnCloseFileCompleted(
      int file_handle,
      const storage::AsyncFileUtil::StatusCallback& callback,
      base::File::Error result);

  scoped_ptr<ProvidedFileSystemInterface> file_system_;
  scoped_ptr<Queue> open_queue_;

  // Map from file handles to open queue tokens.
  std::map<int, int> opened_files_;

  base::WeakPtrFactory<ThrottledFileSystem> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ThrottledFileSystem);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_THROTTLED_FILE_SYSTEM_H_
