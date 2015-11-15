// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#import <WebKit/WebKit.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/ios/wait_util.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/response_providers/string_response_provider.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/test/web_int_test.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

// A WKNavigationDelegate that is used to check if a WKWebView has finished
// a navigation. Used for testing purposes.
@interface TestNavigationDelegate : NSObject <WKNavigationDelegate>
// YES if a navigation has finished.
@property (nonatomic, assign) BOOL didFinishNavigation;
@end

@implementation TestNavigationDelegate

@synthesize didFinishNavigation = _didFinishNavigation;

- (void)webView:(WKWebView*)webView
    didFinishNavigation:(WKNavigation*)navigation {
  self.didFinishNavigation = YES;
}

@end

// A test fixture for testing that browsing data is partitioned between
// web views created with a non-OTR BrowserState and web views created with an
// OTR BrowserState.
class BrowserStateWebViewPartitionTest : public web::WebIntTest {
 protected:
  void SetUp() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    web::WebIntTest::SetUp();

    otr_browser_state_.SetOffTheRecord(true);

    web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
    ASSERT_TRUE(server.IsRunning());

    provider_.reset(new web::StringResponseProvider("Hello World"));
    server.AddResponseProvider(provider_.get());
  }

  void TearDown() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();

    web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
    server.RemoveResponseProvider(provider_.release());

    web::WebIntTest::TearDown();
  }

  // Runs the given JavaScript on |web_view| and returns the result as a string.
  NSString* EvaluateJavaScript(WKWebView* web_view,
                               NSString* js) {
    __block base::scoped_nsobject<NSString> result;
    __block bool block_was_called = false;
    web::EvaluateJavaScript(web_view, js, ^(NSString* js_result, NSError*) {
      result.reset([js_result copy]);
      block_was_called = true;
    });
    base::test::ios::WaitUntilCondition(^bool {
      return block_was_called;
    });
    return [[result copy] autorelease];
  }

  // Sets a persistent cookie with key, value on |web_view|.
  void SetCookie(NSString* key, NSString* value, WKWebView* web_view) {
    NSString* set_cookie = [NSString
        stringWithFormat:@"document.cookie='%@=%@;"
                         @"Expires=Tue, 05-May-9999 02:18:23 GMT; Path=/'",
                         key, value];
    EvaluateJavaScript(web_view, set_cookie);
  }

  // Returns a csv list of all cookies from |web_view|.
  NSString* GetCookies(WKWebView* web_view) {
    return EvaluateJavaScript(web_view, @"document.cookie");
  }

  // Sets a localstorage key, value pair on |web_view|.
  void SetLocalStorageItem(NSString* key,
                           NSString* value,
                           WKWebView* web_view) {
    NSString* set_local_storage_item = [NSString
        stringWithFormat:@"localStorage.setItem('%@', '%@')", key, value];
    EvaluateJavaScript(web_view, set_local_storage_item);
  }

  // Returns the localstorage value associated with |key| from |web_view|.
  NSString* GetLocalStorageItem(NSString* key, WKWebView* web_view) {
    NSString* get_local_storage_value =
        [NSString stringWithFormat:@"localStorage.getItem('%@');", key];
    return EvaluateJavaScript(web_view, get_local_storage_value);
  }

  // Loads a test web page (that contains a small string) in |web_view| and
  // waits until the web view has finished the navigation.
  void LoadTestWebPage(WKWebView* web_view) {
    DCHECK(web_view);

    base::scoped_nsobject<TestNavigationDelegate> navigation_delegate(
        [[TestNavigationDelegate alloc] init]);

    id old_navigation_delegate = web_view.navigationDelegate;
    web_view.navigationDelegate = navigation_delegate;

    web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
    ASSERT_TRUE(server.IsRunning());

    NSURL* url = net::NSURLWithGURL(server.MakeUrl("http://whatever/"));
    [web_view loadRequest:[NSURLRequest requestWithURL:url]];
    base::test::ios::WaitUntilCondition(^bool {
      return [navigation_delegate didFinishNavigation];
    });

    web_view.navigationDelegate = old_navigation_delegate;
  }

  // Returns the BrowserState that is used for testing.
  web::BrowserState* GetOtrBrowserState() { return &otr_browser_state_; }

 private:
  // The ResponseProvider used to load a simple web page.
  scoped_ptr<web::ResponseProvider> provider_;
  // The OTR browser state used in tests.
  web::TestBrowserState otr_browser_state_;
};

// Tests that cookies are partitioned between web views created with a
// non-OTR BrowserState and an OTR BrowserState.
TEST_F(BrowserStateWebViewPartitionTest, Cookies) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  base::scoped_nsobject<WKWebView> web_view_1(
      web::CreateWKWebView(CGRectZero, GetBrowserState()));
  LoadTestWebPage(web_view_1);
  SetCookie(@"someCookieName1", @"someCookieValue1", web_view_1);
  EXPECT_NSEQ(@"someCookieName1=someCookieValue1", GetCookies(web_view_1));

  base::scoped_nsobject<WKWebView> web_view_2(
      web::CreateWKWebView(CGRectZero, GetOtrBrowserState()));
  LoadTestWebPage(web_view_2);

  // Test that the cookie has not leaked over to |web_view_2|.
  EXPECT_NSEQ(@"", GetCookies(web_view_2));

  SetCookie(@"someCookieName2", @"someCookieValue2", web_view_2);
  EXPECT_NSEQ(@"someCookieName2=someCookieValue2", GetCookies(web_view_2));

  // Test that the cookie has not leaked over to |web_view_1|.
  NSString* cookies = GetCookies(web_view_1);
  EXPECT_FALSE([cookies containsString:@"someCookieName2"]);
}

// Tests that localStorage is partitioned between web views created with a
// non-OTR BrowserState and an OTR BrowserState.
TEST_F(BrowserStateWebViewPartitionTest, LocalStorage) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  base::scoped_nsobject<WKWebView> web_view_1(
      web::CreateWKWebView(CGRectZero, GetBrowserState()));
  LoadTestWebPage(web_view_1);
  SetLocalStorageItem(@"someKey1", @"someValue1", web_view_1);
  EXPECT_NSEQ(@"someValue1", GetLocalStorageItem(@"someKey1", web_view_1));

  base::scoped_nsobject<WKWebView> web_view_2(
      web::CreateWKWebView(CGRectZero, GetOtrBrowserState()));
  LoadTestWebPage(web_view_2);

  // Test that LocalStorage has not leaked over to |web_view_2|.
  EXPECT_NSEQ(@"", GetLocalStorageItem(@"someKey1", web_view_2));

  SetLocalStorageItem(@"someKey2", @"someValue2", web_view_2);
  // Due to platform limitation, it's not possible to actually set localStorage
  // item on an OTR BrowserState. Therefore, it's not possible to verify that a
  // localStorage item has been correctly set.
  // Look at
  // http://stackoverflow.com/questions/14555347/html5-localstorage-error-with-safari-quota-exceeded-err-dom-exception-22-an
  // for more details.
  // Test that LocalStorage has not leaked over to |web_view_1|.
  EXPECT_NSEQ(@"", GetLocalStorageItem(@"someKey2", web_view_1));
}
