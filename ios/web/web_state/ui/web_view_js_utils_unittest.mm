// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/web_view_js_utils.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/test/ios/wait_util.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/web_view_internal_creation_util.h"
#import "ios/web/test/web_test.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {

// Test fixture for web::EvaluateJavaScript testing.
class WebViewJsUtilsTest : public web::WebTest {
 protected:
  void SetUp() override {
    web::WebTest::SetUp();
    web_view_.reset(web::CreateWKWebView(CGRectZero, GetBrowserState()));
  }
  // Synchronously returns result of web::EvaluateJavaScript call.
  NSString* EvaluateJavaScript(NSString* js) {
    __block bool evaluation_completed = false;
    __block base::scoped_nsobject<NSString> evaluation_result;
    web::EvaluateJavaScript(web_view_, js, ^(NSString* result, NSError* error) {
      DCHECK(!error);
      evaluation_result.reset([result copy]);
      evaluation_completed = true;
    });
    base::test::ios::WaitUntilCondition(^{
      return evaluation_completed;
    });
    return [[evaluation_result copy] autorelease];
  }

 private:
  // WKWebView created for testing.
  base::scoped_nsobject<WKWebView> web_view_;
};

// Tests that a script with undefined result correctly evaluates to string.
TEST_F(WebViewJsUtilsTest, UndefinedEvaluation) {
  EXPECT_NSEQ(@"", EvaluateJavaScript(@"{}"));
}

// Tests that a script with string result correctly evaluates to string.
TEST_F(WebViewJsUtilsTest, StringEvaluation) {
  EXPECT_NSEQ(@"test", EvaluateJavaScript(@"'test'"));
}

// Tests that a script with number result correctly evaluates to string.
TEST_F(WebViewJsUtilsTest, NumberEvaluation) {
  EXPECT_NSEQ(@"-1", EvaluateJavaScript(@"-1"));
  EXPECT_NSEQ(@"0", EvaluateJavaScript(@"0"));
  EXPECT_NSEQ(@"1", EvaluateJavaScript(@"1"));
  EXPECT_NSEQ(@"3.14", EvaluateJavaScript(@"3.14"));
}

// Tests that a script with bool result correctly evaluates to string.
TEST_F(WebViewJsUtilsTest, BoolEvaluation) {
  EXPECT_NSEQ(@"true", EvaluateJavaScript(@"true"));
  EXPECT_NSEQ(@"false", EvaluateJavaScript(@"false"));
}

// Tests that a script with null result correctly evaluates to empty string.
TEST_F(WebViewJsUtilsTest, NullEvaluation) {
  EXPECT_NSEQ(@"", EvaluateJavaScript(@"null"));
}

}  // namespace web
