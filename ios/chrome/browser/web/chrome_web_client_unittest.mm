// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_web_client.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/payments/core/features.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/credential_manager_features.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/web_view_creation_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ChromeWebClientTest : public PlatformTest {
 public:
  ChromeWebClientTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  ~ChromeWebClientTest() override = default;

  ios::ChromeBrowserState* browser_state() { return browser_state_.get(); }

 private:
  base::test::ScopedTaskEnvironment environment_;
  std::unique_ptr<ios::ChromeBrowserState> browser_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebClientTest);
};

TEST_F(ChromeWebClientTest, UserAgent) {
  std::vector<std::string> pieces;

  // Check if the pieces of the user agent string come in the correct order.
  ChromeWebClient web_client;
  std::string buffer = web_client.GetUserAgent(web::UserAgentType::MOBILE);

  pieces = base::SplitStringUsingSubstr(
      buffer, "Mozilla/5.0 (", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  EXPECT_EQ("", pieces[0]);

  pieces = base::SplitStringUsingSubstr(
      buffer, ") AppleWebKit/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string os_str = pieces[0];

  pieces =
      base::SplitStringUsingSubstr(buffer, " (KHTML, like Gecko) ",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string webkit_version_str = pieces[0];

  pieces = base::SplitStringUsingSubstr(
      buffer, " Safari/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  std::string product_str = pieces[0];
  std::string safari_version_str = pieces[1];

  // Not sure what can be done to better check the OS string, since it's highly
  // platform-dependent.
  EXPECT_FALSE(os_str.empty());

  EXPECT_FALSE(webkit_version_str.empty());
  EXPECT_FALSE(safari_version_str.empty());

  EXPECT_EQ(0u, product_str.find("CriOS/"));
}

// Tests that ChromeWebClient provides print script for WKWebView.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptPrint) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(base::MakeUnique<ChromeWebClient>());
  NSString* script = web_client.Get()->GetEarlyPageScript(browser_state());
  web::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"object",
              web::ExecuteJavaScript(web_view, @"typeof __gCrWeb.print"));
}

// Tests that ChromeWebClient provides credential manager script for WKWebView
// if and only if the feature is enabled.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptCredentialManager) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(base::MakeUnique<ChromeWebClient>());
  NSString* script = web_client.Get()->GetEarlyPageScript(browser_state());
  web::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"undefined", web::ExecuteJavaScript(
                                web_view, @"typeof navigator.credentials"));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      credential_manager::features::kCredentialManager);
  script = web_client.Get()->GetEarlyPageScript(browser_state());
  web::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"object", web::ExecuteJavaScript(
                             web_view, @"typeof navigator.credentials"));
}

// Tests that ChromeWebClient provides payment request script for WKWebView if
// the feature is enabled.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptPaymentRequestEnabled) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(base::MakeUnique<ChromeWebClient>());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(payments::features::kWebPayments);
  NSString* script = web_client.Get()->GetEarlyPageScript(browser_state());
  web::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"function", web::ExecuteJavaScript(
                               web_view, @"typeof window.PaymentRequest"));
}

// Tests that ChromeWebClient does not provide payment request script for
// WKWebView if the feature is disabled.
TEST_F(ChromeWebClientTest, WKWebViewEarlyPageScriptPaymentRequestDisabled) {
  // Chrome scripts rely on __gCrWeb object presence.
  WKWebView* web_view = web::BuildWKWebView(CGRectZero, browser_state());
  web::ExecuteJavaScript(web_view, @"__gCrWeb = {};");

  web::ScopedTestingWebClient web_client(base::MakeUnique<ChromeWebClient>());
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(payments::features::kWebPayments);
  NSString* script = web_client.Get()->GetEarlyPageScript(browser_state());
  web::ExecuteJavaScript(web_view, script);
  EXPECT_NSEQ(@"undefined", web::ExecuteJavaScript(
                                web_view, @"typeof window.PaymentRequest"));
}

}  // namespace
