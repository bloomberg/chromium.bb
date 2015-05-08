// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/clients/crw_csp_network_client.h"

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#import "ios/net/clients/crn_forwarding_network_client.h"
#import "ios/web/net/crw_url_verifying_protocol_handler.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

@interface CRWCspMockClient : CRNForwardingNetworkClient
@end

@implementation CRWCspMockClient {
  base::scoped_nsobject<NSURLResponse> _response;
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  _response.reset([response retain]);
}

- (NSURLResponse*)response {
  return _response;
}

@end

class CRWCspNetworkClientTest : public testing::Test {
 public:
  CRWCspNetworkClientTest() {
    mock_client_.reset([[CRWCspMockClient alloc] init]);
    csp_client_.reset([[CRWCspNetworkClient alloc] init]);
    [csp_client_ setUnderlyingClient:mock_client_];
  }

  web::TestWebThreadBundle thread_bundle_;
  base::scoped_nsobject<CRWCspNetworkClient> csp_client_;
  base::scoped_nsobject<CRWCspMockClient> mock_client_;
};

TEST_F(CRWCspNetworkClientTest, FixCspHeaders) {
  base::scoped_nsobject<NSDictionary> input_headers([@{
    @"Foo" : @"Bar",
    @"coNteNt-seCuRity-POLicy" : @"coNNect-sRc foo.com; script-src 'self'",
    @"X-WebKit-CSP" : @"frame-src 'self'"
  } retain]);

  base::scoped_nsobject<NSHTTPURLResponse> input_response(
      [[NSHTTPURLResponse alloc] initWithURL:[NSURL URLWithString:@"http://foo"]
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:input_headers]);

  [csp_client_ didReceiveResponse:input_response];
  ASSERT_TRUE(
      [[mock_client_ response] isKindOfClass:[NSHTTPURLResponse class]]);
  base::scoped_nsobject<NSDictionary> output_headers(
      [[static_cast<NSHTTPURLResponse*>([mock_client_ response])
          allHeaderFields] retain]);

  // Check that unrelated headers are copied.
  EXPECT_NSEQ(@"Bar", [output_headers objectForKey:@"Foo"]);

  base::scoped_nsobject<NSString> csp_header(
      [[output_headers objectForKey:@"coNteNt-seCuRity-POLicy"] retain]);

  EXPECT_TRUE(csp_header.get());

  // frame-src is not created because there were no frame-src and no
  // default-src.
  // 'self' and |kURLForVerification| are prepended to the connect-src value.
  NSString* expected_csp_header = [NSString stringWithFormat:
      @"coNNect-sRc 'self' %s foo.com; " @"script-src 'self'",
      web::kURLForVerification];
  EXPECT_NSEQ(expected_csp_header, csp_header);

  // X-WebKit-CSP is handled as well.
  // crwebinvoke: crwebinvokeimmediate: and crwebnull: are prepended to the
  // existing frame-src value.
  // connect-src is not created because there were no connect-src and no
  // default-src.
  csp_header.reset([[output_headers objectForKey:@"X-WebKit-CSP"] retain]);
  EXPECT_NSEQ(@"frame-src crwebinvoke: crwebinvokeimmediate: crwebnull: 'self'",
              csp_header);
}

TEST_F(CRWCspNetworkClientTest, FixCspHeadersWithDefault) {
  base::scoped_nsobject<NSDictionary> input_headers([@{
    @"Content-Security-Policy" : @"default-src foo.com; connect-src *"
  } retain]);

  base::scoped_nsobject<NSHTTPURLResponse> input_response(
      [[NSHTTPURLResponse alloc] initWithURL:[NSURL URLWithString:@"http://foo"]
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:input_headers]);

  [csp_client_ didReceiveResponse:input_response];
  ASSERT_TRUE(
      [[mock_client_ response] isKindOfClass:[NSHTTPURLResponse class]]);
  base::scoped_nsobject<NSDictionary> output_headers(
      [[static_cast<NSHTTPURLResponse*>([mock_client_ response])
          allHeaderFields] retain]);

  base::scoped_nsobject<NSString> csp_header(
      [[output_headers objectForKey:@"Content-Security-Policy"] retain]);

  EXPECT_TRUE(csp_header.get());

  // crwebinvoke: crwebinvokeimmediate: and crwebnull: are prepended to the
  // existing default-src value because there was no frame-src.
  // 'self' and |kURLForVerification| are prepended to the connect-src value.
  NSString* expected_csp_header = [NSString stringWithFormat:
      @"default-src crwebinvoke: crwebinvokeimmediate: crwebnull: foo.com; "
      @"connect-src 'self' %s *", web::kURLForVerification];
  EXPECT_NSEQ(expected_csp_header, csp_header);
}

TEST_F(CRWCspNetworkClientTest, NonHTTPResponse) {
  base::scoped_nsobject<NSURLResponse> response([[NSURLResponse alloc] init]);
  [csp_client_ didReceiveResponse:response];
  // The client is a pass-through, compare the pointers.
  EXPECT_EQ(response, [mock_client_ response]);
}
