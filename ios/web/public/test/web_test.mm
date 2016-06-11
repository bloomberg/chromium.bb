// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/web_test.h"

#include "base/memory/ptr_util.h"
#import "ios/web/public/active_state_manager.h"
#import "ios/web/public/test/test_web_client.h"

namespace web {

WebTest::WebTest() : web_client_(base::WrapUnique(new TestWebClient)) {}

WebTest::~WebTest() {}

void WebTest::SetUp() {
  PlatformTest::SetUp();
  BrowserState::GetActiveStateManager(&browser_state_)->SetActive(true);
}

void WebTest::TearDown() {
  BrowserState::GetActiveStateManager(&browser_state_)->SetActive(false);
  PlatformTest::TearDown();
}

TestWebClient* WebTest::GetWebClient() {
  return static_cast<TestWebClient*>(web_client_.Get());
}

BrowserState* WebTest::GetBrowserState() {
  return &browser_state_;
}

}  // namespace web
