// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/storage_area.h"

#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"
#include "webkit/api/public/WebStorageArea.h"
#include "webkit/api/public/WebString.h"

using WebKit::WebStorageArea;

StorageArea::StorageArea(const string16& origin, WebStorageArea* storage_area,
                         int64 id)
    : origin_(origin),
      storage_area_(storage_area),
      id_(id) {
  DCHECK(storage_area_.get());
}

StorageArea::~StorageArea() {
}

unsigned StorageArea::Length() {
  return storage_area_->length();
}

NullableString16 StorageArea::Key(unsigned index) {
  return storage_area_->key(index);
}

NullableString16 StorageArea::GetItem(const string16& key) {
  return storage_area_->getItem(key);
}

void StorageArea::SetItem(const string16& key, const string16& value,
                          bool* quota_exception) {
  storage_area_->setItem(key, value, *quota_exception);
}

void StorageArea::RemoveItem(const string16& key) {
  storage_area_->removeItem(key);
}

void StorageArea::Clear() {
  storage_area_->clear();
}
