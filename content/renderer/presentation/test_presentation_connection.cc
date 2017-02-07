// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/test_presentation_connection.h"

#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnectionProxy.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationController.h"

namespace content {

TestPresentationConnection::TestPresentationConnection() = default;

TestPresentationConnection::~TestPresentationConnection() = default;

void TestPresentationConnection::bindProxy(
    std::unique_ptr<blink::WebPresentationConnectionProxy> proxy) {
  proxy_ = std::move(proxy);
}

blink::WebPresentationConnectionProxy* TestPresentationConnection::proxy() {
  return proxy_.get();
}

}  // namespace content
