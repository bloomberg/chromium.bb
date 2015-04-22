// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_storage_namespace_impl.h"

#include <stdio.h>

#include "third_party/WebKit/public/platform/WebStorageArea.h"

namespace html_viewer {
namespace {

class DummyWebStorageAreaImpl : public blink::WebStorageArea {
 public:
  unsigned length() override { return 0; }
  blink::WebString key(unsigned index) override { return blink::WebString(); }
  blink::WebString getItem(const blink::WebString& key) override {
    return blink::WebString();
  }
};

}  // namespace

WebStorageNamespaceImpl::WebStorageNamespaceImpl() {
}

WebStorageNamespaceImpl::~WebStorageNamespaceImpl() {
}

blink::WebStorageArea* WebStorageNamespaceImpl::createStorageArea(
    const blink::WebString& origin) {
  return new DummyWebStorageAreaImpl();
}

bool WebStorageNamespaceImpl::isSameNamespace(
    const blink::WebStorageNamespace&) const {
  return false;
}

}  // namespace html_viewer
