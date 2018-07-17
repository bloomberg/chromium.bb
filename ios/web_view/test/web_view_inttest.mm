// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#import "ios/web_view/test/web_view_int_test.h"
#import "ios/web_view/test/web_view_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Tests public methods in CWVWebView.
class WebViewTest : public ios_web_view::WebViewIntTest {
 public:
  std::unique_ptr<net::test_server::HttpResponse> TestRequestHandler(
      const net::test_server::HttpRequest& request) {
    last_request_ = request;
    return nullptr;
  }

  net::test_server::HttpRequest last_request_;
};

// Tests +[CWVWebView setUserAgentProduct] and +[CWVWebView userAgentProduct].
TEST_F(WebViewTest, UserAgentProduct) {
  // Registers a custom handler to capture HTTP headers.
  // /echoheader?User-Agent provided by EmbeddedTestServer cannot be used here
  // because it returns content with type text/plain, but we cannot extract the
  // content using test::WaitForWebViewContainingTextOrTimeout() because
  // JavaScript cannot be executed on text/plain content.
  test_server_->RegisterRequestHandler(base::BindRepeating(
      &WebViewTest::TestRequestHandler, base::Unretained(this)));
  ASSERT_TRUE(test_server_->Start());

  [CWVWebView setUserAgentProduct:@"MyUserAgentProduct"];
  ASSERT_NSEQ(@"MyUserAgentProduct", [CWVWebView userAgentProduct]);

  // Cannot use existing |web_view_| here because the change above may only
  // affect web views created after the change.
  CWVWebView* web_view = test::CreateWebView();
  GURL url = test_server_->GetURL("/");
  ASSERT_TRUE(test::LoadUrl(web_view, net::NSURLWithGURL(url)));

  // Tests that the web view has sent User-Agent HTTP header with the specified
  // product name.
  auto user_agent_it = last_request_.headers.find("User-Agent");
  ASSERT_NE(last_request_.headers.end(), user_agent_it);
  EXPECT_NE(std::string::npos,
            user_agent_it->second.find("MyUserAgentProduct"));
}

// TODO(crbug.com/862537): Write more tests.

}  // namespace ios_web_view
