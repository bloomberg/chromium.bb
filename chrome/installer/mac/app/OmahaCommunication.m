// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "OmahaCommunication.h"

#import "OmahaXMLRequest.h"

@implementation OmahaCommunication

@synthesize requestXMLBody = requestXMLBody_;
@synthesize sessionHelper = sessionHelper_;
@synthesize delegate = delegate_;

- (id)init {
  return [self initWithBody:[OmahaXMLRequest createXMLRequestBody]];
}

- (id)initWithBody:(NSXMLDocument*)xmlBody {
  if ((self = [super init])) {
    sessionHelper_ = [[NetworkCommunication alloc] initWithDelegate:self];
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

- (void)sendRequest {
  [sessionHelper_ setDataResponseHandler:^(NSData* _Nullable data,
                                           NSURLResponse* _Nullable response,
                                           NSError* _Nullable error) {
    [delegate_ onOmahaSuccessWithResponseBody:data AndError:error];
  }];
  [sessionHelper_ sendDataRequest];
}

@end