// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/web_http_protocol_handler_delegate.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "ios/web/public/web_client.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web {

namespace {

// Test application specific scheme.
const char kAppSpecificScheme[] = "appspecific";

// URLs expected to be supported.
const char* kSupportedURLs[] = {
    "http://foo.com",
    "https://foo.com",
    "data:text/html;charset=utf-8,Hello",
};

// URLs expected to be unsupported.
const char* kUnsupportedURLs[] = {
    "foo:blank",          // Unknown scheme.
    "appspecific:blank",  // No main document URL.
};

// Test web client with an application specific scheme.
class AppSpecificURLTestWebClient : public WebClient {
 public:
  bool IsAppSpecificURL(const GURL& url) const override {
    return url.SchemeIs(kAppSpecificScheme);
  }
};

class WebHTTPProtocolHandlerDelegateTest : public testing::Test {
 public:
  WebHTTPProtocolHandlerDelegateTest()
      : context_getter_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        delegate_(new WebHTTPProtocolHandlerDelegate(context_getter_.get())),
        original_web_client_(GetWebClient()),
        web_client_(new AppSpecificURLTestWebClient) {
    SetWebClient(web_client_.get());
  }

  ~WebHTTPProtocolHandlerDelegateTest() override {
    SetWebClient(original_web_client_);
  }

 protected:
  base::MessageLoop message_loop_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  scoped_ptr<WebHTTPProtocolHandlerDelegate> delegate_;
  WebClient* original_web_client_;  // Stash the web client, Weak.
  scoped_ptr<AppSpecificURLTestWebClient> web_client_;
};

}  // namespace

TEST_F(WebHTTPProtocolHandlerDelegateTest, IsRequestSupported) {
  base::scoped_nsobject<NSMutableURLRequest> request;

  for (unsigned int i = 0; i < arraysize(kSupportedURLs); ++i) {
    base::scoped_nsobject<NSString> url_string(
        [[NSString alloc] initWithUTF8String:kSupportedURLs[i]]);
    request.reset([[NSMutableURLRequest alloc]
        initWithURL:[NSURL URLWithString:url_string]]);
    EXPECT_TRUE(delegate_->IsRequestSupported(request))
        << kSupportedURLs[i] << " should be supported.";
  }

  for (unsigned int i = 0; i < arraysize(kUnsupportedURLs); ++i) {
    base::scoped_nsobject<NSString> url_string(
        [[NSString alloc] initWithUTF8String:kUnsupportedURLs[i]]);
    request.reset([[NSMutableURLRequest alloc]
        initWithURL:[NSURL URLWithString:url_string]]);
    EXPECT_FALSE(delegate_->IsRequestSupported(request))
        << kUnsupportedURLs[i] << " should NOT be supported.";
  }

  // Application specific scheme with main document URL.
  request.reset([[NSMutableURLRequest alloc]
      initWithURL:[NSURL URLWithString:@"appspecific:blank"]]);
  [request setMainDocumentURL:[NSURL URLWithString:@"http://foo"]];
  EXPECT_FALSE(delegate_->IsRequestSupported(request));
  [request setMainDocumentURL:[NSURL URLWithString:@"appspecific:main"]];
  EXPECT_TRUE(delegate_->IsRequestSupported(request));
  request.reset([[NSMutableURLRequest alloc]
      initWithURL:[NSURL URLWithString:@"foo:blank"]]);
  [request setMainDocumentURL:[NSURL URLWithString:@"appspecific:main"]];
  EXPECT_FALSE(delegate_->IsRequestSupported(request));
}

TEST_F(WebHTTPProtocolHandlerDelegateTest, IsRequestSupportedMalformed) {
  base::scoped_nsobject<NSURLRequest> request;

  // Null URL.
  request.reset([[NSMutableURLRequest alloc] init]);
  ASSERT_FALSE([request URL]);
  EXPECT_FALSE(delegate_->IsRequestSupported(request));

  // URL with no scheme.
  request.reset(
      [[NSMutableURLRequest alloc] initWithURL:[NSURL URLWithString:@"foo"]]);
  ASSERT_TRUE([request URL]);
  ASSERT_FALSE([[request URL] scheme]);
  EXPECT_FALSE(delegate_->IsRequestSupported(request));

  // Empty scheme.
  request.reset(
      [[NSMutableURLRequest alloc] initWithURL:[NSURL URLWithString:@":foo"]]);
  ASSERT_TRUE([request URL]);
  ASSERT_TRUE([[request URL] scheme]);
  ASSERT_FALSE([[[request URL] scheme] length]);
  EXPECT_FALSE(delegate_->IsRequestSupported(request));
}

}  // namespace web
