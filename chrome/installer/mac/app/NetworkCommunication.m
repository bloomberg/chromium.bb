// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "NetworkCommunication.h"

@implementation NetworkCommunication : NSObject

@synthesize session = session_;
@synthesize request = request_;
@synthesize dataResponseHandler = dataResponseHandler_;
@synthesize downloadResponseHandler = downloadResponseHandler_;

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

- (void)sendDataRequestWithCompletionHandler:
    (DataTaskCompletionHandler)completionHandler {
  dataResponseHandler_ = completionHandler;
  NSURLSessionDataTask* dataTask =
      [session_ dataTaskWithRequest:request_
                  completionHandler:dataResponseHandler_];

  [dataTask resume];
}

- (void)sendDownloadRequest {
  NSURLSessionDownloadTask* downloadTask;
  if (downloadResponseHandler_) {
    downloadTask = [session_ downloadTaskWithRequest:request_
                                   completionHandler:downloadResponseHandler_];
  } else {
    downloadTask = [session_ downloadTaskWithRequest:request_];
  }
  [downloadTask resume];
}

@end
