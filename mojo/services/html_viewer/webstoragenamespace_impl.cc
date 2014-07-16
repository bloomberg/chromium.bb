// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/webstoragenamespace_impl.h"

#include <stdio.h>

#include "third_party/WebKit/public/platform/WebStorageArea.h"

namespace mojo {
namespace {

class DummyWebStorageAreaImpl : public blink::WebStorageArea {
 public:
  virtual unsigned length() {
    return 0;
  }
  virtual blink::WebString key(unsigned index) {
    return blink::WebString();
  }
  virtual blink::WebString getItem(const blink::WebString& key) {
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

}  // namespace mojo
