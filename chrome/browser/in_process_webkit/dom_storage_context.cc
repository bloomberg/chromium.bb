// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_context.h"

#include "base/file_path.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/storage_area.h"
#include "chrome/browser/in_process_webkit/storage_namespace.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"

DOMStorageContext::DOMStorageContext(WebKitContext* webkit_context)
    : last_storage_area_id_(kFirstStorageAreaId),
      last_storage_namespace_id_(kFirstStorageNamespaceId),
      webkit_context_(webkit_context) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
}

DOMStorageContext::~DOMStorageContext() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  // The storage namespace destructor unregisters the storage namespace, so
  // our iterator becomes invalid.  Thus we just keep deleting the first item
  // until there are none left.
  while (!storage_namespace_map_.empty())
    delete storage_namespace_map_.begin()->second;
}

StorageNamespace* DOMStorageContext::LocalStorage() {
  StorageNamespace* storage_namespace = GetStorageNamespace(
      kLocalStorageNamespaceId);
  if (storage_namespace)
    return storage_namespace;

  FilePath data_path = webkit_context_->data_path();
  FilePath dir_path;
  if (!data_path.empty())
    dir_path = data_path.AppendASCII("localStorage");
  return StorageNamespace::CreateLocalStorageNamespace(this, dir_path);
}

StorageNamespace* DOMStorageContext::NewSessionStorage() {
  return StorageNamespace::CreateSessionStorageNamespace(this);
}

void DOMStorageContext::RegisterStorageArea(StorageArea* storage_area) {
  int64 id = storage_area->id();
  DCHECK(!GetStorageArea(id));
  storage_area_map_[id] = storage_area;
}

void DOMStorageContext::UnregisterStorageArea(StorageArea* storage_area) {
  int64 id = storage_area->id();
  DCHECK(GetStorageArea(id));
  storage_area_map_.erase(id);
}

StorageArea* DOMStorageContext::GetStorageArea(int64 id) {
  StorageAreaMap::iterator iter = storage_area_map_.find(id);
  if (iter == storage_area_map_.end())
    return NULL;
  return iter->second;
}

void DOMStorageContext::RegisterStorageNamespace(
    StorageNamespace* storage_namespace) {
  int64 id = storage_namespace->id();
  DCHECK(!GetStorageNamespace(id));
  storage_namespace_map_[id] = storage_namespace;
}

void DOMStorageContext::UnregisterStorageNamespace(
    StorageNamespace* storage_namespace) {
  int64 id = storage_namespace->id();
  DCHECK(GetStorageNamespace(id));
  storage_namespace_map_.erase(id);
}

StorageNamespace* DOMStorageContext::GetStorageNamespace(int64 id) {
  StorageNamespaceMap::iterator iter = storage_namespace_map_.find(id);
  if (iter == storage_namespace_map_.end())
    return NULL;
  return iter->second;
}
