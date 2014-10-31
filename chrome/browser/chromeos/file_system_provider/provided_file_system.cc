// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include <vector>

#include "base/debug/trace_event.h"
#include "base/files/file.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager.h"
#include "chrome/browser/chromeos/file_system_provider/operations/abort.h"
#include "chrome/browser/chromeos/file_system_provider/operations/add_watcher.h"
#include "chrome/browser/chromeos/file_system_provider/operations/close_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/copy_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/create_directory.h"
#include "chrome/browser/chromeos/file_system_provider/operations/create_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/delete_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/get_metadata.h"
#include "chrome/browser/chromeos/file_system_provider/operations/move_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/open_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/read_directory.h"
#include "chrome/browser/chromeos/file_system_provider/operations/read_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/remove_watcher.h"
#include "chrome/browser/chromeos/file_system_provider/operations/truncate.h"
#include "chrome/browser/chromeos/file_system_provider/operations/unmount.h"
#include "chrome/browser/chromeos/file_system_provider/operations/write_file.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider.h"
#include "extensions/browser/event_router.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace chromeos {
namespace file_system_provider {
namespace {

// Discards the result of Abort() when called from the destructor.
void EmptyStatusCallback(base::File::Error /* result */) {
}

// Discards the error code and always calls the callback with a success.
void AlwaysSuccessCallback(
    const storage::AsyncFileUtil::StatusCallback& callback,
    base::File::Error /* result */) {
  callback.Run(base::File::FILE_OK);
}

}  // namespace

AutoUpdater::AutoUpdater(const base::Closure& update_callback)
    : update_callback_(update_callback),
      created_callbacks_(0),
      pending_callbacks_(0) {
}

base::Closure AutoUpdater::CreateCallback() {
  ++created_callbacks_;
  ++pending_callbacks_;
  return base::Bind(&AutoUpdater::OnPendingCallback, this);
}

void AutoUpdater::OnPendingCallback() {
  DCHECK_LT(0, pending_callbacks_);
  if (--pending_callbacks_ == 0)
    update_callback_.Run();
}

AutoUpdater::~AutoUpdater() {
  // If no callbacks are created, then we need to invoke updating in the
  // destructor.
  if (!created_callbacks_)
    update_callback_.Run();
  else if (pending_callbacks_)
    LOG(ERROR) << "Not all callbacks called. This may happen on shutdown.";
}

ProvidedFileSystem::ProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info)
    : profile_(profile),
      event_router_(extensions::EventRouter::Get(profile)),  // May be NULL.
      file_system_info_(file_system_info),
      notification_manager_(
          new NotificationManager(profile_, file_system_info_)),
      request_manager_(new RequestManager(notification_manager_.get())),
      weak_ptr_factory_(this) {
}

ProvidedFileSystem::~ProvidedFileSystem() {
  const std::vector<int> request_ids = request_manager_->GetActiveRequestIds();
  for (size_t i = 0; i < request_ids.size(); ++i) {
    Abort(request_ids[i], base::Bind(&EmptyStatusCallback));
  }
}

void ProvidedFileSystem::SetEventRouterForTesting(
    extensions::EventRouter* event_router) {
  event_router_ = event_router;
}

void ProvidedFileSystem::SetNotificationManagerForTesting(
    scoped_ptr<NotificationManagerInterface> notification_manager) {
  notification_manager_ = notification_manager.Pass();
  request_manager_.reset(new RequestManager(notification_manager_.get()));
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::RequestUnmount(
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      REQUEST_UNMOUNT,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::Unmount(event_router_, file_system_info_, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    MetadataFieldMask fields,
    const GetMetadataCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      GET_METADATA,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::GetMetadata(
          event_router_, file_system_info_, entry_path, fields, callback)));
  if (!request_id) {
    callback.Run(make_scoped_ptr<EntryMetadata>(NULL),
                 base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    const storage::AsyncFileUtil::ReadDirectoryCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      READ_DIRECTORY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::ReadDirectory(
              event_router_, file_system_info_, directory_path, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY,
                 storage::AsyncFileUtil::EntryList(),
                 false /* has_more */);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::ReadFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64 offset,
    int length,
    const ReadChunkReceivedCallback& callback) {
  TRACE_EVENT1(
      "file_system_provider", "ProvidedFileSystem::ReadFile", "length", length);
  const int request_id = request_manager_->CreateRequest(
      READ_FILE,
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new operations::ReadFile(event_router_,
                                   file_system_info_,
                                   file_handle,
                                   buffer,
                                   offset,
                                   length,
                                   callback)));
  if (!request_id) {
    callback.Run(0 /* chunk_length */,
                 false /* has_more */,
                 base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::OpenFile(
    const base::FilePath& file_path,
    OpenFileMode mode,
    const OpenFileCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      OPEN_FILE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::OpenFile(
          event_router_, file_system_info_, file_path, mode, callback)));
  if (!request_id) {
    callback.Run(0 /* file_handle */, base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CloseFile(
    int file_handle,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      CLOSE_FILE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::CloseFile(
          event_router_, file_system_info_, file_handle, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      CREATE_DIRECTORY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::CreateDirectory(event_router_,
                                          file_system_info_,
                                          directory_path,
                                          recursive,
                                          callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      DELETE_ENTRY,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::DeleteEntry(
          event_router_, file_system_info_, entry_path, recursive, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CreateFile(
    const base::FilePath& file_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      CREATE_FILE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::CreateFile(
          event_router_, file_system_info_, file_path, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      COPY_ENTRY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::CopyEntry(event_router_,
                                    file_system_info_,
                                    source_path,
                                    target_path,
                                    callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64 offset,
    int length,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  TRACE_EVENT1("file_system_provider",
               "ProvidedFileSystem::WriteFile",
               "length",
               length);
  const int request_id = request_manager_->CreateRequest(
      WRITE_FILE,
      make_scoped_ptr<RequestManager::HandlerInterface>(
          new operations::WriteFile(event_router_,
                                    file_system_info_,
                                    file_handle,
                                    make_scoped_refptr(buffer),
                                    offset,
                                    length,
                                    callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      MOVE_ENTRY,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::MoveEntry(event_router_,
                                    file_system_info_,
                                    source_path,
                                    target_path,
                                    callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::Truncate(
    const base::FilePath& file_path,
    int64 length,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const int request_id = request_manager_->CreateRequest(
      TRUNCATE,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::Truncate(
          event_router_, file_system_info_, file_path, length, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

ProvidedFileSystem::AbortCallback ProvidedFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    const storage::AsyncFileUtil::StatusCallback& callback,
    const storage::WatcherManager::NotificationCallback&
        notification_callback) {
  // TODO(mtomasz): Wrap the entire method body with an asynchronous queue to
  // avoid races.
  if (persistent && (!file_system_info_.supports_notify_tag() ||
                     !notification_callback.is_null())) {
    OnAddWatcherCompleted(entry_path,
                          recursive,
                          Subscriber(),
                          callback,
                          base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }

  // Create a candidate subscriber. This could be done in OnAddWatcherCompleted,
  // but base::Bind supports only up to 7 arguments.
  Subscriber subscriber;
  subscriber.origin = origin;
  subscriber.persistent = persistent;
  subscriber.notification_callback = notification_callback;

  const WatcherKey key(entry_path, recursive);
  const Watchers::const_iterator it = watchers_.find(key);
  if (it != watchers_.end()) {
    const bool exists =
        it->second.subscribers.find(origin) != it->second.subscribers.end();
    OnAddWatcherCompleted(
        entry_path,
        recursive,
        subscriber,
        callback,
        exists ? base::File::FILE_ERROR_EXISTS : base::File::FILE_OK);
    return AbortCallback();
  }

  const int request_id = request_manager_->CreateRequest(
      ADD_WATCHER,
      scoped_ptr<RequestManager::HandlerInterface>(new operations::AddWatcher(
          event_router_,
          file_system_info_,
          entry_path,
          recursive,
          base::Bind(&ProvidedFileSystem::OnAddWatcherCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     entry_path,
                     recursive,
                     subscriber,
                     callback))));

  if (!request_id) {
    OnAddWatcherCompleted(entry_path,
                          recursive,
                          subscriber,
                          callback,
                          base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

void ProvidedFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  const WatcherKey key(entry_path, recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it == watchers_.end() ||
      it->second.subscribers.find(origin) == it->second.subscribers.end()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Delete the subscriber in advance, since the list of watchers is owned by
  // the C++ layer, not by the extension.
  it->second.subscribers.erase(origin);

  FOR_EACH_OBSERVER(ProvidedFileSystemObserver,
                    observers_,
                    OnWatcherListChanged(file_system_info_, watchers_));

  // If there are other subscribers, then do not remove the obsererver, but
  // simply return a success.
  if (it->second.subscribers.size()) {
    callback.Run(base::File::FILE_OK);
    return;
  }

  // Delete the watcher in advance.
  watchers_.erase(it);

  // Even if the extension returns an error, the callback is called with base::
  // File::FILE_OK. The reason for that is that the entry is not watche anymore
  // anyway, as it's removed in advance.
  const int request_id = request_manager_->CreateRequest(
      REMOVE_WATCHER,
      scoped_ptr<RequestManager::HandlerInterface>(
          new operations::RemoveWatcher(
              event_router_,
              file_system_info_,
              entry_path,
              recursive,
              base::Bind(&AlwaysSuccessCallback, callback))));
  if (!request_id)
    callback.Run(base::File::FILE_OK);
}

const ProvidedFileSystemInfo& ProvidedFileSystem::GetFileSystemInfo() const {
  return file_system_info_;
}

RequestManager* ProvidedFileSystem::GetRequestManager() {
  return request_manager_.get();
}

Watchers* ProvidedFileSystem::GetWatchers() {
  return &watchers_;
}

void ProvidedFileSystem::AddObserver(ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ProvidedFileSystem::RemoveObserver(ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool ProvidedFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    scoped_ptr<ProvidedFileSystemObserver::Changes> changes,
    const std::string& tag) {
  const WatcherKey key(entry_path, recursive);
  const auto& watcher_it = watchers_.find(key);
  if (watcher_it == watchers_.end())
    return false;

  // The tag must be provided if and only if it's explicitly supported.
  if (file_system_info_.supports_notify_tag() == tag.empty())
    return false;

  // The object is owned by AutoUpdated, so the reference is valid as long as
  // callbacks created with AutoUpdater::CreateCallback().
  const ProvidedFileSystemObserver::Changes& changes_ref = *changes.get();

  scoped_refptr<AutoUpdater> auto_updater(
      new AutoUpdater(base::Bind(&ProvidedFileSystem::OnNotifyCompleted,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 entry_path,
                                 recursive,
                                 change_type,
                                 base::Passed(&changes),
                                 watcher_it->second.last_tag,
                                 tag)));

  // Call all notification callbacks (if any).
  for (const auto& subscriber_it : watcher_it->second.subscribers) {
    const storage::WatcherManager::NotificationCallback& notification_callback =
        subscriber_it.second.notification_callback;
    if (!notification_callback.is_null())
      notification_callback.Run(change_type);
  }

  // Notify all observers.
  FOR_EACH_OBSERVER(ProvidedFileSystemObserver,
                    observers_,
                    OnWatcherChanged(file_system_info_,
                                     watcher_it->second,
                                     change_type,
                                     changes_ref,
                                     auto_updater->CreateCallback()));

  return true;
}

base::WeakPtr<ProvidedFileSystemInterface> ProvidedFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProvidedFileSystem::Abort(
    int operation_request_id,
    const storage::AsyncFileUtil::StatusCallback& callback) {
  request_manager_->RejectRequest(operation_request_id,
                                  make_scoped_ptr(new RequestValue()),
                                  base::File::FILE_ERROR_ABORT);
  if (!request_manager_->CreateRequest(
          ABORT,
          scoped_ptr<RequestManager::HandlerInterface>(
              new operations::Abort(event_router_,
                                    file_system_info_,
                                    operation_request_id,
                                    callback)))) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
  }
}

void ProvidedFileSystem::OnAddWatcherCompleted(
    const base::FilePath& entry_path,
    bool recursive,
    const Subscriber& subscriber,
    const storage::AsyncFileUtil::StatusCallback& callback,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    callback.Run(result);
    return;
  }

  const WatcherKey key(entry_path, recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it != watchers_.end()) {
    callback.Run(base::File::FILE_OK);
    return;
  }

  // TODO(mtomasz): Add a queue to prevent races.
  Watcher* const watcher = &watchers_[key];
  watcher->entry_path = entry_path;
  watcher->recursive = recursive;
  watcher->subscribers[subscriber.origin] = subscriber;

  FOR_EACH_OBSERVER(ProvidedFileSystemObserver,
                    observers_,
                    OnWatcherListChanged(file_system_info_, watchers_));

  callback.Run(base::File::FILE_OK);
}

void ProvidedFileSystem::OnNotifyCompleted(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    scoped_ptr<ProvidedFileSystemObserver::Changes> /* changes */,
    const std::string& last_tag,
    const std::string& tag) {
  const WatcherKey key(entry_path, recursive);
  const Watchers::iterator it = watchers_.find(key);
  // Check if the entry is still watched.
  if (it == watchers_.end())
    return;

  // Another following notification finished earlier.
  if (it->second.last_tag != last_tag)
    return;

  // It's illegal to provide a tag which is not unique. As for now only an error
  // message is printed, but we may want to pass the error to the providing
  // extension. TODO(mtomasz): Consider it.
  if (!tag.empty() && tag == it->second.last_tag)
    LOG(ERROR) << "Tag specified, but same as the previous one.";

  it->second.last_tag = tag;

  FOR_EACH_OBSERVER(ProvidedFileSystemObserver,
                    observers_,
                    OnWatcherTagUpdated(file_system_info_, it->second));

  // If the watched entry is deleted, then remove the watcher.
  if (change_type == storage::WatcherManager::DELETED) {
    // Make a copy, since the |it| iterator will get invalidated on the last
    // subscriber.
    Subscribers subscribers = it->second.subscribers;
    for (const auto& subscriber_it : subscribers) {
      RemoveWatcher(subscriber_it.second.origin,
                    entry_path,
                    recursive,
                    base::Bind(&EmptyStatusCallback));
    }
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
