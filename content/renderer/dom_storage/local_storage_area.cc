// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_area.h"

#include "content/common/storage_partition_service.mojom.h"

using blink::WebString;
using blink::WebURL;

namespace content {

LocalStorageArea::LocalStorageArea(
    const url::Origin& origin,
    StoragePartitionService* storage_partition_service)
    : origin_(origin), binding_(this) {
  storage_partition_service->OpenLocalStorage(
      origin_.Serialize(), binding_.CreateInterfacePtrAndBind(),
      mojo::GetProxy(&leveldb_));
}

LocalStorageArea::~LocalStorageArea() {
}

unsigned LocalStorageArea::length() {
  return 0u;
}

WebString LocalStorageArea::key(unsigned index) {
  return WebString();
}

WebString LocalStorageArea::getItem(const WebString& key) {
  return WebString();
}

void LocalStorageArea::setItem(
    const WebString& key, const WebString& value, const WebURL& page_url,
    WebStorageArea::Result& result) {
}

void LocalStorageArea::removeItem(
    const WebString& key, const WebURL& page_url) {
}

void LocalStorageArea::clear(const WebURL& page_url) {
}

void LocalStorageArea::KeyChanged(mojo::Array<uint8_t> key,
                                  mojo::Array<uint8_t> new_value,
                                  mojo::Array<uint8_t> old_value,
                                  const mojo::String& source) {
}

void LocalStorageArea::KeyDeleted(mojo::Array<uint8_t> key,
                                  const mojo::String& source) {
}

void LocalStorageArea::AllDeleted(const mojo::String& source) {
}

}  // namespace content
