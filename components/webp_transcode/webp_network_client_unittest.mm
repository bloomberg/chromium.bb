// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webp_transcode/webp_network_client.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
class WebPNetworkClientTest : public testing::Test {
 public:
  WebPNetworkClientTest() {
    // Set up mock original network client proxy.
    OCMockObject* mockProxy_ = [[OCMockObject
        niceMockForProtocol:@protocol(CRNNetworkClientProtocol)] retain];
    mockWebProxy_.reset(mockProxy_);

    // Link all the mock objects into the WebPNetworkClient.
    webp_client_.reset([[WebPNetworkClient alloc]
        initWithTaskRunner:base::ThreadTaskRunnerHandle::Get()]);
    [webp_client_
        setUnderlyingClient:(id<CRNNetworkClientProtocol>)mockWebProxy_];
  }

 protected:
  base::MessageLoop loop_;
  base::scoped_nsobject<WebPNetworkClient> webp_client_;
  // Holds a mock CRNNetworkClientProtocol object.
  base::scoped_nsobject<OCMockObject> mockWebProxy_;
};
}  // namespace

TEST_F(WebPNetworkClientTest, TestAcceptHeaders) {
  const struct {
    const std::string header_in;
    const std::string header_out;
  } tests[] = {
      {"", "image/webp"},
      {"*/*", "*/*,image/webp"},
      {"image/webp", "image/webp"},
      {"text/html,*/*", "text/html,*/*,image/webp"},
      // Desktop Chrome default without image/webp.
      {"text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
       "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8,"
       "image/webp"},
      // Desktop Chrome default.
      {"text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
       "*/*;q=0.8",
       "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,"
       "*/*;q=0.8"}};
  GURL url("http://www.google.com");
  scoped_ptr<net::URLRequestContext> request_context(
      new net::TestURLRequestContext(false));
  for (size_t i = 0; i < arraysize(tests); ++i) {
    scoped_ptr<net::URLRequest> request =
        request_context->CreateRequest(url, net::DEFAULT_PRIORITY, nullptr);
    if (!tests[i].header_in.empty())
      request->SetExtraRequestHeaderByName("Accept", tests[i].header_in, true);
    [webp_client_ didCreateNativeRequest:request.get()];
    const net::HttpRequestHeaders& headers = request->extra_request_headers();
    std::string acceptHeader;
    EXPECT_TRUE(headers.GetHeader("Accept", &acceptHeader));
    EXPECT_EQ(tests[i].header_out, acceptHeader);
  }
}
