// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "NetworkCommunication.h"

@interface NSURLSession (PartialAvailability)
- (NSURLSessionDataTask*)dataTaskWithRequest:(NSURLRequest*)request
                           completionHandler:
                               (void (^)(NSData* data,
                                         NSURLResponse* response,
                                         NSError* error))completionHandler;
- (NSURLSessionDownloadTask*)
downloadTaskWithRequest:(NSURLRequest*)request
      completionHandler:(void (^)(NSURL* location,
                                  NSURLResponse* response,
                                  NSError* error))completionHandler;
@end

@implementation NetworkCommunication : NSObject

@synthesize session = session_;
@synthesize request = request_;
@synthesize dataResponseHandler = dataResponseHandler_;

- (id)init {
  return [self initWithDelegate:nil];
}

- (id)initWithDelegate:(id)delegate {
  if ((self = [super init])) {
    NSURLSessionConfiguration* sessionConfig =
        [NSURLSessionConfiguration defaultSessionConfiguration];
    session_ = [NSURLSession sessionWithConfiguration:sessionConfig
                                             delegate:delegate
                                        delegateQueue:nil];
  }
  return self;
}

- (NSMutableURLRequest*)createRequestWithUrlAsString:(NSString*)urlString
                                          andXMLBody:(NSXMLDocument*)body {
  NSURL* requestURL = [NSURL URLWithString:urlString];
  request_ = [NSMutableURLRequest requestWithURL:requestURL];
  if (body) {
    [request_ addValue:@"text/xml" forHTTPHeaderField:@"Content-Type"];
    NSData* requestBody =
        [[body XMLString] dataUsingEncoding:NSUTF8StringEncoding];
    request_.HTTPBody = requestBody;
  }
  return request_;
}

- (void)sendDataRequest {
  NSURLSessionDataTask* dataTask;
  if (dataResponseHandler_) {
    dataTask = [session_ dataTaskWithRequest:request_
                           completionHandler:dataResponseHandler_];
  } else {
    dataTask = [session_ dataTaskWithRequest:request_];
  }

  [dataTask resume];
}

- (void)sendDownloadRequest {
  NSURLSessionDownloadTask* downloadTask =
      [session_ downloadTaskWithRequest:request_];
  [downloadTask resume];
}

@end
