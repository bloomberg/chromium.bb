// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager.h"
#include "chrome/browser/chromeos/file_system_provider/operations/abort.h"
#include "chrome/browser/chromeos/file_system_provider/operations/add_watcher.h"
#include "chrome/browser/chromeos/file_system_provider/operations/close_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/configure.h"
#include "chrome/browser/chromeos/file_system_provider/operations/copy_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/create_directory.h"
#include "chrome/browser/chromeos/file_system_provider/operations/create_file.h"
#include "chrome/browser/chromeos/file_system_provider/operations/delete_entry.h"
#include "chrome/browser/chromeos/file_system_provider/operations/execute_action.h"
#include "chrome/browser/chromeos/file_system_provider/operations/get_actions.h"
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

struct ProvidedFileSystem::AddWatcherInQueueArgs {
  AddWatcherInQueueArgs(
      size_t token,
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      bool persistent,
      storage::AsyncFileUtil::StatusCallback callback,
      storage::WatcherManager::NotificationCallback notification_callback)
      : token(token),
        origin(origin),
        entry_path(entry_path),
        recursive(recursive),
        persistent(persistent),
        callback(std::move(callback)),
        notification_callback(std::move(notification_callback)) {}
  ~AddWatcherInQueueArgs() {}
  AddWatcherInQueueArgs(AddWatcherInQueueArgs&&) = default;

  const size_t token;
  const GURL origin;
  const base::FilePath entry_path;
  const bool recursive;
  const bool persistent;
  storage::AsyncFileUtil::StatusCallback callback;
  const storage::WatcherManager::NotificationCallback notification_callback;
};

struct ProvidedFileSystem::NotifyInQueueArgs {
  NotifyInQueueArgs(
      size_t token,
      const base::FilePath& entry_path,
      bool recursive,
      storage::WatcherManager::ChangeType change_type,
      std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
      const std::string& tag,
      storage::AsyncFileUtil::StatusCallback callback)
      : token(token),
        entry_path(entry_path),
        recursive(recursive),
        change_type(change_type),
        changes(std::move(changes)),
        tag(tag),
        callback(std::move(callback)) {}
  ~NotifyInQueueArgs() {}

  const size_t token;
  const base::FilePath entry_path;
  const bool recursive;
  const storage::WatcherManager::ChangeType change_type;
  const std::unique_ptr<ProvidedFileSystemObserver::Changes> changes;
  const std::string tag;
  storage::AsyncFileUtil::StatusCallback callback;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifyInQueueArgs);
};

ProvidedFileSystem::ProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info)
    : profile_(profile),
      event_router_(extensions::EventRouter::Get(profile)),  // May be NULL.
      file_system_info_(file_system_info),
      notification_manager_(
          new NotificationManager(profile_, file_system_info_)),
      request_manager_(
          new RequestManager(profile,
                             file_system_info.provider_id().GetExtensionId(),
                             notification_manager_.get())),
      watcher_queue_(1) {
  DCHECK_EQ(ProviderId::EXTENSION, file_system_info.provider_id().GetType());
}

ProvidedFileSystem::~ProvidedFileSystem() {
  const std::vector<int> request_ids = request_manager_->GetActiveRequestIds();
  for (size_t i = 0; i < request_ids.size(); ++i) {
    Abort(request_ids[i]);
  }
}

void ProvidedFileSystem::SetEventRouterForTesting(
    extensions::EventRouter* event_router) {
  event_router_ = event_router;
}

void ProvidedFileSystem::SetNotificationManagerForTesting(
    std::unique_ptr<NotificationManagerInterface> notification_manager) {
  notification_manager_ = std::move(notification_manager);
  request_manager_.reset(new RequestManager(
      profile_, file_system_info_.provider_id().GetExtensionId(),
      notification_manager_.get()));
}

AbortCallback ProvidedFileSystem::RequestUnmount(
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      REQUEST_UNMOUNT,
      std::make_unique<operations::Unmount>(event_router_, file_system_info_,
                                            copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::GetMetadata(const base::FilePath& entry_path,
                                              MetadataFieldMask fields,
                                              GetMetadataCallback callback) {
  // Create |copyable_callback| which is copyable, though it can still only be
  // called at most once.  This is safe, because RequestManager::CreateRequest()
  // is guaranteed not to call |callback| if it signals an error (by returning
  // request_id == 0).
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      GET_METADATA,
      std::unique_ptr<RequestManager::HandlerInterface>(
          new operations::GetMetadata(event_router_, file_system_info_,
                                      entry_path, fields, copyable_callback)));
  if (!request_id) {
    copyable_callback.Run(base::WrapUnique<EntryMetadata>(NULL),
                          base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    GetActionsCallback callback) {
  const int request_id = request_manager_->CreateRequest(
      GET_ACTIONS,
      std::unique_ptr<RequestManager::HandlerInterface>(
          new operations::GetActions(event_router_, file_system_info_,
                                     entry_paths, callback)));
  if (!request_id) {
    // If the provider doesn't listen for GetActions requests, treat it as
    // having no actions.
    callback.Run(Actions(), base::File::FILE_OK);
    return AbortCallback();
  }

  return base::Bind(&ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(),
                    request_id);
}

AbortCallback ProvidedFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      EXECUTE_ACTION, std::make_unique<operations::ExecuteAction>(
                          event_router_, file_system_info_, entry_paths,
                          action_id, copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(&ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(),
                    request_id);
}

AbortCallback ProvidedFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  const int request_id = request_manager_->CreateRequest(
      READ_DIRECTORY,
      std::unique_ptr<RequestManager::HandlerInterface>(
          new operations::ReadDirectory(event_router_, file_system_info_,
                                        directory_path, callback)));
  if (!request_id) {
    callback.Run(base::File::FILE_ERROR_SECURITY,
                 storage::AsyncFileUtil::EntryList(),
                 false /* has_more */);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::ReadFile(int file_handle,
                                           net::IOBuffer* buffer,
                                           int64_t offset,
                                           int length,
                                           ReadChunkReceivedCallback callback) {
  TRACE_EVENT1(
      "file_system_provider", "ProvidedFileSystem::ReadFile", "length", length);
  const int request_id = request_manager_->CreateRequest(
      READ_FILE, base::WrapUnique<RequestManager::HandlerInterface>(
                     new operations::ReadFile(event_router_, file_system_info_,
                                              file_handle, buffer, offset,
                                              length, callback)));
  if (!request_id) {
    callback.Run(0 /* chunk_length */,
                 false /* has_more */,
                 base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::OpenFile(const base::FilePath& file_path,
                                           OpenFileMode mode,
                                           OpenFileCallback callback) {
  const int request_id = request_manager_->CreateRequest(
      OPEN_FILE, std::unique_ptr<RequestManager::HandlerInterface>(
                     new operations::OpenFile(
                         event_router_, file_system_info_, file_path, mode,
                         base::Bind(&ProvidedFileSystem::OnOpenFileCompleted,
                                    weak_ptr_factory_.GetWeakPtr(), file_path,
                                    mode, callback))));
  if (!request_id) {
    callback.Run(0 /* file_handle */, base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(&ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(),
                    request_id);
}

AbortCallback ProvidedFileSystem::CloseFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      CLOSE_FILE, std::make_unique<operations::CloseFile>(
                      event_router_, file_system_info_, file_handle,
                      base::Bind(&ProvidedFileSystem::OnCloseFileCompleted,
                                 weak_ptr_factory_.GetWeakPtr(), file_handle,
                                 copyable_callback)));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      CREATE_DIRECTORY, std::make_unique<operations::CreateDirectory>(
                            event_router_, file_system_info_, directory_path,
                            recursive, copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      DELETE_ENTRY, std::make_unique<operations::DeleteEntry>(
                        event_router_, file_system_info_, entry_path, recursive,
                        copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      CREATE_FILE,
      std::make_unique<operations::CreateFile>(event_router_, file_system_info_,
                                               file_path, copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      COPY_ENTRY, std::make_unique<operations::CopyEntry>(
                      event_router_, file_system_info_, source_path,
                      target_path, copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    storage::AsyncFileUtil::StatusCallback callback) {
  TRACE_EVENT1("file_system_provider",
               "ProvidedFileSystem::WriteFile",
               "length",
               length);
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      WRITE_FILE,
      std::make_unique<operations::WriteFile>(
          event_router_, file_system_info_, file_handle,
          base::WrapRefCounted(buffer), offset, length, copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      MOVE_ENTRY, std::make_unique<operations::MoveEntry>(
                      event_router_, file_system_info_, source_path,
                      target_path, copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      TRUNCATE, std::make_unique<operations::Truncate>(
                    event_router_, file_system_info_, file_path, length,
                    copyable_callback));
  if (!request_id) {
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
    return AbortCallback();
  }

  return base::Bind(
      &ProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), request_id);
}

AbortCallback ProvidedFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  const size_t token = watcher_queue_.NewToken();
  watcher_queue_.Enqueue(
      token,
      base::BindOnce(&ProvidedFileSystem::AddWatcherInQueue,
                     base::Unretained(this),  // Outlived by the queue.
                     AddWatcherInQueueArgs(token, origin, entry_path, recursive,
                                           persistent, std::move(callback),
                                           std::move(notification_callback))));
  return AbortCallback();
}

void ProvidedFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  const size_t token = watcher_queue_.NewToken();
  watcher_queue_.Enqueue(
      token, base::BindOnce(&ProvidedFileSystem::RemoveWatcherInQueue,
                            base::Unretained(this),  // Outlived by the queue.
                            token, origin, entry_path, recursive,
                            std::move(callback)));
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

const OpenedFiles& ProvidedFileSystem::GetOpenedFiles() const {
  return opened_files_;
}

void ProvidedFileSystem::AddObserver(ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ProvidedFileSystem::RemoveObserver(ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void ProvidedFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
    const std::string& tag,
    storage::AsyncFileUtil::StatusCallback callback) {
  const size_t token = watcher_queue_.NewToken();
  watcher_queue_.Enqueue(
      token, base::BindOnce(&ProvidedFileSystem::NotifyInQueue,
                            base::Unretained(this),  // Outlived by the queue.
                            std::make_unique<NotifyInQueueArgs>(
                                token, entry_path, recursive, change_type,
                                std::move(changes), tag, std::move(callback))));
}

void ProvidedFileSystem::Configure(
    storage::AsyncFileUtil::StatusCallback callback) {
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      CONFIGURE, std::make_unique<operations::Configure>(
                     event_router_, file_system_info_, copyable_callback));
  if (!request_id)
    copyable_callback.Run(base::File::FILE_ERROR_SECURITY);
}

void ProvidedFileSystem::Abort(int operation_request_id) {
  if (!request_manager_->CreateRequest(
          ABORT, std::unique_ptr<RequestManager::HandlerInterface>(
                     new operations::Abort(
                         event_router_, file_system_info_, operation_request_id,
                         base::Bind(&ProvidedFileSystem::OnAbortCompleted,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    operation_request_id))))) {
    // If the aborting event is not handled, then the operation should simply
    // be not aborted. Instead we'll wait until it completes.
    LOG(ERROR) << "Failed to create an abort request.";
  }
}

void ProvidedFileSystem::OnAbortCompleted(int operation_request_id,
                                          base::File::Error result) {
  if (result != base::File::FILE_OK) {
    // If an error in aborting happens, then do not abort the request in the
    // request manager, as the operation is supposed to complete. The only case
    // it wouldn't complete is if there is a bug in the extension code, and
    // the extension never calls the callback. We consiously *do not* handle
    // bugs in extensions here.
    return;
  }
  request_manager_->RejectRequest(operation_request_id,
                                  std::make_unique<RequestValue>(),
                                  base::File::FILE_ERROR_ABORT);
}

AbortCallback ProvidedFileSystem::AddWatcherInQueue(
    AddWatcherInQueueArgs args) {
  if (args.persistent && (!file_system_info_.supports_notify_tag() ||
                          !args.notification_callback.is_null())) {
    OnAddWatcherInQueueCompleted(args.token, args.entry_path, args.recursive,
                                 Subscriber(), std::move(args.callback),
                                 base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }

  // Create a candidate subscriber. This could be done in OnAddWatcherCompleted,
  // but base::Bind supports only up to 7 arguments.
  Subscriber subscriber;
  subscriber.origin = args.origin;
  subscriber.persistent = args.persistent;
  subscriber.notification_callback = args.notification_callback;

  const WatcherKey key(args.entry_path, args.recursive);
  const Watchers::const_iterator it = watchers_.find(key);
  if (it != watchers_.end()) {
    const bool exists = it->second.subscribers.find(args.origin) !=
                        it->second.subscribers.end();
    OnAddWatcherInQueueCompleted(
        args.token, args.entry_path, args.recursive, subscriber,
        std::move(args.callback),
        exists ? base::File::FILE_ERROR_EXISTS : base::File::FILE_OK);
    return AbortCallback();
  }

  auto copyable_callback =
      base::AdaptCallbackForRepeating(std::move(args.callback));
  const int request_id = request_manager_->CreateRequest(
      ADD_WATCHER,
      std::make_unique<operations::AddWatcher>(
          event_router_, file_system_info_, args.entry_path, args.recursive,
          base::Bind(&ProvidedFileSystem::OnAddWatcherInQueueCompleted,
                     weak_ptr_factory_.GetWeakPtr(), args.token,
                     args.entry_path, args.recursive, subscriber,
                     copyable_callback)));

  if (!request_id) {
    OnAddWatcherInQueueCompleted(args.token, args.entry_path, args.recursive,
                                 subscriber, copyable_callback,
                                 base::File::FILE_ERROR_SECURITY);
  }

  return AbortCallback();
}

AbortCallback ProvidedFileSystem::RemoveWatcherInQueue(
    size_t token,
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  const WatcherKey key(entry_path, recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it == watchers_.end() ||
      it->second.subscribers.find(origin) == it->second.subscribers.end()) {
    OnRemoveWatcherInQueueCompleted(token, origin, key, std::move(callback),
                                    false /* extension_response */,
                                    base::File::FILE_ERROR_NOT_FOUND);
    return AbortCallback();
  }

  // If there are other subscribers, then do not remove the observer, but simply
  // return a success.
  if (it->second.subscribers.size() > 1) {
    OnRemoveWatcherInQueueCompleted(token, origin, key, std::move(callback),
                                    false /* extension_response */,
                                    base::File::FILE_OK);
    return AbortCallback();
  }

  // Otherwise, emit an event, and remove the watcher.
  request_manager_->CreateRequest(
      REMOVE_WATCHER,
      std::make_unique<operations::RemoveWatcher>(
          event_router_, file_system_info_, entry_path, recursive,
          base::Bind(&ProvidedFileSystem::OnRemoveWatcherInQueueCompleted,
                     weak_ptr_factory_.GetWeakPtr(), token, origin, key,
                     base::Passed(&callback), true /* extension_response */)));

  return AbortCallback();
}

AbortCallback ProvidedFileSystem::NotifyInQueue(
    std::unique_ptr<NotifyInQueueArgs> args) {
  const WatcherKey key(args->entry_path, args->recursive);
  const auto& watcher_it = watchers_.find(key);
  if (watcher_it == watchers_.end()) {
    OnNotifyInQueueCompleted(std::move(args), base::File::FILE_ERROR_NOT_FOUND);
    return AbortCallback();
  }

  // The tag must be provided if and only if it's explicitly supported.
  if (file_system_info_.supports_notify_tag() == args->tag.empty()) {
    OnNotifyInQueueCompleted(std::move(args),
                             base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }

  // It's illegal to provide a tag which is not unique.
  if (!args->tag.empty() && args->tag == watcher_it->second.last_tag) {
    OnNotifyInQueueCompleted(std::move(args),
                             base::File::FILE_ERROR_INVALID_OPERATION);
    return AbortCallback();
  }

  // The object is owned by AutoUpdated, so the reference is valid as long as
  // callbacks created with AutoUpdater::CreateCallback().
  const ProvidedFileSystemObserver::Changes& changes_ref = *args->changes.get();
  const storage::WatcherManager::ChangeType change_type = args->change_type;

  scoped_refptr<AutoUpdater> auto_updater(
      new AutoUpdater(base::Bind(&ProvidedFileSystem::OnNotifyInQueueCompleted,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 base::Passed(&args), base::File::FILE_OK)));

  // Call all notification callbacks (if any).
  for (const auto& subscriber_it : watcher_it->second.subscribers) {
    const storage::WatcherManager::NotificationCallback& notification_callback =
        subscriber_it.second.notification_callback;
    if (!notification_callback.is_null())
      notification_callback.Run(change_type);
  }

  // Notify all observers.
  for (auto& observer : observers_) {
    observer.OnWatcherChanged(file_system_info_, watcher_it->second,
                              change_type, changes_ref,
                              auto_updater->CreateCallback());
  }

  return AbortCallback();
}

base::WeakPtr<ProvidedFileSystemInterface> ProvidedFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProvidedFileSystem::OnAddWatcherInQueueCompleted(
    size_t token,
    const base::FilePath& entry_path,
    bool recursive,
    const Subscriber& subscriber,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(result);
    watcher_queue_.Complete(token);
    return;
  }

  const WatcherKey key(entry_path, recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it != watchers_.end()) {
    std::move(callback).Run(base::File::FILE_OK);
    watcher_queue_.Complete(token);
    return;
  }

  Watcher* const watcher = &watchers_[key];
  watcher->entry_path = entry_path;
  watcher->recursive = recursive;
  watcher->subscribers[subscriber.origin] = subscriber;

  for (auto& observer : observers_)
    observer.OnWatcherListChanged(file_system_info_, watchers_);

  std::move(callback).Run(base::File::FILE_OK);
  watcher_queue_.Complete(token);
}

void ProvidedFileSystem::OnRemoveWatcherInQueueCompleted(
    size_t token,
    const GURL& origin,
    const WatcherKey& key,
    storage::AsyncFileUtil::StatusCallback callback,
    bool extension_response,
    base::File::Error result) {
  if (!extension_response && result != base::File::FILE_OK) {
    watcher_queue_.Complete(token);
    std::move(callback).Run(result);
    return;
  }

  // Even if the extension returns an error, the callback is called with base::
  // File::FILE_OK.
  const auto it = watchers_.find(key);
  DCHECK(it != watchers_.end());
  DCHECK(it->second.subscribers.find(origin) != it->second.subscribers.end());

  it->second.subscribers.erase(origin);

  for (auto& observer : observers_)
    observer.OnWatcherListChanged(file_system_info_, watchers_);

  // If there are no more subscribers, then remove the watcher.
  if (it->second.subscribers.empty())
    watchers_.erase(it);

  std::move(callback).Run(base::File::FILE_OK);
  watcher_queue_.Complete(token);
}

void ProvidedFileSystem::OnNotifyInQueueCompleted(
    std::unique_ptr<NotifyInQueueArgs> args,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    std::move(args->callback).Run(result);
    watcher_queue_.Complete(args->token);
    return;
  }

  // Check if the entry is still watched.
  const WatcherKey key(args->entry_path, args->recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it == watchers_.end()) {
    std::move(args->callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    watcher_queue_.Complete(args->token);
    return;
  }

  it->second.last_tag = args->tag;

  for (auto& observer : observers_)
    observer.OnWatcherTagUpdated(file_system_info_, it->second);

  // If the watched entry is deleted, then remove the watcher.
  if (args->change_type == storage::WatcherManager::DELETED) {
    // Make a copy, since the |it| iterator will get invalidated on the last
    // subscriber.
    Subscribers subscribers = it->second.subscribers;
    for (const auto& subscriber_it : subscribers) {
      RemoveWatcher(subscriber_it.second.origin, args->entry_path,
                    args->recursive, base::DoNothing());
    }
  }

  std::move(args->callback).Run(base::File::FILE_OK);
  watcher_queue_.Complete(args->token);
}

void ProvidedFileSystem::OnOpenFileCompleted(const base::FilePath& file_path,
                                             OpenFileMode mode,
                                             OpenFileCallback callback,
                                             int file_handle,
                                             base::File::Error result) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_handle, result);
    return;
  }

  opened_files_[file_handle] = OpenedFile(file_path, mode);
  std::move(callback).Run(file_handle, base::File::FILE_OK);
}

void ProvidedFileSystem::OnCloseFileCompleted(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback,
    base::File::Error result) {
  // Closing files is final. Even if an error happened, we remove it from the
  // list of opened files.
  opened_files_.erase(file_handle);
  std::move(callback).Run(result);
}

}  // namespace file_system_provider
}  // namespace chromeos
