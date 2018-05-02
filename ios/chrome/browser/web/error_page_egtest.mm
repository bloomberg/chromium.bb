// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <string>

#import "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns ERR_INTERNET_DISCONNECTED error message.
NSString* GetErrorMessage() {
  return base::SysUTF8ToNSString(
      net::ErrorToShortString(net::ERR_INTERNET_DISCONNECTED));
}
}  // namespace

// Tests critical user journeys reloated to page load errors.
@interface ErrorPageTestCase : ChromeTestCase
// YES if test server is replying with valid HTML content (URL query). NO if
// test server closes the socket.
@property(atomic) BOOL serverRespondsWithContent;
@end

@implementation ErrorPageTestCase
@synthesize serverRespondsWithContent = _serverRespondsWithContent;

- (void)setUp {
  [super setUp];

  // Tests handler which replies with URL query for /echo-query path if
  // serverRespondsWithContent set to YES. Otherwise the handler closes the
  // socket.
  using net::test_server::HttpRequest;
  using net::test_server::HttpResponse;
  auto handler = ^std::unique_ptr<HttpResponse>(const HttpRequest& request) {
    if (!self.serverRespondsWithContent) {
      return std::make_unique<net::test_server::RawHttpResponse>(
          /*headers=*/"", /*contents=*/"");
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("text/html");
    response->set_content(request.GetURL().query());
    return std::move(response);
  };
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest,
                          "/echo-query", base::BindBlockArc(handler)));
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, "/iframe",
                          base::BindRepeating(&testing::HandleIFrame)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Loads the URL which fails to load, then sucessfully reloads the page.
- (void)testReload {
  // No response leads to ERR_INTERNET_DISCONNECTED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForStaticHTMLViewContainingText:GetErrorMessage()];

  // Reload the page, which should load without errors.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebViewContainingText:"foo"];
}

// Loads the URL which redirects to unresponsive server.
- (void)testRedirectToFailingURL {
  // No response leads to ERR_INTERNET_DISCONNECTED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey
      loadURL:self.testServer->GetURL("/server-redirect?echo-query")];
  [ChromeEarlGrey waitForStaticHTMLViewContainingText:GetErrorMessage()];
}

// Loads the page with iframe, and that iframe fails to load. There should be no
// error page if the main frame has sucessfully loaded.
- (void)testErrorPageInIFrame {
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/iframe?echo-query")];
  [ChromeEarlGrey
      waitForWebViewContainingCSSSelector:"iframe[src*='echo-query']"];
}

@end
