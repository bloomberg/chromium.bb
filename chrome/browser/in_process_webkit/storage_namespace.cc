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
#include "webkit/api/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;

/* static */
StorageNamespace* StorageNamespace::CreateLocalStorageNamespace(
    DOMStorageContext* dom_storage_context, const FilePath& dir_path) {
  int64 id = dom_storage_context->kLocalStorageNamespaceId;
  DCHECK(!dom_storage_context->GetStorageNamespace(id));
  WebString path = webkit_glue::FilePathToWebString(dir_path);
  WebStorageNamespace* web_storage_namespace =
      WebStorageNamespace::createLocalStorageNamespace(path,
                                                       kLocalStorageQuota);
  return new StorageNamespace(dom_storage_context, web_storage_namespace, id,
                              DOM_STORAGE_LOCAL);
}

/* static */
StorageNamespace* StorageNamespace::CreateSessionStorageNamespace(
    DOMStorageContext* dom_storage_context) {
  int64 id = dom_storage_context->AllocateStorageNamespaceId();
  DCHECK(!dom_storage_context->GetStorageNamespace(id));
  WebStorageNamespace* web_storage_namespace =
      WebStorageNamespace::createSessionStorageNamespace();
  return new StorageNamespace(dom_storage_context, web_storage_namespace, id,
                              DOM_STORAGE_SESSION);
}

StorageNamespace::StorageNamespace(DOMStorageContext* dom_storage_context,
                                   WebStorageNamespace* storage_namespace,
                                   int64 id, DOMStorageType storage_type)
    : dom_storage_context_(dom_storage_context),
      storage_namespace_(storage_namespace),
      id_(id),
      storage_type_(storage_type) {
  DCHECK(dom_storage_context_);
  DCHECK(storage_namespace_);
  dom_storage_context_->RegisterStorageNamespace(this);
}

StorageNamespace::~StorageNamespace() {
  dom_storage_context_->UnregisterStorageNamespace(this);

  OriginToStorageAreaMap::iterator iter = origin_to_storage_area_.begin();
  OriginToStorageAreaMap::iterator end = origin_to_storage_area_.end();
  while (iter != end) {
    dom_storage_context_->UnregisterStorageArea(iter->second);
    delete iter->second;
    ++iter;
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
  WebStorageArea* web_storage_area =
      storage_namespace_->createStorageArea(origin);
  StorageArea* storage_area = new StorageArea(origin, web_storage_area, id);
  origin_to_storage_area_[origin] = storage_area;
  dom_storage_context_->RegisterStorageArea(storage_area);
  return storage_area;
}

StorageNamespace* StorageNamespace::Copy() {
  DCHECK(storage_type_ == DOM_STORAGE_SESSION);
  int64 id = dom_storage_context_->AllocateStorageNamespaceId();
  DCHECK(!dom_storage_context_->GetStorageNamespace(id));
  WebStorageNamespace* new_storage_namespace = storage_namespace_->copy();
  return new StorageNamespace(dom_storage_context_, new_storage_namespace, id,
                              storage_type_);
}

void StorageNamespace::Close() {
  storage_namespace_->close();
}
