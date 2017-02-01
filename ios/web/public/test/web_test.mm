// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/web_test.h"

#include "base/memory/ptr_util.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/web_state/global_web_state_observer.h"
#import "ios/web/public/test/fakes/test_web_client.h"

namespace web {

class WebTestRenderProcessCrashObserver : public GlobalWebStateObserver {
 public:
  WebTestRenderProcessCrashObserver() = default;
  ~WebTestRenderProcessCrashObserver() override = default;

  void RenderProcessGone(WebState* web_state) override {
    FAIL() << "Renderer process died unexpectedly during the test";
  }
};

WebTest::WebTest()
    : web_client_(base::WrapUnique(new TestWebClient)),
      crash_observer_(base::MakeUnique<WebTestRenderProcessCrashObserver>()) {}

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

void WebTest::SetIgnoreRenderProcessCrashesDuringTesting(bool allow) {
  if (allow) {
    crash_observer_ = nullptr;
  } else {
    crash_observer_ = base::MakeUnique<WebTestRenderProcessCrashObserver>();
  }
}

}  // namespace web
