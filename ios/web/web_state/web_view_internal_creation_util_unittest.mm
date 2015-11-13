// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#import <CoreGraphics/CoreGraphics.h>
#import <WebKit/WebKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/public/test/test_browser_state.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/ui/crw_debug_web_view.h"
#import "ios/web/web_state/ui/crw_simple_web_view_controller.h"
#import "ios/web/web_state/ui/crw_static_file_web_view.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/test/web_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest_mac.h"

@interface CRWStaticFileWebView (Testing)
+ (BOOL)isStaticFileUserAgent:(NSString*)userAgent;
@end

namespace web {
namespace {

const CGRect kTestFrame = CGRectMake(5.0f, 10.0f, 15.0f, 20.0f);
NSString* const kTestRequestGroupID = @"100042";

// Returns user agent for given UIWebView.
NSString* GetWebViewUserAgent(UIWebView* web_view) {
  NSString* const kGetUserAgentJS = @"navigator.userAgent";
  return [web_view stringByEvaluatingJavaScriptFromString:kGetUserAgentJS];
}

// A WebClient that stubs PreWebViewCreation/PostWebViewCreation calls for
// testing purposes.
class CreationUtilsWebClient : public TestWebClient {
 public:
  MOCK_CONST_METHOD0(PreWebViewCreation, void());
  MOCK_CONST_METHOD1(PostWebViewCreation, void(UIWebView* web_view));
};

class WebViewCreationUtilsTest : public WebTest {
 public:
  WebViewCreationUtilsTest() {}

 protected:
  void SetUp() override {
    WebTest::SetUp();
    logJavaScriptPref_ =
        [[NSUserDefaults standardUserDefaults] boolForKey:@"LogJavascript"];
    original_web_client_ = GetWebClient();
    SetWebClient(&creation_utils_web_client_);
    creation_utils_web_client_.SetUserAgent("TestUA", false);
  }
  void TearDown() override {
    SetWebClient(original_web_client_);
    [[NSUserDefaults standardUserDefaults] setBool:logJavaScriptPref_
                                            forKey:@"LogJavascript"];
    WebTest::TearDown();
  }
  // Sets up expectation for WebClient::PreWebViewCreation and
  // WebClient::PostWebViewCreation calls. Captures UIWebView passed to
  // PostWebViewCreation into captured_web_view param.
  void ExpectWebClientCalls(UIWebView** captured_web_view) const {
    EXPECT_CALL(creation_utils_web_client_, PreWebViewCreation()).Times(1);
    EXPECT_CALL(creation_utils_web_client_, PostWebViewCreation(testing::_))
        .Times(1)
        .WillOnce(testing::SaveArg<0>(captured_web_view));
  }

 private:
  // Original value of @"LogJavascript" pref from NSUserDefaults.
  BOOL logJavaScriptPref_;
  // WebClient that stubs PreWebViewCreation/PostWebViewCreation.
  CreationUtilsWebClient creation_utils_web_client_;
  // The WebClient that was set before this test was run.
  WebClient* original_web_client_;
};

// Tests that a web view created with a certain id returns the same
// requestGroupID in the user agent string.
TEST_F(WebViewCreationUtilsTest, CreationWithRequestGroupID) {
  UIWebView* captured_web_view = nil;
  ExpectWebClientCalls(&captured_web_view);

  base::scoped_nsobject<UIWebView> web_view(
      CreateWebView(kTestFrame, kTestRequestGroupID, NO));
  EXPECT_TRUE([web_view isMemberOfClass:[UIWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(kTestFrame, [web_view frame]));
  EXPECT_NSEQ(web_view, captured_web_view);

  NSString* const kExpectedUserAgent =
      [NSString stringWithFormat:@"TestUA (%@)", kTestRequestGroupID];
  NSString* const kActualUserAgent = GetWebViewUserAgent(web_view);
  EXPECT_NSEQ(kExpectedUserAgent, kActualUserAgent);
  EXPECT_TRUE([ExtractRequestGroupIDFromUserAgent(kActualUserAgent)
      isEqualToString:kTestRequestGroupID]);
}

// Tests that a web view not created for displaying static file content from
// the application bundle does not return a user agent that allows static file
// content display.
TEST_F(WebViewCreationUtilsTest, CRWStaticFileWebViewFalse) {
  base::scoped_nsobject<UIWebView> web_view(
      CreateWebView(CGRectZero, kTestRequestGroupID, NO));
  EXPECT_FALSE([CRWStaticFileWebView
      isStaticFileUserAgent:GetWebViewUserAgent(web_view)]);
}

// Tests web::CreateWebView function that it correctly returns a UIWebView with
// the correct frame and calls WebClient::PreWebViewCreation/
// WebClient::PostWebViewCreation methods.
TEST_F(WebViewCreationUtilsTest, Creation) {
  [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"LogJavascript"];

  UIWebView* captured_web_view = nil;
  ExpectWebClientCalls(&captured_web_view);

  base::scoped_nsobject<UIWebView> web_view(CreateWebView(kTestFrame));
  EXPECT_TRUE([web_view isMemberOfClass:[UIWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(kTestFrame, [web_view frame]));
  EXPECT_NSEQ(web_view, captured_web_view);
}

// Tests web::CreateWKWebView function that it correctly returns a WKWebView
// with the correct frame and WKProcessPool.
TEST_F(WebViewCreationUtilsTest, WKWebViewCreationWithBrowserState) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(kTestFrame, GetBrowserState()));

  EXPECT_TRUE([web_view isKindOfClass:[WKWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(kTestFrame, [web_view frame]));

  // Make sure that web view's configuration shares the same process pool with
  // browser state's configuration. Otherwise cookie will not be immediately
  // shared between different web views.
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  EXPECT_EQ(config_provider.GetWebViewConfiguration().processPool,
            [[web_view configuration] processPool]);
}

// Tests that web::CreateWKWebView always returns a web view with the same
// processPool.
TEST_F(WebViewCreationUtilsTest, WKWebViewsShareProcessPool) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();

  base::scoped_nsobject<WKWebView> web_view(
      CreateWKWebView(kTestFrame, GetBrowserState()));
  ASSERT_TRUE(web_view);
  base::scoped_nsobject<WKWebView> web_view2(
      CreateWKWebView(kTestFrame, GetBrowserState()));
  ASSERT_TRUE(web_view2);

  // Make sure that web views share the same non-nil process pool. Otherwise
  // cookie will not be immediately shared between different web views.
  EXPECT_TRUE([[web_view configuration] processPool]);
  EXPECT_EQ([[web_view configuration] processPool],
            [[web_view2 configuration] processPool]);
}

#if !defined(NDEBUG)

// Tests web::CreateWebView function that it correctly returns a CRWDebugWebView
// with the correct frame and calls WebClient::PreWebViewCreation/
// WebClient::PostWebViewCreation methods.
TEST_F(WebViewCreationUtilsTest, DebugCreation) {
  [[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"LogJavascript"];

  UIWebView* captured_web_view = nil;
  ExpectWebClientCalls(&captured_web_view);

  base::scoped_nsobject<UIWebView> web_view(CreateWebView(kTestFrame));
  EXPECT_TRUE([web_view isMemberOfClass:[CRWDebugWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(kTestFrame, [web_view frame]));
  EXPECT_NSEQ(web_view, captured_web_view);
}

// Tests that getting a WKWebView from the util methods correctly maintains
// the global active wkwebview count (which is debug-only).
TEST_F(WebViewCreationUtilsTest, GetActiveWKWebViewsCount) {
  CR_TEST_REQUIRES_WK_WEB_VIEW();
  base::scoped_nsobject<WKWebView> web_view1(
      CreateWKWebView(CGRectZero, GetBrowserState()));
  EXPECT_EQ(1U, GetActiveWKWebViewsCount());
  base::scoped_nsobject<WKWebView> web_view2(
      CreateWKWebView(CGRectZero, GetBrowserState()));
  EXPECT_EQ(2U, GetActiveWKWebViewsCount());
  web_view2.reset();
  EXPECT_EQ(1U, GetActiveWKWebViewsCount());
  web_view1.reset();
  EXPECT_EQ(0U, GetActiveWKWebViewsCount());
}

#endif  // defined(NDEBUG)

// Tests web::CreateStaticFileWebView that it correctly returns a
// CRWStaticFileWebView with the correct frame, user agent, and calls
// WebClient::PreWebViewCreation/WebClient::PostWebViewCreation methods.
TEST_F(WebViewCreationUtilsTest, TestNewStaticFileWebViewTrue) {
  UIWebView* captured_web_view = nil;
  ExpectWebClientCalls(&captured_web_view);

  base::scoped_nsobject<UIWebView> web_view(
      CreateStaticFileWebView(kTestFrame, GetBrowserState()));
  ASSERT_TRUE([web_view isMemberOfClass:[CRWStaticFileWebView class]]);
  EXPECT_TRUE(CGRectEqualToRect(kTestFrame, [web_view frame]));
  EXPECT_NSEQ(web_view, captured_web_view);

  NSString* user_agent = GetWebViewUserAgent(web_view);
  EXPECT_TRUE([CRWStaticFileWebView isStaticFileUserAgent:user_agent]);
}

// Tests web::CreateSimpleWebViewController returns a CRWSimpleWebViewController
// instance with a web view.
TEST_F(WebViewCreationUtilsTest, CreateSimpleWebViewController) {
  base::scoped_nsprotocol<id<CRWSimpleWebViewController>>
      simpleWebViewController(
          CreateSimpleWebViewController(CGRectZero, nullptr, UI_WEB_VIEW_TYPE));
  EXPECT_TRUE([simpleWebViewController view]);
}

}  // namespace
}  // namespace web
