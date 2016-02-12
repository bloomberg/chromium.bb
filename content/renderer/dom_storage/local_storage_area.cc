// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_area.h"

using blink::WebString;
using blink::WebURL;

namespace content {

LocalStorageArea::LocalStorageArea(const url::Origin& origin)
    : origin_(origin) {
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

size_t LocalStorageArea::memoryBytesUsedByCache() const {
  return 0u;
}

}  // namespace content
