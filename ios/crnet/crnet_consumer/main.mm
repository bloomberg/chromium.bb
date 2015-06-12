// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "CrNet.h"

#import "crnet_consumer_app_delegate.h"

@interface TestDelegate : NSObject<NSURLSessionDelegate,
                                   NSURLSessionDataDelegate,
                                   NSURLSessionTaskDelegate>

- (id)initWithSemaphore:(dispatch_semaphore_t)sem;

@end

@implementation TestDelegate {
  dispatch_semaphore_t _sem;
}

- (id)initWithSemaphore:(dispatch_semaphore_t)sem {
  _sem = sem;
  return self;
}

- (void)URLSession:(NSURLSession*)session
    didBecomeInvalidWithError:(NSError*)error {
  NSLog(@"URLSession didBecomeInvalidWithError %@", error);
}

- (void)URLSession:(NSURLSession*)session
                    task:(NSURLSessionTask*)task
    didCompleteWithError:(NSError*)error {
  NSLog(@"URLSessionTask didCompleteWithError %@", error);
  dispatch_semaphore_signal(_sem);
}

- (void)URLSession:(NSURLSession*)session
                   task:(NSURLSessionTask*)task
    didReceiveChallenge:(NSURLAuthenticationChallenge*)challenge
      completionHandler:
          (void (^)(NSURLSessionAuthChallengeDisposition disp,
                    NSURLCredential* credential))completionHandler {
  NSLog(@"URLSessionTask didReceiveChallenge %@", challenge);
  completionHandler(NSURLSessionAuthChallengeUseCredential, nil);
}

- (void)URLSession:(NSURLSession*)session
                          task:(NSURLSessionTask*)task
    willPerformHTTPRedirection:(NSHTTPURLResponse*)response
                    newRequest:(NSURLRequest*)request
             completionHandler:(void (^)(NSURLRequest*))completionHandler {
  NSLog(@"URLSessionTask willPerformHttpRedirection %@", request);
  completionHandler(request);
}

- (void)URLSession:(NSURLSession*)session
              dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveResponse:(NSURLResponse*)response
     completionHandler:(void (^)(NSURLSessionResponseDisposition disposition))
                           completionHandler {
  NSHTTPURLResponse* resp = (NSHTTPURLResponse*)response;
  NSLog(@"URLSessionDataTask didReceiveResponse status %ld",
        (long)resp.statusCode);
  completionHandler(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)session
          dataTask:(NSURLSessionDataTask*)dataTask
    didReceiveData:(NSData*)data {
  NSLog(@"URLSessionDataTask didReceiveData %lu bytes",
        (unsigned long)data.length);
}

- (void)URLSession:(NSURLSession*)session
             dataTask:(NSURLSessionDataTask*)dataTask
    willCacheResponse:(NSCachedURLResponse*)proposedResponse
    completionHandler:
        (void (^)(NSCachedURLResponse* cachedResponse))completionHandler {
  NSLog(@"URLSessionDataTask willCacheResponse %@", proposedResponse);
  completionHandler(proposedResponse);
}

@end

void use_crnet(NSURLSessionConfiguration* config) {
  [CrNet setPartialUserAgent:@"Foo/1.0"];
  [CrNet install];
  [CrNet installIntoSessionConfiguration:config];
}

void fetch(NSURLSession* session, NSString* url) {
  NSURL* testURL = [NSURL URLWithString:url];
  NSURLSessionDataTask* task = [session dataTaskWithURL:testURL];
  NSLog(@"fetch: starting %@", url);
  [task resume];
}

int main(int argc, char *argv[]) {
  dispatch_semaphore_t sem = dispatch_semaphore_create(0);
  TestDelegate* delegate = [[TestDelegate alloc] initWithSemaphore:sem];
  NSURLSessionConfiguration* config =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  NSURLSession* session = [NSURLSession sessionWithConfiguration:config
                                                        delegate:delegate
                                                   delegateQueue:nil];

  NSLog(@"main: installing crnet");
  use_crnet(config);

  fetch(session, @"https://www.google.com");
  fetch(session, @"https://twitter.com");
  fetch(session, @"https://m.facebook.com");
  fetch(session, @"https://www.yahoo.com");

  int64_t secs = 1000000000LL;
  secs *= 5LL;
  NSLog(@"main: waiting");
  dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, secs));
  dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, secs));
  dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, secs));
  dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, secs));
  NSLog(@"main: timeout, exiting");
}
