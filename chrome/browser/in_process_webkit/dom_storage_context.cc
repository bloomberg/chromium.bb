// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_context.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/storage_area.h"
#include "chrome/browser/in_process_webkit/storage_namespace.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"

static const char* kLocalStorageDirectory = "Local Storage";

// TODO(jorlow): Remove after Chrome 4 ships.
static void MigrateLocalStorageDirectory(const FilePath& data_path) {
  FilePath new_path = data_path.AppendASCII(kLocalStorageDirectory);
  FilePath old_path = data_path.AppendASCII("localStorage");
  if (!file_util::DirectoryExists(new_path) &&
      file_util::DirectoryExists(old_path)) {
    file_util::Move(old_path, new_path);
  }
}

DOMStorageContext::DOMStorageContext(WebKitContext* webkit_context)
    : last_storage_area_id_(kFirstStorageAreaId),
      last_storage_namespace_id_(kFirstStorageNamespaceId),
      webkit_context_(webkit_context) {
}

DOMStorageContext::~DOMStorageContext() {
  // This should not go away until all DOM Storage Dispatcher hosts have gone
  // away.  And they remove themselves from this list.
  DCHECK(dispatcher_host_set_.empty());

  // If we don't have any work to do on the WebKit thread, bail.
  if (storage_namespace_map_.empty())
    return;
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));

  // The storage namespace destructor unregisters the storage namespace, so
  // our iterator becomes invalid.  Thus we just keep deleting the first item
  // until there are none left.
  while (!storage_namespace_map_.empty())
    delete storage_namespace_map_.begin()->second;
}

StorageNamespace* DOMStorageContext::LocalStorage() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageNamespace* storage_namespace = GetStorageNamespace(
      kLocalStorageNamespaceId);
  if (storage_namespace)
    return storage_namespace;

  FilePath data_path = webkit_context_->data_path();
  FilePath dir_path;
  if (!data_path.empty()) {
    MigrateLocalStorageDirectory(data_path);
    dir_path = data_path.AppendASCII(kLocalStorageDirectory);
  }
  return StorageNamespace::CreateLocalStorageNamespace(this, dir_path);
}

StorageNamespace* DOMStorageContext::NewSessionStorage() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  return StorageNamespace::CreateSessionStorageNamespace(this);
}

void DOMStorageContext::RegisterStorageArea(StorageArea* storage_area) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  int64 id = storage_area->id();
  DCHECK(!GetStorageArea(id));
  storage_area_map_[id] = storage_area;
}

void DOMStorageContext::UnregisterStorageArea(StorageArea* storage_area) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  int64 id = storage_area->id();
  DCHECK(GetStorageArea(id));
  storage_area_map_.erase(id);
}

StorageArea* DOMStorageContext::GetStorageArea(int64 id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageAreaMap::iterator iter = storage_area_map_.find(id);
  if (iter == storage_area_map_.end())
    return NULL;
  return iter->second;
}

void DOMStorageContext::RegisterStorageNamespace(
    StorageNamespace* storage_namespace) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  int64 id = storage_namespace->id();
  DCHECK(!GetStorageNamespace(id));
  storage_namespace_map_[id] = storage_namespace;
}

void DOMStorageContext::UnregisterStorageNamespace(
    StorageNamespace* storage_namespace) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  int64 id = storage_namespace->id();
  DCHECK(GetStorageNamespace(id));
  storage_namespace_map_.erase(id);
}

StorageNamespace* DOMStorageContext::GetStorageNamespace(int64 id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  StorageNamespaceMap::iterator iter = storage_namespace_map_.find(id);
  if (iter == storage_namespace_map_.end())
    return NULL;
  return iter->second;
}

void DOMStorageContext::RegisterDispatcherHost(
    DOMStorageDispatcherHost* dispatcher_host) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(dispatcher_host_set_.find(dispatcher_host) ==
         dispatcher_host_set_.end());
  dispatcher_host_set_.insert(dispatcher_host);
}

void DOMStorageContext::UnregisterDispatcherHost(
    DOMStorageDispatcherHost* dispatcher_host) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(dispatcher_host_set_.find(dispatcher_host) !=
         dispatcher_host_set_.end());
  dispatcher_host_set_.erase(dispatcher_host);
}

const DOMStorageContext::DispatcherHostSet*
DOMStorageContext::GetDispatcherHostSet() const {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return &dispatcher_host_set_;
}

void DOMStorageContext::PurgeMemory() {
  // It is only safe to purge the memory from the LocalStorage namespace,
  // because it is backed by disk and can be reloaded later.  If we purge a
  // SessionStorage namespace, its data will be gone forever, because it isn't
  // currently backed by disk.
  StorageNamespace* local_storage =
      GetStorageNamespace(kLocalStorageNamespaceId);
  if (local_storage)
    local_storage->PurgeMemory();
}
