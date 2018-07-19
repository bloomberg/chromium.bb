// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "ios/web_view/test/web_view_inttest_base.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Tests CWVNavigationDelegate.
class NavigationDelegateTest : public ios_web_view::WebViewInttestBase {
 public:
  void SetUp() override {
    ASSERT_TRUE(test_server_->Start());
    url_with_content_ =
        net::NSURLWithGURL(GetUrlForPageWithTitleAndBody("Title", "Body"));
    url_with_error_ = net::NSURLWithGURL(test_server_->GetURL("/close-socket"));

    mock_delegate_ = OCMStrictProtocolMock(@protocol(CWVNavigationDelegate));
    [(id)mock_delegate_ setExpectationOrderMatters:YES];
    web_view_.navigationDelegate = mock_delegate_;
  }

  id ArgWithURL(NSURL* url) {
    return [OCMArg checkWithBlock:^(id object) {
      return [[object URL] isEqual:url];
    }];
  }

  NSURL* url_with_content_;
  NSURL* url_with_error_;
  id<CWVNavigationDelegate> mock_delegate_;
};

// Tests that expected delegate methods are called for a successful request.
TEST_F(NavigationDelegateTest, RequestSucceeds) {
  // A request made with -loadRequest: has type CWVNavigationTypeClientRedirect.
  OCMExpect([mock_delegate_ webView:web_view_
                shouldStartLoadWithRequest:ArgWithURL(url_with_content_)
                            navigationType:CWVNavigationTypeClientRedirect])
      .andReturn(YES);
  OCMExpect([mock_delegate_ webViewDidStartProvisionalNavigation:web_view_]);
  OCMExpect([mock_delegate_ webView:web_view_
                shouldContinueLoadWithResponse:ArgWithURL(url_with_content_)
                                  forMainFrame:YES])
      .andReturn(YES);
  OCMExpect([mock_delegate_ webViewDidCommitNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidFinishNavigation:web_view_]);

  ASSERT_TRUE(test::LoadUrl(web_view_, url_with_content_));
  [(id)mock_delegate_ verify];
}

// Tests that expected delegate methods are called for a failed request.
TEST_F(NavigationDelegateTest, RequestFails) {
  OCMExpect([mock_delegate_ webView:web_view_
                shouldStartLoadWithRequest:ArgWithURL(url_with_error_)
                            navigationType:CWVNavigationTypeClientRedirect])
      .andReturn(YES);
  OCMExpect([mock_delegate_ webViewDidStartProvisionalNavigation:web_view_]);
  OCMExpect([mock_delegate_ webViewDidCommitNavigation:web_view_]);
  OCMExpect([mock_delegate_ webView:web_view_
         didFailNavigationWithError:[OCMArg any]]);
  // -webViewDidCommitNavigation: is called one more time for failures.
  OCMExpect([mock_delegate_ webViewDidCommitNavigation:web_view_]);

  ASSERT_TRUE(test::LoadUrl(web_view_, url_with_error_));
  [(id)mock_delegate_ verify];
}

// Tests that a request is canceled and no further delegate methods are called
// when -shouldStartLoadWithRequest:navigationType: returns NO.
TEST_F(NavigationDelegateTest, CancelRequest) {
  OCMExpect([mock_delegate_ webView:web_view_
                shouldStartLoadWithRequest:ArgWithURL(url_with_content_)
                            navigationType:CWVNavigationTypeClientRedirect])
      .andReturn(NO);

  ASSERT_TRUE(test::LoadUrl(web_view_, url_with_content_));
  [(id)mock_delegate_ verify];
}

// Tests that a response is canceled and no further delegate methods are called
// when -shouldContinueLoadWithResponse:forMainFrame: returns NO.
TEST_F(NavigationDelegateTest, CancelResponse) {
  OCMExpect([mock_delegate_ webView:web_view_
                shouldStartLoadWithRequest:ArgWithURL(url_with_content_)
                            navigationType:CWVNavigationTypeClientRedirect])
      .andReturn(YES);
  OCMExpect([mock_delegate_ webViewDidStartProvisionalNavigation:web_view_]);
  OCMExpect([mock_delegate_ webView:web_view_
                shouldContinueLoadWithResponse:ArgWithURL(url_with_content_)
                                  forMainFrame:YES])
      .andReturn(NO);

  ASSERT_TRUE(test::LoadUrl(web_view_, url_with_content_));
  [(id)mock_delegate_ verify];
}

}  // namespace ios_web_view
