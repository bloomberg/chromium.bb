// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <WebKit/WebKit.h>

#import "ios/web_view/test/web_view_test.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {

// Creates web view with incognito configuration and frame equal to screen
// bounds.
CWVWebView* CreateIncognitoWebView() {
  return test::CreateWebView([CWVWebViewConfiguration incognitoConfiguration]);
}

}  // namespace

// Test fixture for incognito browsing mode.
typedef ios_web_view::WebViewTest WebViewIncognitoTest;

// Tests that browsing data (cookie and localStorage) does not leak from
// non-incognito to incognito web view.
TEST_F(WebViewIncognitoTest, BrowsingDataNotLeakingToIncognito) {
  // CWVWebView does not allow JavaScript execution if the page was not loaded.
  GURL url = GetUrlForPageWithHtmlBody(std::string());
  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(url)));

  NSError* error = nil;
  test::EvaluateJavaScript(web_view_, @"localStorage.setItem('k', 'v');",
                           &error);
  ASSERT_NSEQ(nil, error);
  test::EvaluateJavaScript(web_view_, @"document.cookie='n=v;'", &error);
  ASSERT_NSEQ(nil, error);

  // Create web view with the same configuration, otherwise browswing data may
  // not be shared immidiately. Make sure that new web view has browsing data
  // from the previous web view.
  CWVWebView* non_incognito_web_view =
      test::CreateWebView([web_view_ configuration]);
  ASSERT_TRUE(test::LoadUrl(non_incognito_web_view, net::NSURLWithGURL(url)));
  id localStorageValue = test::EvaluateJavaScript(
      non_incognito_web_view, @"localStorage.getItem('k');", &error);
  ASSERT_NSEQ(nil, error);
  ASSERT_NSEQ(@"v", localStorageValue);
  id cookie = test::EvaluateJavaScript(non_incognito_web_view,
                                       @"document.cookie", &error);
  ASSERT_NSEQ(nil, error);
  ASSERT_TRUE([cookie containsString:@"n=v"]);

  // Verify that incognito web view does not have browsing data from
  // non-incognito web view.
  CWVWebView* incognito_web_view = CreateIncognitoWebView();
  ASSERT_TRUE(incognito_web_view);
  ASSERT_TRUE(test::LoadUrl(incognito_web_view, net::NSURLWithGURL(url)));
  localStorageValue = test::EvaluateJavaScript(
      incognito_web_view, @"localStorage.getItem('k');", &error);
  EXPECT_NSEQ(nil, error);
  ASSERT_NSEQ([NSNull null], localStorageValue);
  cookie =
      test::EvaluateJavaScript(incognito_web_view, @"document.cookie", &error);
  EXPECT_NSEQ(nil, error);
  ASSERT_NSEQ(@"", cookie);
}

// Tests that browsing data (cookie and localStorage) does not leak from
// incognito to non-incognito web view.
TEST_F(WebViewIncognitoTest, BrowsingDataNotLeakingFromIncognito) {
  // CWVWebView does not allow JavaScript execution if the page was not loaded.
  CWVWebView* incognito_web_view = CreateIncognitoWebView();
  GURL url = GetUrlForPageWithHtmlBody(std::string());
  ASSERT_TRUE(test::LoadUrl(incognito_web_view, net::NSURLWithGURL(url)));

  NSError* error = nil;
  test::EvaluateJavaScript(incognito_web_view,
                           @"localStorage.setItem('k2', 'v');", &error);
  // |localStorage.setItem| throws exception in Incognito.
  ASSERT_EQ(WKErrorJavaScriptExceptionOccurred, error.code);
  test::EvaluateJavaScript(incognito_web_view, @"document.cookie='n2=v;'",
                           &error);
  ASSERT_NSEQ(nil, error);

  // Create incognito web view with the same configuration, otherwise browswing
  // data will not be shared. Make sure that new incognito web view has browsing
  // data from the previous incognito web view.
  CWVWebView* incognito_web_view2 =
      test::CreateWebView([incognito_web_view configuration]);
  ASSERT_TRUE(test::LoadUrl(incognito_web_view2, net::NSURLWithGURL(url)));
  id localStorageValue = test::EvaluateJavaScript(
      incognito_web_view2, @"localStorage.getItem('k2');", &error);
  ASSERT_NSEQ(nil, error);
  ASSERT_NSEQ([NSNull null], localStorageValue);
  id cookie =
      test::EvaluateJavaScript(incognito_web_view2, @"document.cookie", &error);
  ASSERT_NSEQ(nil, error);
  ASSERT_TRUE([cookie containsString:@"n2=v"]);

  // Verify that non-incognito web view does not have browsing data from
  // incognito web view.
  ASSERT_TRUE(web_view_);
  ASSERT_TRUE(test::LoadUrl(web_view_, net::NSURLWithGURL(url)));
  localStorageValue = test::EvaluateJavaScript(
      web_view_, @"localStorage.getItem('k2');", &error);
  EXPECT_NSEQ(nil, error);
  ASSERT_NSEQ([NSNull null], localStorageValue);
  cookie = test::EvaluateJavaScript(web_view_, @"document.cookie", &error);
  EXPECT_NSEQ(nil, error);
  ASSERT_FALSE([cookie containsString:@"n2=v"]);
}

}  // namespace ios_web_view
