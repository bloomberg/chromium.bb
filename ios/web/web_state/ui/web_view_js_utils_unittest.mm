// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/web_view_js_utils.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
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

// Tests that ValueResultFromWKResult converts nil value to nullptr.
TEST_F(WebViewJsUtilsTest, ValueResultFromUndefinedWKResult) {
  EXPECT_FALSE(ValueResultFromWKResult(nil));
}

// Tests that ValueResultFromWKResult converts string to Value::TYPE_STRING.
TEST_F(WebViewJsUtilsTest, ValueResultFromStringWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@"test"));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_STRING, value->GetType());
  std::string converted_result;
  value->GetAsString(&converted_result);
  EXPECT_EQ("test", converted_result);
}

// Tests that ValueResultFromWKResult converts inetger to Value::TYPE_DOUBLE.
// NOTE: WKWebView API returns all numbers as kCFNumberFloat64Type, so there is
// no way to tell if the result is integer or double.
TEST_F(WebViewJsUtilsTest, ValueResultFromIntegerWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@1));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_DOUBLE, value->GetType());
  double converted_result = 0;
  value->GetAsDouble(&converted_result);
  EXPECT_EQ(1, converted_result);
}

// Tests that ValueResultFromWKResult converts double to Value::TYPE_DOUBLE.
TEST_F(WebViewJsUtilsTest, ValueResultFromDoubleWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@3.14));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_DOUBLE, value->GetType());
  double converted_result = 0;
  value->GetAsDouble(&converted_result);
  EXPECT_EQ(3.14, converted_result);
}

// Tests that ValueResultFromWKResult converts bool to Value::TYPE_BOOLEAN.
TEST_F(WebViewJsUtilsTest, ValueResultFromBoolWKResult) {
  std::unique_ptr<base::Value> value(web::ValueResultFromWKResult(@YES));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, value->GetType());
  bool converted_result = false;
  value->GetAsBoolean(&converted_result);
  EXPECT_TRUE(converted_result);
}

// Tests that ValueResultFromWKResult converts null to Value::TYPE_NULL.
TEST_F(WebViewJsUtilsTest, ValueResultFromNullWKResult) {
  std::unique_ptr<base::Value> value(
      web::ValueResultFromWKResult([NSNull null]));
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::TYPE_NULL, value->GetType());
}

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
