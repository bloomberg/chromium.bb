// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "OmahaCommunication.h"

@implementation OmahaCommunication : NSObject

@synthesize requestXMLBody = requestXMLBody_;
@synthesize sessionHelper = sessionHelper_;

- (id)init {
  return [self initWithBody:[[NSXMLDocument alloc] init]];
}

- (id)initWithBody:(NSXMLDocument*)xmlBody {
  if ((self = [super init])) {
    sessionHelper_ = [[NetworkCommunication alloc] init];
    requestXMLBody_ = xmlBody;
    [self createOmahaRequest];
  }
  return self;
}

- (NSURLRequest*)createOmahaRequest {
  // TODO: turn this string to a comand-line flag
  NSMutableURLRequest* request = [sessionHelper_
      createRequestWithUrlAsString:@"https://tools.google.com/service/update2"
                        andXMLBody:requestXMLBody_];
  request.HTTPMethod = @"POST";
  return request;
}

- (void)sendRequestWithBlock:(OmahaRequestCompletionHandler)block {
  DataTaskCompletionHandler cHandler =
      ^(NSData* _Nullable data, NSURLResponse* _Nullable response,
        NSError* _Nullable error) {
        if (error) {
          NSLog(@"%@", error);
          block(data, error);
          return;
        }

        NSHTTPURLResponse* HTTPResponse = (NSHTTPURLResponse*)response;
        if (HTTPResponse.statusCode != 200) {
          // TODO: make these logging statements more rare
          NSLog(@"HTTP response: %ld", (unsigned long)HTTPResponse.statusCode);
        }

        block(data, error);

      };

  [sessionHelper_ sendDataRequestWithCompletionHandler:cHandler];
}

@end