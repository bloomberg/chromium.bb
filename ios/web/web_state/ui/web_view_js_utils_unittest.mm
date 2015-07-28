// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/web_view_js_utils.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/test/ios/wait_util.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/test/web_test.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

// Synchronously returns result of web::EvaluateJavaScript call.
template <typename WebView>
NSString* EvaluateJavaScript(WebView web_view, NSString* js) {
  __block bool evaluation_completed = false;
  __block base::scoped_nsobject<NSString> evaluation_result;
  web::EvaluateJavaScript(web_view, js, ^(NSString* result, NSError* error) {
      DCHECK(!error);
      evaluation_result.reset([result copy]);
      evaluation_completed = true;
  });
  base::test::ios::WaitUntilCondition(^bool() {
    return evaluation_completed;
  });
  return [[evaluation_result copy] autorelease];
}

// Base test fixture for web::EvaluateJavaScript testing.
typedef web::WebTest WebViewJSUtilsTest;

// Test fixture for web::EvaluateJavaScript(UIWebView*..) testing.
class UIWebViewJSUtilsTest : public WebViewJSUtilsTest {
 protected:
  void SetUp() override {
    WebViewJSUtilsTest::SetUp();
    web_view_.reset(web::CreateWebView(CGRectZero));
  }
  // UIWebView created for testing.
  base::scoped_nsobject<UIWebView> web_view_;
};

// Test fixture for web::EvaluateJavaScript(WKWebView*..) testing.
class WKWebViewJSUtilsTest : public WebViewJSUtilsTest {
 protected:
  void SetUp() override {
    // SetUp crashes on iOS 7.
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    WebViewJSUtilsTest::SetUp();
    web_view_.reset(web::CreateWKWebView(CGRectZero, GetBrowserState()));
  }
  // WKWebView created for testing.
  base::scoped_nsobject<WKWebView> web_view_;
};

// Tests that a script with undefined result correctly evaluates to string.
WEB_TEST_F(UIWebViewJSUtilsTest, WKWebViewJSUtilsTest, UndefinedEvaluation) {
  EXPECT_NSEQ(@"", EvaluateJavaScript(this->web_view_, @"{}"));
}

// Tests that a script with string result correctly evaluates to string.
WEB_TEST_F(UIWebViewJSUtilsTest, WKWebViewJSUtilsTest, StringEvaluation) {
  EXPECT_NSEQ(@"test", EvaluateJavaScript(this->web_view_, @"'test'"));
}

// Tests that a script with number result correctly evaluates to string.
WEB_TEST_F(UIWebViewJSUtilsTest, WKWebViewJSUtilsTest, NumberEvaluation) {
  EXPECT_NSEQ(@"-1", EvaluateJavaScript(this->web_view_, @"-1"));
  EXPECT_NSEQ(@"0", EvaluateJavaScript(this->web_view_, @"0"));
  EXPECT_NSEQ(@"1", EvaluateJavaScript(this->web_view_, @"1"));
  EXPECT_NSEQ(@"3.14", EvaluateJavaScript(this->web_view_, @"3.14"));
}

// Tests that a script with bool result correctly evaluates to string.
WEB_TEST_F(UIWebViewJSUtilsTest, WKWebViewJSUtilsTest, BoolEvaluation) {
  EXPECT_NSEQ(@"true", EvaluateJavaScript(this->web_view_, @"true"));
  EXPECT_NSEQ(@"false", EvaluateJavaScript(this->web_view_, @"false"));
}

// Tests that a script with null result correctly evaluates to empty string.
WEB_TEST_F(UIWebViewJSUtilsTest, WKWebViewJSUtilsTest, NullEvaluation) {
  EXPECT_NSEQ(@"", EvaluateJavaScript(this->web_view_, @"null"));
}

}  // namespace
