// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/test/chrome_web_view_test.h"

#import <ChromeWebView/ChromeWebView.h>
#import <Foundation/Foundation.h>

#include "base/base64.h"
#import "base/memory/ptr_util.h"
#import "ios/testing/wait_util.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Test server path which echos the remainder of the url as html. The html
// must be base64 encoded.
const char kPageHtmlBodyPath[] = "/PageHtmlBody?";

// Generates an html response from a request to |kPageHtmlBodyPath|.
std::unique_ptr<net::test_server::HttpResponse> EchoPageHTMLBodyInResponse(
    const net::test_server::HttpRequest& request) {
  DCHECK(base::StartsWith(request.relative_url, kPageHtmlBodyPath,
                          base::CompareCase::INSENSITIVE_ASCII));

  std::string body = request.relative_url.substr(strlen(kPageHtmlBodyPath));

  std::string unescaped_body;
  base::Base64Decode(body, &unescaped_body);
  std::string html = "<html><body>" + unescaped_body + "</body></html>";

  auto http_response = base::MakeUnique<net::test_server::BasicHttpResponse>();
  http_response->set_content(html);
  return std::move(http_response);
}

// Maps test server requests to responses.
std::unique_ptr<net::test_server::HttpResponse> TestRequestHandler(
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(request.relative_url, kPageHtmlBodyPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return EchoPageHTMLBodyInResponse(request);
  }
  return nullptr;
}

}  // namespace

namespace ios_web_view {

ChromeWebViewTest::ChromeWebViewTest()
    : test_server_(base::MakeUnique<net::EmbeddedTestServer>(
          net::test_server::EmbeddedTestServer::TYPE_HTTP)) {
  net::test_server::RegisterDefaultHandlers(test_server_.get());
  test_server_->RegisterRequestHandler(base::Bind(&TestRequestHandler));
}

ChromeWebViewTest::~ChromeWebViewTest() = default;

void ChromeWebViewTest::SetUp() {
  PlatformTest::SetUp();
  ASSERT_TRUE(test_server_->Start());
}

GURL ChromeWebViewTest::GetUrlForPageWithTitle(const std::string& title) {
  return test_server_->GetURL("/echotitle/" + title);
}

GURL ChromeWebViewTest::GetUrlForPageWithHTMLBody(const std::string& html) {
  std::string base64_html;
  base::Base64Encode(html, &base64_html);
  return test_server_->GetURL(kPageHtmlBodyPath + base64_html);
}

void ChromeWebViewTest::LoadUrl(CWVWebView* web_view, NSURL* url) {
  [web_view loadRequest:[NSURLRequest requestWithURL:url]];

  WaitForPageLoadCompletion(web_view);
}

void ChromeWebViewTest::WaitForPageLoadCompletion(CWVWebView* web_view) {
  BOOL success =
      testing::WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
        return !web_view.isLoading;
      });
  ASSERT_TRUE(success);
}

}  // namespace ios_web_view
