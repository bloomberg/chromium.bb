// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_ui_simple_web_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/mac/scoped_nsobject.h"
#import "base/test/ios/wait_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

class CRWUISimpleWebViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    mock_web_view_.reset(
        [[OCMockObject niceMockForClass:[UIWebView class]] retain]);
    simple_web_view_controller_.reset([[CRWUISimpleWebViewController alloc]
        initWithUIWebView:mock_web_view_]);
  }
  base::scoped_nsobject<id> mock_web_view_;
  base::scoped_nsobject<CRWUISimpleWebViewController>
      simple_web_view_controller_;
};

// Tests to make sure a CRWUISimpleWebViewController correctly sets the backing
// UIWebView and its delegate.
TEST_F(CRWUISimpleWebViewControllerTest, Basic) {
  EXPECT_EQ(mock_web_view_.get(), [simple_web_view_controller_ view]);
}

// Tests that CRWUISimpleWebViewController can correctly retrieve the title from
// the underlying UIWebView.
// TODO(shreyasv): Revisit this test if mock HTTP server works for unit tests.
TEST_F(CRWUISimpleWebViewControllerTest, Title) {
  [[[mock_web_view_ stub] andReturn:@"Steven Wilson"]
      stringByEvaluatingJavaScriptFromString:@"document.title"];
  EXPECT_NSEQ(@"Steven Wilson", [simple_web_view_controller_ title]);
}

// Tests that CRWUISimpleWebViewController correctly reloads from the underlying
// UIWebView.
TEST_F(CRWUISimpleWebViewControllerTest, Reload) {
  [[mock_web_view_ expect] reload];
  [simple_web_view_controller_ reload];
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that CRWUISimpleWebViewController correctly returns the WebView's
// scroll view.
TEST_F(CRWUISimpleWebViewControllerTest, ScrollView) {
  base::scoped_nsobject<UIScrollView> scrollView([[UIScrollView alloc] init]);
  [[[mock_web_view_ stub] andReturn:scrollView] scrollView];
  EXPECT_EQ(scrollView, [simple_web_view_controller_ scrollView]);
}

// Tests that CRWUISimpleWebViewController correctly loads a request from the
// underlying UIWebView.
TEST_F(CRWUISimpleWebViewControllerTest, LoadRequest) {
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://google.com"]];
  [[mock_web_view_ expect] loadRequest:request];
  [simple_web_view_controller_ loadRequest:request];
  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests that CRWUISimpleWebViewControllerDelegate is correctly informed of a
// load request.
TEST_F(CRWUISimpleWebViewControllerTest, ShouldStartLoad) {
  base::scoped_nsobject<id> mockDelegate([[OCMockObject
      niceMockForProtocol:@protocol(CRWSimpleWebViewControllerDelegate)]
          retain]);
  NSURLRequest* request =
      [NSURLRequest requestWithURL:[NSURL URLWithString:@"http://google.com"]];
  [[mockDelegate expect] simpleWebViewController:simple_web_view_controller_
                      shouldStartLoadWithRequest:request];

  // Simulate a UIWebViewDelegate callback.
  [static_cast<id<UIWebViewDelegate>>(simple_web_view_controller_.get())
      webView:mock_web_view_
      shouldStartLoadWithRequest:request
                  navigationType:UIWebViewNavigationTypeOther];

  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}

// Tests correct JavaScript evaluation.
TEST_F(CRWUISimpleWebViewControllerTest, JavaScriptEvaluation) {
  NSString* const kTestScript = @"script";
  NSString* const kTestResult = @"result";
  [[[mock_web_view_ stub] andReturn:kTestResult]
      stringByEvaluatingJavaScriptFromString:kTestScript];

  __block bool evaluation_completed = false;
  id completion_handler = ^(NSString* result, NSError* error) {
      evaluation_completed = true;
      EXPECT_NSEQ(kTestResult, result);
      EXPECT_EQ(nil, error);
  };
  [simple_web_view_controller_ evaluateJavaScript:kTestScript
                              stringResultHandler:completion_handler];
  base::test::ios::WaitUntilCondition(^bool() {
    return evaluation_completed;
  });

  EXPECT_OCMOCK_VERIFY(mock_web_view_);
}
