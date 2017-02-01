// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_WEB_TEST_H_
#define IOS_WEB_PUBLIC_TEST_WEB_TEST_H_

#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/platform_test.h"

namespace web {

class BrowserState;
class TestWebClient;
class WebTestRenderProcessCrashObserver;

// A test fixture for web tests that need a minimum environment set up that
// mimics a web embedder.
class WebTest : public PlatformTest {
 protected:
  WebTest();
  ~WebTest() override;

  // PlatformTest methods.
  void SetUp() override;
  void TearDown() override;

  // Returns the WebClient that is used for testing.
  TestWebClient* GetWebClient();

  // Returns the BrowserState that is used for testing.
  virtual BrowserState* GetBrowserState();

  // If called with |true|, prevents the test fixture from automatically failing
  // when a render process crashes during the test.  This is useful for tests
  // that intentionally crash the render process.  By default, the WebTest
  // fixture will fail if a render process crashes.
  void SetIgnoreRenderProcessCrashesDuringTesting(bool allow);

 private:
  // The WebClient used in tests.
  ScopedTestingWebClient web_client_;
  // The threads used for testing.
  web::TestWebThreadBundle thread_bundle_;
  // The browser state used in tests.
  TestBrowserState browser_state_;

  // Triggers test failures if a render process dies during the test.
  std::unique_ptr<WebTestRenderProcessCrashObserver> crash_observer_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_WEB_TEST_H_
