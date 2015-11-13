// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_simple_web_view_controller.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/mac/scoped_nsobject.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace {

// A test fixture for testing CRWWKSimpleWebViewController.
class CRWWKSimpleWebViewControllerTest : public web::WebTest {
 protected:
  void SetUp() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    web::WebTest::SetUp();
    mock_web_view_.reset(
        [[OCMockObject niceMockForClass:[WKWebView class]] retain]);
    web_view_controller_.reset([[CRWWKSimpleWebViewController alloc]
        initWithWKWebView:mock_web_view_]);
    mock_delegate_.reset([[OCMockObject
        mockForProtocol:@protocol(CRWSimpleWebViewControllerDelegate)] retain]);
    [web_view_controller_ setDelegate:mock_delegate_];
  }

  void TearDown() override {
    CR_TEST_REQUIRES_WK_WEB_VIEW();
    web::WebTest::TearDown();
  }

  // Tests that |shouldStartLoadWithRequest:| decision by the delegate is
  // respected by the CRWSWKimpleWebViewController.
  void TestShouldStartLoadRespected(BOOL expected_decision) {
    NSURLRequest* request = [NSURLRequest requestWithURL:
        [NSURL URLWithString:@"http://google.com"]];
    base::scoped_nsobject<WKNavigationAction> navigation_action(
        [[OCMockObject mockForClass:[WKNavigationAction class]] retain]);
    [[[static_cast<OCMockObject*>(navigation_action.get()) stub]
        andReturn:request] request];
    [[[mock_delegate_ expect] andReturnValue:OCMOCK_VALUE(expected_decision)]
                     simpleWebViewController:web_view_controller_
                  shouldStartLoadWithRequest:request];

    __block BOOL decision_handler_block_was_called = NO;
    id decision_handler_block = ^void(WKNavigationActionPolicy policy) {
        decision_handler_block_was_called = YES;
        WKNavigationActionPolicy expected_policy = expected_decision ?
            WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel;
        EXPECT_EQ(expected_policy , policy);
    };
    [static_cast<id<WKNavigationDelegate>>(web_view_controller_.get())
                                webView:mock_web_view_
        decidePolicyForNavigationAction:navigation_action
                        decisionHandler:decision_handler_block];
    EXPECT_TRUE(decision_handler_block_was_called);
  }

  base::scoped_nsobject<id> mock_web_view_;
  base::scoped_nsobject<id> mock_delegate_;
  base::scoped_nsobject<CRWWKSimpleWebViewController> web_view_controller_;
};

// Tests to make sure a CRWWKSimpleWebViewController correctly sets the backing
// WKWebView.
TEST_F(CRWWKSimpleWebViewControllerTest, View) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  EXPECT_EQ(mock_web_view_.get(), [web_view_controller_ view]);
}

// Tests to make sure a CRWWKSimpleWebViewController correctly sets the backing
// WKWebView's navigation delegate
TEST_F(CRWWKSimpleWebViewControllerTest, Delegate) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  base::scoped_nsobject<WKWebView> web_view(
      web::CreateWKWebView(CGRectZero, GetBrowserState()));
  ASSERT_TRUE(web_view);
  web_view_controller_.reset([[CRWWKSimpleWebViewController alloc]
      initWithWKWebView:web_view]);
  EXPECT_EQ(static_cast<id<WKNavigationDelegate>>(web_view_controller_.get()),
            [web_view navigationDelegate]);
}

// Tests that CRWWKSimpleWebViewController can correctly retrieve the title from
// the underlying WKWebView.
TEST_F(CRWWKSimpleWebViewControllerTest, Title) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  [[[mock_web_view_ stub] andReturn:@"The Rolling Stones"] title];
  EXPECT_NSEQ(@"The Rolling Stones", [web_view_controller_ title]);
}

// Tests that CRWWKSimpleWebViewController correctly reloads from the underlying
// WKWebView.
TEST_F(CRWWKSimpleWebViewControllerTest, Reload) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  [static_cast<WKWebView*>([mock_web_view_ expect]) reload];
  [web_view_controller_ reload];
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that CRWWKSimpleWebViewController correctly returns the web view's
// scroll view.
TEST_F(CRWWKSimpleWebViewControllerTest, ScrollView) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  base::scoped_nsobject<UIScrollView> scroll_view([[UIScrollView alloc] init]);
  [[[mock_web_view_ stub] andReturn:scroll_view] scrollView];
  EXPECT_EQ(scroll_view, [web_view_controller_ scrollView]);
}

// Tests that CRWWKSimpleWebViewControllerTest correctly loads a request from
// the underlying WKWebView.
TEST_F(CRWWKSimpleWebViewControllerTest, LoadRequest) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://google.com"]];
  [static_cast<WKWebView*>([mock_web_view_ expect]) loadRequest:request];
  [web_view_controller_ loadRequest:request];
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that CRWWKSimpleWebViewControllerTest correctly loads a html request
// from the underlying WKWebView.
TEST_F(CRWWKSimpleWebViewControllerTest, LoadHTMLString) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  NSURL* base_url = [NSURL URLWithString:@"http://google.com"];
  NSString* html_string = @"<html>Some html</html>";
  [static_cast<WKWebView*>([mock_web_view_ expect]) loadHTMLString:html_string
                                                           baseURL:base_url];
  [web_view_controller_ loadHTMLString:html_string baseURL:base_url];
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that CRWWKSimpleWebViewControllerTest correctly calls
// |shouldStartLoadWithRequest:| on the delegate when a navigation delegate
// callback is recieved.
// TODO(shreyasv): Revisit this test if mockHTTPServer is moved to
// ios_web_unittests.
TEST_F(CRWWKSimpleWebViewControllerTest, ShouldStartLoad) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  NSURLRequest* request = [NSURLRequest requestWithURL:
      [NSURL URLWithString:@"http://google.com"]];
  base::scoped_nsobject<WKNavigationAction> navigation_action(
      [[OCMockObject mockForClass:[WKNavigationAction class]] retain]);
  [[[static_cast<OCMockObject*>(navigation_action.get()) stub]
      andReturn:request] request];
  [[mock_delegate_ expect] simpleWebViewController:web_view_controller_
                        shouldStartLoadWithRequest:request];
  [static_cast<id<WKNavigationDelegate>>(web_view_controller_.get())
                              webView:mock_web_view_
      decidePolicyForNavigationAction:navigation_action
                      decisionHandler:^void(WKNavigationActionPolicy){}];
  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that |shouldStartLoadWithRequest:| YES decision by the delegate is
// respected by the CRWSWKimpleWebViewController
TEST_F(CRWWKSimpleWebViewControllerTest, ShouldStartLoadRespectedYes) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  TestShouldStartLoadRespected(YES);
}

// Tests that |shouldStartLoadWithRequest:| NO decision by the delegate is
// respected by the CRWSWKimpleWebViewController
TEST_F(CRWWKSimpleWebViewControllerTest, ShouldStartLoadRespectedNo) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  TestShouldStartLoadRespected(NO);
}

// Tests that |titleMayHaveChanged:| is correctly called on the delegate.
TEST_F(CRWWKSimpleWebViewControllerTest, TitleMayHaveChanged) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  base::scoped_nsobject<WKWebView> web_view(
      web::CreateWKWebView(CGRectZero, GetBrowserState()));
  base::scoped_nsobject<WKWebView> mock_web_view(
      [[OCMockObject partialMockForObject:web_view] retain]);
  web_view_controller_.reset(
      [[CRWWKSimpleWebViewController alloc] initWithWKWebView:mock_web_view]);
  [web_view_controller_ setDelegate:mock_delegate_];
  [[mock_delegate_ expect] simpleWebViewController:web_view_controller_
                               titleMayHaveChanged:@"John Mayer"];

  // Simulate a KVO callback.
  [web_view willChangeValueForKey:@"title"];
  [[[static_cast<OCMockObject*>(mock_web_view.get()) stub]
      andReturn:@"John Mayer"] title];
  [web_view didChangeValueForKey:@"title"];

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests correct JavaScript evaluation.
TEST_F(CRWWKSimpleWebViewControllerTest, JavaScriptEvaluation) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  NSString* const kTestResult = @"result";
  NSError* const test_error = [NSError errorWithDomain:@"" code:0 userInfo:nil];
  id run_completion_handler = ^(NSInvocation* invocation) {
      void(^completionHandler)(id, NSError*);
      [invocation getArgument:&completionHandler atIndex:3];
      completionHandler(kTestResult, test_error);
  };
  NSString* const kTestScript = @"script";
  [[[mock_web_view_ stub] andDo:run_completion_handler]
      evaluateJavaScript:kTestScript completionHandler:[OCMArg isNotNil]];

  __block bool evaluation_completed = false;
  id completion_handler = ^(NSString* result, NSError* error) {
      evaluation_completed = true;
      EXPECT_NSEQ(kTestResult, result);
      EXPECT_NSEQ(test_error, error);
  };
  [web_view_controller_ evaluateJavaScript:kTestScript
                       stringResultHandler:completion_handler];
  base::test::ios::WaitUntilCondition(^bool() {
    return evaluation_completed;
  });

  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

}  // namespace
