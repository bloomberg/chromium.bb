// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_web_view_crash_detector.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/test/web_test.h"
#import "ios/web/test/wk_web_view_crash_utils.h"

namespace {

// A test fixture for testing CRWWKWebViewCrashDetector.
typedef web::WebTest CRWWKWebViewCrashDetectorTest;

// Tests that crash is reported for WKWebView if
// WKErrorWebContentProcessTerminated happend during JavaScript evaluation.
TEST_F(CRWWKWebViewCrashDetectorTest, Crash) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  __block bool crashed = false;

  base::scoped_nsobject<WKWebView> terminated_web_view(
      web::CreateTerminatedWKWebView());

  base::scoped_nsprotocol<id> crash_detector(
      [[CRWWKWebViewCrashDetector alloc]
          initWithWebView:terminated_web_view
             crashHandler:^{ crashed = true; }]);

  web::SimulateWKWebViewCrash(terminated_web_view);

  EXPECT_TRUE(crashed);
}

// Tests that crash is not reported for healthy WKWebView.
TEST_F(CRWWKWebViewCrashDetectorTest, NoCrash) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  base::scoped_nsobject<WKWebView> healthy_web_view(
      web::CreateHealthyWKWebView());
  base::scoped_nsprotocol<id> crash_detector(
      [[CRWWKWebViewCrashDetector alloc]
          initWithWebView:healthy_web_view
             crashHandler:^{ GTEST_FAIL(); }]);

  web::SimulateWKWebViewCrash(healthy_web_view);
}

}  // namespace
