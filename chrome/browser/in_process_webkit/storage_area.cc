// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/storage_area.h"

#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "chrome/browser/in_process_webkit/storage_namespace.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"

using WebKit::WebStorageArea;
using WebKit::WebURL;

StorageArea::StorageArea(const string16& origin,
                         int64 id,
                         StorageNamespace* owner)
    : origin_(origin),
      id_(id),
      owner_(owner) {
  DCHECK(owner_);
}

StorageArea::~StorageArea() {
}

unsigned StorageArea::Length() {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->length();
}

NullableString16 StorageArea::Key(unsigned index) {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->key(index);
}

NullableString16 StorageArea::GetItem(const string16& key) {
  CreateWebStorageAreaIfNecessary();
  return storage_area_->getItem(key);
}

void StorageArea::SetItem(const string16& key, const string16& value,
                          bool* quota_exception) {
  CreateWebStorageAreaIfNecessary();
  storage_area_->setItem(key, value, WebURL(), *quota_exception);
}

void StorageArea::RemoveItem(const string16& key) {
  CreateWebStorageAreaIfNecessary();
  storage_area_->removeItem(key, WebURL());
}

void StorageArea::Clear() {
  CreateWebStorageAreaIfNecessary();
  storage_area_->clear(WebURL());
}

void StorageArea::PurgeMemory() {
  storage_area_.reset();
}

void StorageArea::CreateWebStorageAreaIfNecessary() {
  if (!storage_area_.get())
    storage_area_.reset(owner_->CreateWebStorageArea(origin_));
}
