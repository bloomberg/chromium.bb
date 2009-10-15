// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/storage_namespace.h"

#include "base/file_path.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/in_process_webkit/storage_area.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebStorageNamespace.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;

/* static */
StorageNamespace* StorageNamespace::CreateLocalStorageNamespace(
    DOMStorageContext* dom_storage_context, const FilePath& data_dir_path) {
  int64 id = dom_storage_context->kLocalStorageNamespaceId;
  DCHECK(!dom_storage_context->GetStorageNamespace(id));
  return new StorageNamespace(dom_storage_context, id,
      webkit_glue::FilePathToWebString(data_dir_path), DOM_STORAGE_LOCAL);
}

/* static */
StorageNamespace* StorageNamespace::CreateSessionStorageNamespace(
    DOMStorageContext* dom_storage_context) {
  int64 id = dom_storage_context->AllocateStorageNamespaceId();
  DCHECK(!dom_storage_context->GetStorageNamespace(id));
  return new StorageNamespace(dom_storage_context, id, WebString(),
                              DOM_STORAGE_SESSION);
}

StorageNamespace::StorageNamespace(DOMStorageContext* dom_storage_context,
                                   int64 id,
                                   const WebString& data_dir_path,
                                   DOMStorageType dom_storage_type)
    : dom_storage_context_(dom_storage_context),
      id_(id),
      data_dir_path_(data_dir_path),
      dom_storage_type_(dom_storage_type) {
  DCHECK(dom_storage_context_);
  dom_storage_context_->RegisterStorageNamespace(this);
}

StorageNamespace::~StorageNamespace() {
  dom_storage_context_->UnregisterStorageNamespace(this);

  for (OriginToStorageAreaMap::iterator iter(origin_to_storage_area_.begin());
       iter != origin_to_storage_area_.end(); ++iter) {
    dom_storage_context_->UnregisterStorageArea(iter->second);
    delete iter->second;
  }
}

StorageArea* StorageNamespace::GetStorageArea(const string16& origin) {
  // We may have already created it for another dispatcher host.
  OriginToStorageAreaMap::iterator iter = origin_to_storage_area_.find(origin);
  if (iter != origin_to_storage_area_.end())
    return iter->second;

  // We need to create a new one.
  int64 id = dom_storage_context_->AllocateStorageAreaId();
  DCHECK(!dom_storage_context_->GetStorageArea(id));
  StorageArea* storage_area = new StorageArea(origin, id, this);
  origin_to_storage_area_[origin] = storage_area;
  dom_storage_context_->RegisterStorageArea(storage_area);
  return storage_area;
}

StorageNamespace* StorageNamespace::Copy() {
  DCHECK(dom_storage_type_ == DOM_STORAGE_SESSION);
  int64 id = dom_storage_context_->AllocateStorageNamespaceId();
  DCHECK(!dom_storage_context_->GetStorageNamespace(id));
  StorageNamespace* new_storage_namespace = new StorageNamespace(
      dom_storage_context_, id, data_dir_path_, dom_storage_type_);
  CreateWebStorageNamespaceIfNecessary();
  new_storage_namespace->storage_namespace_.reset(storage_namespace_->copy());
  return new_storage_namespace;
}

void StorageNamespace::PurgeMemory() {
  for (OriginToStorageAreaMap::iterator iter(origin_to_storage_area_.begin());
       iter != origin_to_storage_area_.end(); ++iter)
    iter->second->PurgeMemory();
  storage_namespace_.reset();
}

WebStorageArea* StorageNamespace::CreateWebStorageArea(const string16& origin) {
  CreateWebStorageNamespaceIfNecessary();
  return storage_namespace_->createStorageArea(origin);
}

void StorageNamespace::CreateWebStorageNamespaceIfNecessary() {
  if (storage_namespace_.get())
    return;

  if (dom_storage_type_ == DOM_STORAGE_LOCAL) {
    storage_namespace_.reset(
        WebStorageNamespace::createLocalStorageNamespace(data_dir_path_,
                                                         kLocalStorageQuota);
  } else {
    storage_namespace_.reset(
        WebStorageNamespace::createSessionStorageNamespace());
  }
}
