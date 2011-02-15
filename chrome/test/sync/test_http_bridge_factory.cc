// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/sync/test_http_bridge_factory.h"

namespace browser_sync {

bool TestHttpBridge::MakeSynchronousPost(int* os_error_code,
                                         int* response_code) {
  return false;
}

int TestHttpBridge::GetResponseContentLength() const {
  return 0;
}

const char* TestHttpBridge::GetResponseContent() const {
  return 0;
}

const std::string TestHttpBridge::GetResponseHeaderValue(
    const std::string &) const {
  return std::string();
}

TestHttpBridgeFactory::TestHttpBridgeFactory() {}

TestHttpBridgeFactory::~TestHttpBridgeFactory() {}

sync_api::HttpPostProviderInterface* TestHttpBridgeFactory::Create() {
  return new TestHttpBridge();
}

void TestHttpBridgeFactory::Destroy(sync_api::HttpPostProviderInterface* http) {
  delete http;
}

}  // namespace browser_sync
