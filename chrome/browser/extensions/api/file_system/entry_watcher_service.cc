// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/entry_watcher_service.h"

#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/event_router.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace extensions {
namespace {

// Default implementation for dispatching an event. Can be replaced for unit
// tests by EntryWatcherService::SetDispatchEventImplForTesting().
void DispatchEventImpl(EventRouter* event_router,
                       const std::string& extension_id,
                       scoped_ptr<Event> event) {
  event_router->DispatchEventToExtension(extension_id, event.Pass());
}

// Default implementation for acquiring a file system context for a specific
// |extension_id| and |context|.
storage::FileSystemContext* GetFileSystemContextImpl(
    const std::string& extension_id,
    content::BrowserContext* context) {
  const GURL site = util::GetSiteForExtensionId(extension_id, context);
  return content::BrowserContext::GetStoragePartitionForSite(context, site)
      ->GetFileSystemContext();
}

}  // namespace

EntryWatcherService::EntryWatcherService(content::BrowserContext* context)
    : context_(context),
      dispatch_event_impl_(
          base::Bind(&DispatchEventImpl, EventRouter::Get(context))),
      get_file_system_context_impl_(base::Bind(&GetFileSystemContextImpl)),
      observing_(this),
      weak_ptr_factory_(this) {
  // TODO(mtomasz): Restore persistent watchers.
}

EntryWatcherService::~EntryWatcherService() {
}

void EntryWatcherService::SetDispatchEventImplForTesting(
    const DispatchEventImplCallback& callback) {
  dispatch_event_impl_ = callback;
}

void EntryWatcherService::SetGetFileSystemContextImplForTesting(
    const GetFileSystemContextImplCallback& callback) {
  get_file_system_context_impl_ = callback;
}

void EntryWatcherService::WatchDirectory(
    const std::string& extension_id,
    const storage::FileSystemURL& url,
    bool recursive,
    const storage::WatcherManager::StatusCallback& callback) {
  // TODO(mtomasz): Add support for recursive watchers.
  if (recursive) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  storage::FileSystemContext* const context =
      get_file_system_context_impl_.Run(extension_id, context_);
  DCHECK(context);

  storage::WatcherManager* const watcher_manager =
      context->GetWatcherManager(url.type());
  if (!watcher_manager) {
    // Post a task instead of calling the callback directly, since the caller
    // may expect the callback to be called asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  // Passing a pointer to WatcherManager is safe, since the pointer is required
  // to be valid until shutdown.
  context->default_file_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::Bind(&storage::WatcherManager::WatchDirectory,
                 base::Unretained(watcher_manager),  // Outlives the service.
                 url,
                 recursive,
                 base::Bind(&EntryWatcherService::OnWatchDirectoryCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            watcher_manager,  // Outlives the service.
                            extension_id,
                            url,
                            recursive,
                            callback)));
}

void EntryWatcherService::UnwatchEntry(
    const std::string& extension_id,
    const storage::FileSystemURL& url,
    const storage::WatcherManager::StatusCallback& callback) {
  storage::FileSystemContext* const context =
      get_file_system_context_impl_.Run(extension_id, context_);
  DCHECK(context);

  storage::WatcherManager* const watcher_manager =
      context->GetWatcherManager(url.type());
  if (!watcher_manager) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, base::File::FILE_ERROR_INVALID_OPERATION));
    return;
  }

  // Passing a pointer to WatcherManager is safe, since the pointer is required
  // to be valid until shutdown.
  context->default_file_task_runner()->PostNonNestableTask(
      FROM_HERE,
      base::Bind(&storage::WatcherManager::UnwatchEntry,
                 base::Unretained(watcher_manager),  // Outlives the service.
                 url,
                 base::Bind(&EntryWatcherService::OnUnwatchEntryCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            extension_id,
                            url,
                            callback)));
}

std::vector<storage::FileSystemURL> EntryWatcherService::GetWatchedEntries(
    const std::string& extension_id) {
  std::vector<storage::FileSystemURL> result;
  for (WatcherMap::const_iterator it = watchers_.begin(); it != watchers_.end();
       ++it) {
    const std::map<std::string, EntryWatcher>::const_iterator watcher_it =
        it->second.find(extension_id);
    if (watcher_it != it->second.end())
      result.push_back(watcher_it->second.url);
  }

  return result;
}

void EntryWatcherService::OnEntryChanged(const storage::FileSystemURL& url) {
  const WatcherMap::const_iterator it = watchers_.find(url);
  DCHECK(it != watchers_.end());
  for (std::map<std::string, EntryWatcher>::const_iterator watcher_it =
           it->second.begin();
       watcher_it != it->second.end();
       ++watcher_it) {
    const std::string& extension_id = watcher_it->first;
    api::file_system::EntryChangedEvent event;
    dispatch_event_impl_.Run(
        extension_id,
        make_scoped_ptr(
            new Event(api::file_system::OnEntryChanged::kEventName,
                      api::file_system::OnEntryChanged::Create(event))));
  }
}

void EntryWatcherService::OnEntryRemoved(const storage::FileSystemURL& url) {
  WatcherMap::const_iterator it = watchers_.find(url);
  DCHECK(it != watchers_.end());
  for (std::map<std::string, EntryWatcher>::const_iterator watcher_it =
           it->second.begin();
       watcher_it != it->second.end();
       ++watcher_it) {
    const std::string& extension_id = watcher_it->first;
    api::file_system::EntryRemovedEvent event;
    dispatch_event_impl_.Run(
        extension_id,
        make_scoped_ptr(
            new Event(api::file_system::OnEntryRemoved::kEventName,
                      api::file_system::OnEntryRemoved::Create(event))));
  }
}

void EntryWatcherService::OnWatchDirectoryCompleted(
    storage::WatcherManager* watcher_manager,
    const std::string& extension_id,
    const storage::FileSystemURL& url,
    bool recursive,
    const storage::WatcherManager::StatusCallback& callback,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    callback.Run(result);
    return;
  }

  storage::FileSystemContext* const context =
      get_file_system_context_impl_.Run(extension_id, context_);
  DCHECK(context);

  // Observe the manager if not observed yet.
  if (!observing_.IsObserving(watcher_manager))
    observing_.Add(watcher_manager);

  watchers_[url][extension_id] =
      EntryWatcher(url, true /* directory */, recursive);

  // TODO(mtomasz): Save in preferences.

  callback.Run(base::File::FILE_OK);
}

void EntryWatcherService::OnUnwatchEntryCompleted(
    const std::string& extension_id,
    const storage::FileSystemURL& url,
    const storage::WatcherManager::StatusCallback& callback,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    callback.Run(result);
    return;
  }

  if (watchers_[url].erase(extension_id) == 0) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  if (watchers_[url].empty())
    watchers_.erase(url);

  // TODO(mtomasz): Save in preferences.

  callback.Run(base::File::FILE_OK);
}

EntryWatcherService::EntryWatcher::EntryWatcher()
    : directory(false), recursive(false) {
}

EntryWatcherService::EntryWatcher::EntryWatcher(
    const storage::FileSystemURL& url,
    bool directory,
    bool recursive)
    : url(url), directory(directory), recursive(recursive) {
}

EntryWatcherService::EntryWatcher::~EntryWatcher() {
}

}  // namespace extensions
