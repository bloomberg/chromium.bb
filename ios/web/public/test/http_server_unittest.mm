// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server.h"

#import <Foundation/Foundation.h>

#include <string>

#import "base/ios/ios_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/test/response_providers/data_response_provider.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

// A response provider that returns a simple string for all requests. Used for
// testing purposes.
class TestResponseProvider : public web::DataResponseProvider {
 public:
  bool CanHandleRequest(const Request& request) override {
    return true;
  }

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    *headers = GetDefaultResponseHeaders();
    *response_body = response_body_;
  }
  // The string that is returned in the response body.
  std::string response_body_;
};

}  // namespace

// Tests that a web::test::HttpServer can be started and can send and receive
// requests and response from |TestResponseProvider|
TEST(HTTPServer, StartAndInterfaceWithResponseProvider) {
  // Disabled on iOS 9 as it fails with App Transport Security error.
  // Tracked by http://crbug.com/516600 issue.
  if (base::ios::IsRunningOnIOS9OrLater())
    return;

  scoped_ptr<TestResponseProvider> provider(new TestResponseProvider());
  const std::string kHelloWorld = "Hello World";
  provider->response_body_ = kHelloWorld;
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  const NSUInteger kPort = 8080;
  server.StartOnPort(kPort);
  EXPECT_TRUE(server.IsRunning());
  server.AddResponseProvider(provider.release());

  NSString* url =
      [NSString stringWithFormat:@"http://localhost:%@/samp", @(kPort)];
  __block base::scoped_nsobject<NSString> evaluation_result;
  id completion_handler =
      ^(NSData* data, NSURLResponse* response, NSError* error) {
          evaluation_result.reset([[NSString alloc]
              initWithData:data encoding:NSUTF8StringEncoding]);
      };
  NSURLSessionDataTask* data_task =
      [[NSURLSession sharedSession] dataTaskWithURL:[NSURL URLWithString:url]
                                  completionHandler:completion_handler];
  [data_task resume];
  base::test::ios::WaitUntilCondition(^bool() {
    return evaluation_result;
  });
  EXPECT_NSEQ(evaluation_result, base::SysUTF8ToNSString(kHelloWorld));
  server.Stop();
  EXPECT_FALSE(server.IsRunning());
}
