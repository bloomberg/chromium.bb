// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#include <stdint.h>

#import "CrNet.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/third_party/gcdwebserver/src/GCDWebServer/Core/GCDWebServer.h"
#include "net/base/mac/url_conversions.h"
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

// Total count of bytes received by the request this delegate is attached to.
@property(nonatomic) unsigned long receivedBytes;

// Error the request this delegate is attached to failed with, if any.
@property(retain, nonatomic) NSError* error;

@end

@implementation TestDelegate
@synthesize semaphore = _semaphore;
@synthesize receivedBytes = _receivedBytes;
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
  _receivedBytes += (unsigned long)data.length;
}

- (void)URLSession:(NSURLSession*)session
             dataTask:(NSURLSessionDataTask*)dataTask
    willCacheResponse:(NSCachedURLResponse*)proposedResponse
    completionHandler:
        (void (^)(NSCachedURLResponse* cachedResponse))completionHandler {
  completionHandler(proposedResponse);
}

@end

// base::TimeDelta would normally be ideal for this but it does not support
// nanosecond resolution.
static const int64_t ns_in_second = 1000000000LL;

class HttpTest : public ::testing::Test {
 protected:
  HttpTest() {}
  ~HttpTest() override {}

  void SetUp() override {
    [CrNet setPartialUserAgent:@"CrNetTest/1.0.0.0"];
    [CrNet install];
    NSURLSessionConfiguration* config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    [CrNet installIntoSessionConfiguration:config];
    delegate_.reset([[TestDelegate alloc] init]);
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config
                                                          delegate:delegate_
                                                     delegateQueue:nil];
    // Take a reference to the session and store it so it doesn't get
    // deallocated until this object does.
    session_.reset([session retain]);
    web_server_.reset([[GCDWebServer alloc] init]);
  }

  void TearDown() override {
    [CrNet uninstall];
    [web_server_ stop];
  }

  // Starts a GCDWebServer instance on localhost port 8080, and remembers the
  // root URL for later; tests can use GetURL() to produce a URL referring to a
  // specific resource under the root URL.
  void StartWebServer() {
    [web_server_ startWithPort:8080 bonjourName:nil];
    server_root_ = net::GURLWithNSURL([web_server_ serverURL]);
  }

  // Registers a fixed response |text| to be returned to requests for |path|,
  // which is relative to |server_root_|.
  void RegisterPathText(const std::string& path, const std::string& text) {
    NSString* nspath = base::SysUTF8ToNSString(path);
    NSData* data = [NSData dataWithBytes:text.c_str() length:text.length()];
    [web_server_ addGETHandlerForPath:nspath
                          staticData:data
                         contentType:@"text/plain"
                            cacheAge:30];
  }

  void RegisterPathHandler(const std::string& path,
                           GCDWebServerProcessBlock handler) {
    NSString* nspath = base::SysUTF8ToNSString(path);
    [web_server_ addHandlerForMethod:@"GET"
                                path:nspath
                        requestClass:NSClassFromString(@"GCDWebServerRequest")
                        processBlock:handler];
  }

  // Launches the supplied |task| and blocks until it completes, with a timeout
  // of 1 second.
  void StartDataTaskAndWaitForCompletion(NSURLSessionDataTask* task) {
    [task resume];
    int64_t deadline_ns = 1 * ns_in_second;
    dispatch_semaphore_wait([delegate_ semaphore],
                            dispatch_time(DISPATCH_TIME_NOW, deadline_ns));
  }

  // Returns a URL to refer to the resource named |path| served by the test
  // server. If |path| starts with a /, the leading / will be stripped.
  GURL GetURL(const std::string& path) {
    std::string real_path = path[0] == '/' ? path.substr(1) : path;
    return server_root_.Resolve(real_path);
  }

  // Some convenience functions for working with GCDWebServerRequest and
  // GCDWebServerResponse.

  // Returns true if the value for the request header |header| is not nil and
  // contains the string |target|.
  bool HeaderValueContains(GCDWebServerRequest* request,
                           const std::string& header,
                           const std::string& target) {
    NSString* key = base::SysUTF8ToNSString(header);
    NSString* needle = base::SysUTF8ToNSString(target);
    NSString* haystack = request.headers[key];
    if (!haystack)
      return false;
    return [haystack rangeOfString:needle].location != NSNotFound;
  }

  base::scoped_nsobject<NSURLSession> session_;
  base::scoped_nsobject<TestDelegate> delegate_;

 private:
  base::scoped_nsobject<GCDWebServer> web_server_;
  GURL server_root_;
};

TEST_F(HttpTest, NSURLSessionReceivesData) {
  const char kPath[] = "/foo";
  const char kData[] = "foobar";
  RegisterPathText(kPath, kData);
  StartWebServer();

  NSURL* url = net::NSURLWithGURL(GetURL(kPath));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_EQ(strlen(kData), [delegate_ receivedBytes]);
}

TEST_F(HttpTest, SdchDisabledByDefault) {
  const char kPath[] = "/foo";
  RegisterPathHandler(kPath,
      ^GCDWebServerResponse* (GCDWebServerRequest* req) {
        EXPECT_FALSE(HeaderValueContains(req, "Accept-Encoding", "sdch"));
        return nil;
      });
  StartWebServer();
  NSURL* url = net::NSURLWithGURL(GetURL(kPath));
  NSURLSessionDataTask* task = [session_ dataTaskWithURL:url];
  StartDataTaskAndWaitForCompletion(task);
  EXPECT_EQ(nil, [delegate_ error]);
  EXPECT_TRUE([delegate_ receivedBytes]);
}

// TODO(ellyjones): There needs to be a test that enabling SDCH works, but
// because CrNet is static and 'uninstall' only disables it, there is no way to
// have an individual test enable or disable SDCH.
// Probably there is a way to get gtest tests to run in a separate process, but
// I'm not sure what it is.
