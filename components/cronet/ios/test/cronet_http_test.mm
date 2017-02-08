// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include <stdint.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "components/cronet/ios/test/start_cronet.h"
#include "components/cronet/ios/test/test_server.h"
#include "components/grpc_support/test/quic_test_server.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#include "url/gurl.h"

@interface TestDelegate : NSObject<NSURLSessionDataDelegate,
                                   NSURLSessionDelegate,
                                   NSURLSessionTaskDelegate>

// Completion semaphore for this TestDelegate. When the request this delegate is
// attached to finishes (either successfully or with an error), this delegate
// signals this semaphore.
@property(assign, nonatomic) dispatch_semaphore_t semaphore;

// Body of response received by the request this delegate is attached to.
@property(retain, nonatomic) NSString* responseBody;

// Error the request this delegate is attached to failed with, if any.
@property(retain, nonatomic) NSError* error;

@end

@implementation TestDelegate
@synthesize semaphore = _semaphore;
@synthesize responseBody = _responseBody;
@synthesize error = _error;

- (id)init {
  if (self = [super init]) {
    _semaphore = dispatch_semaphore_create(0);
  }
  return self;
}

- (void)dealloc {
  dispatch_release(_semaphore);
  [_error release];
  _error = nil;
  [super dealloc];
}

- (void)reset {
  _responseBody = nil;
  _error = nil;
}

- (void)URLSession:(NSURLSession*)session
    didBecomeInvalidWithError:(NSError*)error {
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  [self setError:error];
  dispatch_semaphore_signal(_semaphore);
}

- (void)URLSession:(NSURLSession*)session
                   task:(NSURLSessionTask*)task
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:
          (void (^)(NSURLSessionAuthChallengeDisposition disp,
                    NSURLCredential* credential))completionHandler {
  completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))
                           completionHandler {
  completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  NSString* stringData =
      [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
  if (_responseBody == nil) {
    _responseBody = stringData;
  } else {
    _responseBody = [_responseBody stringByAppendingString:stringData];
  }
}

- (void)URLSession:(NSURLSession*)session
             dataTask:(NSURLSessionDataTask*)dataTask
    willCacheResponse:(NSCachedURLResponse*)proposedResponse
    completionHandler:
        (void (^)(NSCachedURLResponse* cachedResponse))completionHandler {
  completionHandler(proposedResponse);
}

@end

namespace cronet {
// base::TimeDelta would normally be ideal for this but it does not support
// nanosecond resolution.
static const int64_t ns_in_second = 1000000000LL;
const char kUserAgent[] = "CronetTest/1.0.0.0";

class HttpTest : public ::testing::Test {
 protected:
  HttpTest() {}
  ~HttpTest() override {}

  void SetUp() override {
    grpc_support::StartQuicTestServer();
    TestServer::Start();

    [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
      return YES;
    }];
    StartCronetIfNecessary(grpc_support::GetQuicTestServerPort());
    [Cronet registerHttpProtocolHandler];
    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    [Cronet installIntoSessionConfiguration:config];
    delegate_.reset([[TestDelegate alloc] init]);
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config
                                                          delegate:delegate_
                                                     delegateQueue:nil];
    // Take a reference to the session and store it so it doesn't get
    // deallocated until this object does.
    session_.reset([session retain]);
  }

  void TearDown() override {
    grpc_support::ShutdownQuicTestServer();
    TestServer::Shutdown();
  }

  // Launches the supplied |task| and blocks until it completes, with a timeout
  // of 1 second.
  void StartDataTaskAndWaitForCompletion(NSURLSessionDataTask* task) {
    [delegate_ reset];
    [task resume];
    int64_t deadline_ns = 1 * ns_in_second;
    dispatch_semaphore_wait([delegate_ semaphore],
                            dispatch_time(DISPATCH_TIME_NOW, deadline_ns));
  }

  base::scoped_nsobject<NSURLSession> session_;
  base::scoped_nsobject<TestDelegate> delegate_;
};

TEST_F(HttpTest, NSURLSessionReceivesData) {
  NSURL* url = net::NSURLWithGURL(GURL(grpc_support::kTestServerUrl));
  __block BOOL block_used = NO;
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    block_used = YES;
    EXPECT_EQ([request URL], url);
    return YES;
  }];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_TRUE(block_used);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_STREQ(grpc_support::kHelloBodyValue,
               base::SysNSStringToUTF8([delegate_ responseBody]).c_str());
}

TEST_F(HttpTest, GetGlobalMetricsDeltas) {
  NSData* delta1 = [Cronet getGlobalMetricsDeltas];

  NSURL* url = net::NSURLWithGURL(GURL(grpc_support::kTestServerUrl));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_STREQ(grpc_support::kHelloBodyValue,
               base::SysNSStringToUTF8([delegate_ responseBody]).c_str());

  NSData* delta2 = [Cronet getGlobalMetricsDeltas];
  EXPECT_FALSE([delta2 isEqualToData:delta1]);
}

TEST_F(HttpTest, SdchDisabledByDefault) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_FALSE([[delegate_ responseBody] containsString:@"sdch"]);
}

// Verify that explictly setting Accept-Encoding request header to 'gzip,sdch"
// is passed to the server and does not trigger any failures. This behavior may
// In the future Cronet may not allow caller to set Accept-Encoding header and
// could limit it to set of internally suported and enabled encodings, matching
// behavior of Cronet on Android.
TEST_F(HttpTest, AcceptEncodingSdchIsAllowed) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSMutableURLRequest* mutableRequest =
      [[NSURLRequest requestWithURL:url] mutableCopy];
  [mutableRequest addValue:@"gzip,sdch" forHTTPHeaderField:@"Accept-Encoding"];
  NSURLSessionDataTask* task = [session_ dataTaskWithRequest:mutableRequest];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:@"gzip,sdch"]);
}

// Verify that explictly setting Accept-Encoding request header to 'foo,bar"
// is passed to the server and does not trigger any failures. This behavior may
// In the future Cronet may not allow caller to set Accept-Encoding header and
// could limit it to set of internally suported and enabled encodings, matching
// behavior of Cronet on Android.
TEST_F(HttpTest, AcceptEncodingFooBarIsAllowed) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Encoding")));
  NSMutableURLRequest* mutableRequest =
      [[NSURLRequest requestWithURL:url] mutableCopy];
  [mutableRequest addValue:@"foo,bar" forHTTPHeaderField:@"Accept-Encoding"];
  NSURLSessionDataTask* task = [session_ dataTaskWithRequest:mutableRequest];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:@"foo,bar"]);
}

TEST_F(HttpTest, NSURLSessionAcceptLanguage) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("Accept-Language")));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  ASSERT_STREQ("en-US,en", [[delegate_ responseBody] UTF8String]);
}

TEST_F(HttpTest, SetUserAgentIsExact) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("User-Agent")));
  [Cronet setRequestFilterBlock:nil];
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_STREQ(kUserAgent, [[delegate_ responseBody] UTF8String]);
}

TEST_F(HttpTest, SetCookie) {
  const char kCookieHeader[] = "Cookie";
  NSString* cookieName =
      [NSString stringWithFormat:@"SetCookie-%@", [[NSUUID UUID] UUIDString]];
  NSString* cookieValue = [[NSUUID UUID] UUIDString];
  NSString* cookieLine =
      [NSString stringWithFormat:@"%@=%@", cookieName, cookieValue];
  NSHTTPCookieStorage* systemCookieStorage =
      [NSHTTPCookieStorage sharedHTTPCookieStorage];
  NSURL* cookieUrl =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL(kCookieHeader)));
  // Verify that cookie is not set in system storage.
  for (NSHTTPCookie* cookie in [systemCookieStorage cookiesForURL:cookieUrl]) {
    EXPECT_FALSE([[cookie name] isEqualToString:cookieName]);
  }

  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:cookieUrl]);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_EQ(nil, [delegate_ responseBody]);

  NSURL* setCookieUrl = net::NSURLWithGURL(
      GURL(TestServer::GetSetCookieURL(base::SysNSStringToUTF8(cookieLine))));
  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:setCookieUrl]);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieLine]);

  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:cookieUrl]);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieLine]);

  // Verify that cookie is set in system storage.
  NSHTTPCookie* systemCookie = nil;
  for (NSHTTPCookie* cookie in [systemCookieStorage cookiesForURL:cookieUrl]) {
    if ([cookie.name isEqualToString:cookieName]) {
      systemCookie = cookie;
      break;
    }
  }
  EXPECT_TRUE([[systemCookie value] isEqualToString:cookieValue]);
  [systemCookieStorage deleteCookie:systemCookie];
}

TEST_F(HttpTest, SetSystemCookie) {
  const char kCookieHeader[] = "Cookie";
  NSString* cookieName = [NSString
      stringWithFormat:@"SetSystemCookie-%@", [[NSUUID UUID] UUIDString]];
  NSString* cookieValue = [[NSUUID UUID] UUIDString];
  NSHTTPCookieStorage* systemCookieStorage =
      [NSHTTPCookieStorage sharedHTTPCookieStorage];
  NSURL* echoCookieUrl =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL(kCookieHeader)));
  NSHTTPCookie* systemCookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : [echoCookieUrl path],
    NSHTTPCookieName : cookieName,
    NSHTTPCookieValue : cookieValue,
    NSHTTPCookieDomain : [echoCookieUrl host],
  }];
  [systemCookieStorage setCookie:systemCookie];

  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:echoCookieUrl]);
  [systemCookieStorage deleteCookie:systemCookie];
  EXPECT_EQ(nil, [delegate_ error]);
  // Verify that cookie set in system store was sent to the serever.
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieName]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieValue]);
}

TEST_F(HttpTest, SystemCookieWithNullCreationTime) {
  const char kCookieHeader[] = "Cookie";
  NSString* cookieName = [NSString
      stringWithFormat:@"SetSystemCookie-%@", [[NSUUID UUID] UUIDString]];
  NSString* cookieValue = [[NSUUID UUID] UUIDString];
  NSHTTPCookieStorage* systemCookieStorage =
      [NSHTTPCookieStorage sharedHTTPCookieStorage];
  NSURL* echoCookieUrl =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL(kCookieHeader)));
  NSHTTPCookie* nullCreationTimeCookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : [echoCookieUrl path],
    NSHTTPCookieName : cookieName,
    NSHTTPCookieValue : cookieValue,
    NSHTTPCookieDomain : [echoCookieUrl host],
    @"Created" : [NSNumber numberWithDouble:0.0],
  }];
  [systemCookieStorage setCookie:nullCreationTimeCookie];
  NSHTTPCookie* normalCookie = [NSHTTPCookie cookieWithProperties:@{
    NSHTTPCookiePath : [echoCookieUrl path],
    NSHTTPCookieName : [cookieName stringByAppendingString:@"-normal"],
    NSHTTPCookieValue : cookieValue,
    NSHTTPCookieDomain : [echoCookieUrl host],
  }];
  [systemCookieStorage setCookie:normalCookie];
  StartDataTaskAndWaitForCompletion([session_ dataTaskWithURL:echoCookieUrl]);
  [systemCookieStorage deleteCookie:nullCreationTimeCookie];
  [systemCookieStorage deleteCookie:normalCookie];
  EXPECT_EQ(nil, [delegate_ error]);
  // Verify that cookie set in system store was sent to the serever.
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieName]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:cookieValue]);
}

TEST_F(HttpTest, FilterOutRequest) {
  NSURL* url =
      net::NSURLWithGURL(GURL(TestServer::GetEchoHeaderURL("User-Agent")));
  __block BOOL block_used = NO;
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    block_used = YES;
    EXPECT_EQ([request URL], url);
    return NO;
  }];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_TRUE(block_used);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_FALSE([[delegate_ responseBody]
      containsString:base::SysUTF8ToNSString(kUserAgent)]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:@"CFNetwork"]);
}

TEST_F(HttpTest, FileSchemeNotSupported) {
  NSString* fileData = @"Hello, World!";
  NSString* documentsDirectory = [NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
  NSString* filePath = [documentsDirectory
      stringByAppendingPathComponent:[[NSProcessInfo processInfo]
                                         globallyUniqueString]];
  [fileData writeToFile:filePath
             atomically:YES
               encoding:NSUTF8StringEncoding
                  error:nil];

  NSURL* url = [NSURL fileURLWithPath:filePath];
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  [Cronet setRequestFilterBlock:^(NSURLRequest* request) {
    [[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
    EXPECT_TRUE(false) << "Block should not be called for unsupported requests";
    return YES;
  }];
  StartDataTaskAndWaitForCompletion(task);
  [[NSFileManager defaultManager] removeItemAtPath:filePath error:nil];
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([[delegate_ responseBody] containsString:fileData]);
}

}  // namespace cronet
