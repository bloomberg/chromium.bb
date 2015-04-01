// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/clients/crw_js_injection_network_client.h"

#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/net/clients/crn_network_client_protocol.h"
#import "ios/net/crn_http_url_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Returns an NSString filled with the char 'a' of length |length|.
NSString* GetLongString(NSUInteger length) {
   base::scoped_nsobject<NSMutableData> data(
       [[NSMutableData alloc] initWithLength:length]);
   memset([data mutableBytes], 'a', length);
   NSString* long_string =
       [[NSString alloc] initWithData:data
                             encoding:NSASCIIStringEncoding];
   return [long_string autorelease];
}

}

// Class to serve as underlying client for JS injection client to expose
// data and responses that are passed on from the JS injection client.
@interface UnderlyingClient : CRNForwardingNetworkClient {
  base::scoped_nsobject<NSMutableData> _loadedData;
  base::scoped_nsobject<NSURLResponse> _receivedResponse;
}
// Returns all data loaded by the client.
- (NSData*)loadedData;
// Returns response received by the client.
- (NSURLResponse*)receivedResponse;
@end

@implementation UnderlyingClient

- (instancetype)init {
  if ((self = [super init])) {
    _loadedData.reset([[NSMutableData alloc] init]);
  }
  return self;
}

- (NSData*)loadedData {
  return _loadedData.get();
}

- (NSURLResponse*)receivedResponse {
  return _receivedResponse.get();
}

- (void)didLoadData:(NSData*)data {
  [_loadedData appendData:data];
  [super didLoadData:data];
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  _receivedResponse.reset([response copy]);
  [super didReceiveResponse:response];
}

@end

namespace {

const char kTestFile[] = "ios/web/test/data/chrome.html";

class CRWJSInjectionNetworkClientTest : public testing::Test {
 public:
  CRWJSInjectionNetworkClientTest() {}

  void SetUp() override {
    // Set up mock original network client proxy.
    mock_web_proxy_.reset([[OCMockObject
        niceMockForProtocol:@protocol(CRNNetworkClientProtocol)] retain]);

    // Set up underlying client to inspect data and responses passed on by
    // the JS injection client.
    underlying_client_.reset([[UnderlyingClient alloc] init]);
    [underlying_client_ setUnderlyingClient:
        static_cast<id<CRNNetworkClientProtocol>>(mock_web_proxy_)];

    // Link mock proxy into the JSInjectionNetworkClient.
    js_injection_client_.reset([[CRWJSInjectionNetworkClient alloc] init]);
    [js_injection_client_ setUnderlyingClient:underlying_client_];

    // Load data for testing
    base::FilePath file_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
    file_path = file_path.AppendASCII(kTestFile);
    test_data_.reset([[NSData dataWithContentsOfFile:
        base::SysUTF8ToNSString(file_path.value())] retain]);
    ASSERT_TRUE(test_data_);
  }

  void TearDown() override { EXPECT_OCMOCK_VERIFY(mock_web_proxy_); }

 protected:
  // Returns a CRNHTTPURLResponse. If |include_content_length|, header includes
  // Content-Length set to the length of test_data_.
  CRNHTTPURLResponse* CreateTestResponse(BOOL include_content_length);

  // Returns number of times an injected cr_web script tag is found in the
  // underlying client's loaded data. Script tag should immediately follow
  // the html start tag, if it exists, or should be injected before any header,
  // if the first tag is something other than an html start tag.
  NSUInteger GetScriptTagCount() const;

  // Checks that if response forwarded to the underlying client has header field
  // Content-Length, the value matches the length of the data.
  void ExpectConsistentContentLength();

  base::scoped_nsobject<CRWJSInjectionNetworkClient> js_injection_client_;
  base::scoped_nsobject<UnderlyingClient> underlying_client_;
  base::scoped_nsobject<OCMockObject> mock_web_proxy_;
  base::scoped_nsobject<NSData> test_data_;
};

CRNHTTPURLResponse* CRWJSInjectionNetworkClientTest::CreateTestResponse(
    BOOL include_content_length) {
  NSMutableDictionary *headers = [NSMutableDictionary
      dictionaryWithDictionary:@{ @"Content-Type" : @"text/html" }];
  if (include_content_length) {
    headers[@"Content-Length"] = @([test_data_ length]).stringValue;
  }
  return [[CRNHTTPURLResponse alloc]
      initWithURL:[NSURL URLWithString:@"http://testjsinjection.html"]
       statusCode:200
      HTTPVersion:@"HTTP/1.1"
     headerFields:headers];
};

NSUInteger CRWJSInjectionNetworkClientTest::GetScriptTagCount() const {
  base::scoped_nsobject<NSString> data_string(
      [[NSString alloc] initWithData:[underlying_client_ loadedData]
                            encoding:NSUTF8StringEncoding]);
  NSRegularExpression* script_tag_reg_exp = [NSRegularExpression
      regularExpressionWithPattern:@"(^|<html>)<script src="
                                    "\"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-"
                                    "[0-9a-f]{4}-[0-9a-f]{12}+_crweb\\.js\" "
                                    "charset=\"utf-8\"></script>"
                           options:NSRegularExpressionCaseInsensitive
                             error:nil];
  return [script_tag_reg_exp
      numberOfMatchesInString:data_string
                      options:0
                        range:NSMakeRange(0, [data_string length])];
}

void CRWJSInjectionNetworkClientTest::ExpectConsistentContentLength() {
  NSData* forwarded_data = [underlying_client_ loadedData];
  NSDictionary* output_headers =
      [static_cast<NSHTTPURLResponse*>([underlying_client_ receivedResponse])
          allHeaderFields];
  ASSERT_TRUE(output_headers);
  NSInteger content_length = [output_headers[@"Content-Length"] integerValue];
  if (content_length) {
    EXPECT_EQ(static_cast<NSInteger>([forwarded_data length]),
        (content_length));
  }
}

}  // namespace

#pragma mark - Tests

// Tests injection where response header has Content-Length. Checks that
// Content-Length is updated to match new size of data.
TEST_F(CRWJSInjectionNetworkClientTest, InjectionWithContentLength) {
  base::scoped_nsobject<CRNHTTPURLResponse> test_response(
      CreateTestResponse(YES));
  [js_injection_client_ didReceiveResponse:test_response];
  [js_injection_client_ didLoadData:test_data_];
  [js_injection_client_ didFinishLoading];

  EXPECT_EQ(1u, GetScriptTagCount());
  ExpectConsistentContentLength();
}

// Tests injection where response header does not have Content-Length.
TEST_F(CRWJSInjectionNetworkClientTest, InjectionWithoutContentLength) {
  base::scoped_nsobject<CRNHTTPURLResponse> test_response(
      CreateTestResponse(NO));
  [js_injection_client_ didReceiveResponse:test_response];
  [js_injection_client_ didLoadData:test_data_];
  [js_injection_client_ didFinishLoading];

  EXPECT_EQ(1u, GetScriptTagCount());
  ExpectConsistentContentLength();
}

// Tests that injection occurs at the beginning of the file if data has no html
// start tag but does have other tags within the first 1KB.
TEST_F(CRWJSInjectionNetworkClientTest, MissingHTMLTag) {
  base::scoped_nsobject<NSString> test_string(
      [[NSString alloc] initWithData:test_data_
                            encoding:NSUTF8StringEncoding]);
  NSData* truncated_data =
      [[test_string stringByReplacingOccurrencesOfString:@"<html>"
                                              withString:@""]
          dataUsingEncoding:NSUTF8StringEncoding];
  test_data_.reset([truncated_data retain]);
  ASSERT_TRUE(test_data_);

  base::scoped_nsobject<CRNHTTPURLResponse> test_response(
      CreateTestResponse(YES));
  [js_injection_client_ didReceiveResponse:test_response];
  [js_injection_client_ didLoadData:test_data_];
  [js_injection_client_ didFinishLoading];

  EXPECT_EQ(1u, GetScriptTagCount());
  ExpectConsistentContentLength();
}

// Tests that injection occurs just following the html tag when there is < 1KB
// of padding preceding the html start tag.
TEST_F(CRWJSInjectionNetworkClientTest, LessThan1KBBeforeHTMLTag) {
  base::scoped_nsobject<NSString> test_string(
      [[NSString alloc] initWithData:test_data_
                            encoding:NSUTF8StringEncoding]);
  NSData* padded_data = [[GetLongString(900u)
      stringByAppendingString:test_string]
          dataUsingEncoding:NSUTF8StringEncoding];
  test_data_.reset([padded_data retain]);
  ASSERT_TRUE(test_data_);

  base::scoped_nsobject<CRNHTTPURLResponse> test_response(
      CreateTestResponse(YES));
  [js_injection_client_ didReceiveResponse:test_response];
  [js_injection_client_ didLoadData:test_data_];
  [js_injection_client_ didFinishLoading];

  EXPECT_EQ(1u, GetScriptTagCount());
  ExpectConsistentContentLength();
}

TEST_F(CRWJSInjectionNetworkClientTest, PaddedMissingHTMLTag) {
  base::scoped_nsobject<NSString> test_string(
      [[NSString alloc] initWithData:test_data_
                            encoding:NSUTF8StringEncoding]);
  test_string.reset(
      [[test_string stringByReplacingOccurrencesOfString:@"<html>"
                                              withString:@""] retain]);
  NSData* padded_data = [[GetLongString(900u)
      stringByAppendingString:test_string]
            dataUsingEncoding:NSUTF8StringEncoding];
  test_data_.reset([padded_data retain]);
  ASSERT_TRUE(test_data_);

  base::scoped_nsobject<CRNHTTPURLResponse> test_response(
      CreateTestResponse(YES));
  [js_injection_client_ didReceiveResponse:test_response];
  [js_injection_client_ didLoadData:test_data_];
  [js_injection_client_ didFinishLoading];

  EXPECT_EQ(1u, GetScriptTagCount());
  ExpectConsistentContentLength();
}

// Tests scenario in which data is loaded one byte at a time, as might occur
// under a slow connection.
TEST_F(CRWJSInjectionNetworkClientTest, FragmentedDataLoad) {
  base::scoped_nsobject<CRNHTTPURLResponse> test_response(
      CreateTestResponse(YES));
  [js_injection_client_ didReceiveResponse:test_response];
  // Load data one byte at a time.
  for (NSUInteger i = 0; i < [test_data_ length]; i++) {
    [js_injection_client_ didLoadData:
        [test_data_ subdataWithRange:NSMakeRange(i, 1u)]];
  }
  [js_injection_client_ didFinishLoading];

  EXPECT_EQ(1u, GetScriptTagCount());
  ExpectConsistentContentLength();
}
