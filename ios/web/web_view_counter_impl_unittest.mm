// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view_counter_impl.h"

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/test/web_test_util.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_view_counter.h"
#import "ios/web/public/web_view_creation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {

// A test fixture that sets up web-related classes for testing a
// |WebViewCounter|.
class WebViewCounterTest : public PlatformTest {
 public:
  WebViewCounterTest() : web_client_(make_scoped_ptr(new web::TestWebClient)) {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_.reset(new TestBrowserState());
  }

  // Used to create TestWebThreads.
  TestWebThreadBundle thread_bundle_;
  // Required by web::CreateWebView/web::CreateWKWebView functions.
  web::ScopedTestingWebClient web_client_;

  // The BrowserState used for testing purposes.
  scoped_ptr<BrowserState> browser_state_;
};

}  // namespace

// Tests that WebViewCounter correctly maintains the count of WKWebViews.
TEST_F(WebViewCounterTest, WKWebViewCount) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  WebViewCounterImpl* web_view_counter =
      WebViewCounterImpl::FromBrowserState(browser_state_.get());
  ASSERT_TRUE(web_view_counter);

  base::scoped_nsobject<WKWebView> web_view_1(
      web::CreateWKWebView(CGRectZero, browser_state_.get()));
  web_view_counter->InsertWKWebView(web_view_1);
  base::scoped_nsobject<WKWebView> web_view_2(
      web::CreateWKWebView(CGRectZero, browser_state_.get()));
  web_view_counter->InsertWKWebView(web_view_2);

  EXPECT_EQ(2U, web_view_counter->GetWKWebViewCount());

  // Deallocate WKWebViews.
  web_view_2.reset();
  EXPECT_EQ(1U, web_view_counter->GetWKWebViewCount());
  web_view_1.reset();
  EXPECT_EQ(0U, web_view_counter->GetWKWebViewCount());
}

}  // namespace web
