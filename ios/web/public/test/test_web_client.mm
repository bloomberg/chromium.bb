// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/test_web_client.h"

#include "base/logging.h"

namespace web {

TestWebClient::TestWebClient() {}

TestWebClient::~TestWebClient() {}

NSString* TestWebClient::GetEarlyPageScript() const {
  return early_page_script_ ? early_page_script_.get() : @"";
}

bool TestWebClient::WebViewsNeedActiveStateManager() const {
  return true;
}

void TestWebClient::SetEarlyPageScript(NSString* page_script) {
  early_page_script_.reset([page_script copy]);
}

}  // namespace web
