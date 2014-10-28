// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "storage/browser/fileapi/async_file_util.h"

class Profile;

namespace net {
class IOBuffer;
}  // namespace net

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {

class NotificationManagerInterface;

// Automatically calls the |update_callback| after all of the callbacks created
// with |CreateCallback| are called.
//
// It's used to update tags of observed entries once a notification about a
// change are fully handles. It is to make sure that the change notification is
// fully handled before remembering the new tag.
//
// It is necessary to update the tag after all observers handle it fully, so
// in case of shutdown or a crash we get the notifications again.
class AutoUpdater : public base::RefCounted<AutoUpdater> {
 public:
  explicit AutoUpdater(const base::Closure& update_callback);

  // Creates a new callback which needs to be called before the update callback
  // is called.
  base::Closure CreateCallback();

 private:
  friend class base::RefCounted<AutoUpdater>;

  // Called once the callback created with |CreateCallback| is executed. Once
  // all of such callbacks are called, then the update callback is invoked.
  void OnPendingCallback();

  virtual ~AutoUpdater();

  base::Closure update_callback_;
  int created_callbacks_;
  int pending_callbacks_;
};

// Provided file system implementation. Forwards requests between providers and
// clients.
class ProvidedFileSystem : public ProvidedFileSystemInterface {
 public:
  ProvidedFileSystem(Profile* profile,
                     const ProvidedFileSystemInfo& file_system_info);
  virtual ~ProvidedFileSystem();

  // Sets a custom event router. Used in unit tests to mock out the real
  // extension.
  void SetEventRouterForTesting(extensions::EventRouter* event_router);

  // Sets a custom notification manager. It will recreate the request manager,
  // so is must be called just after creating ProvideFileSystem instance.
  // Used by unit tests.
  void SetNotificationManagerForTesting(
      scoped_ptr<NotificationManagerInterface> notification_manager);

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
  virtual AbortCallback ObserveDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual void UnobserveEntry(
      const base::FilePath& entry_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback) override;
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const override;
  virtual RequestManager* GetRequestManager() override;
  virtual ObservedEntries* GetObservedEntries() override;
  virtual void AddObserver(ProvidedFileSystemObserver* observer) override;
  virtual void RemoveObserver(ProvidedFileSystemObserver* observer) override;
  virtual bool Notify(const base::FilePath& observed_path,
                      bool recursive,
                      ProvidedFileSystemObserver::ChangeType change_type,
                      scoped_ptr<ProvidedFileSystemObserver::Changes> changes,
                      const std::string& tag) override;
  virtual base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() override;

 private:
  // Aborts an operation executed with a request id equal to
  // |operation_request_id|. The request is removed immediately on the C++ side
  // despite being handled by the providing extension or not.
  void Abort(int operation_request_id,
             const storage::AsyncFileUtil::StatusCallback& callback);

  // Called when a directory becomes watched successfully.
  void OnObserveDirectoryCompleted(
      const base::FilePath& directory_path,
      bool recursive,
      const storage::AsyncFileUtil::StatusCallback& callback,
      base::File::Error result);

  // Called when all observers finished handling the change notification. It
  // updates the tag from |last_tag| to |tag| for the entry at |observed_path|.
  void OnNotifyCompleted(
      const base::FilePath& observed_path,
      bool recursive,
      ProvidedFileSystemObserver::ChangeType change_type,
      scoped_ptr<ProvidedFileSystemObserver::Changes> changes,
      const std::string& last_tag,
      const std::string& tag);

  Profile* profile_;                       // Not owned.
  extensions::EventRouter* event_router_;  // Not owned. May be NULL.
  ProvidedFileSystemInfo file_system_info_;
  scoped_ptr<NotificationManagerInterface> notification_manager_;
  scoped_ptr<RequestManager> request_manager_;
  ObservedEntries observed_entries_;
  ObserverList<ProvidedFileSystemObserver> observers_;

  base::WeakPtrFactory<ProvidedFileSystem> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ProvidedFileSystem);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
