// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_static_file_web_view.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/ui_web_view_util.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface CRWStaticFileWebView (Testing)
+ (BOOL)isStaticFileUserAgent:(NSString*)userAgent;
@end

class CRWStaticFileWebViewTest : public PlatformTest {
 public:
  CRWStaticFileWebViewTest() {}

 protected:
  void SetUp() override { PlatformTest::SetUp(); }
  void TearDown() override { PlatformTest::TearDown(); }

  // Gets the user agent of |web_view|, using javascript.
  NSString* GetWebViewUserAgent(UIWebView* web_view) {
    NSString* const kJsUserAgent = @"navigator.userAgent";
    return [web_view stringByEvaluatingJavaScriptFromString:kJsUserAgent];
  };

  web::TestWebThreadBundle thread_bundle_;
  web::TestBrowserState browser_state_;
};

// Tests that requests for images are considered as static file requests,
// regardless of the user agent.
TEST_F(CRWStaticFileWebViewTest, TestIsStaticImageRequestTrue) {
  // Empty dictionary so User-Agent check fails.
  NSDictionary* dictionary = @{};
  NSURL* url = [NSURL URLWithString:@"file:///show/this.png"];
  id mockRequest = [OCMockObject mockForClass:[NSURLRequest class]];
  [[[mockRequest stub] andReturn:dictionary] allHTTPHeaderFields];
  [[[mockRequest stub] andReturn:url] URL];
  EXPECT_TRUE([CRWStaticFileWebView isStaticFileRequest:mockRequest]);
}

// Tests that requests for files are considered as static file requests if they
// have the static file user agent.
TEST_F(CRWStaticFileWebViewTest, TestIsStaticFileRequestTrue) {
  base::scoped_nsobject<UIWebView> webView(
      [[CRWStaticFileWebView alloc] initWithFrame:CGRectZero
                                     browserState:&browser_state_]);
  EXPECT_TRUE(webView);
  NSString* userAgent = GetWebViewUserAgent(webView);
  NSDictionary* dictionary = @{ @"User-Agent" : userAgent };
  NSURL* url = [NSURL URLWithString:@"file:///some/random/url.html"];
  id mockRequest = [OCMockObject mockForClass:[NSURLRequest class]];
  [[[mockRequest stub] andReturn:dictionary] allHTTPHeaderFields];
  [[[mockRequest stub] andReturn:url] URL];
  EXPECT_TRUE([CRWStaticFileWebView isStaticFileRequest:mockRequest]);
}


// Tests that arbitrary files cannot be retrieved by a web view for
// static file content.
TEST_F(CRWStaticFileWebViewTest, TestIsStaticFileRequestFalse) {
  // Empty dictionary so User-Agent check fails.
  NSDictionary* dictionary = @{};
  NSURL* url = [NSURL URLWithString:@"file:///steal/this/file.html"];
  id mockRequest = [OCMockObject mockForClass:[NSURLRequest class]];
  [[[mockRequest stub] andReturn:dictionary] allHTTPHeaderFields];
  [[[mockRequest stub] andReturn:url] URL];
  EXPECT_FALSE([CRWStaticFileWebView isStaticFileRequest:mockRequest]);
}

// Tests that the user agent of a CRWStaticFileWebView includes a request group
// ID.
TEST_F(CRWStaticFileWebViewTest, TestExtractRequestGroupIDStaticFile) {
  base::scoped_nsobject<UIWebView> webView(
      [[CRWStaticFileWebView alloc] initWithFrame:CGRectZero
                                     browserState:&browser_state_]);
  EXPECT_TRUE(webView);
  NSString* userAgentString = GetWebViewUserAgent(webView);
  EXPECT_TRUE(web::ExtractRequestGroupIDFromUserAgent(userAgentString));
}
