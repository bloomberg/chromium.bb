// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/clients/crw_passkit_network_client_factory.h"

#import "ios/web/net/clients/crw_passkit_network_client.h"
#include "ios/web/net/request_tracker_impl.h"
#include "ios/web/public/web_thread.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

@implementation CRWPassKitNetworkClientFactory {
  // Delegate that's passed on to any created clients.
  id<CRWPassKitDelegate> _delegate;
  web::RequestTrackerImpl* _tracker;
}

- (Class)clientClass {
  return [CRWPassKitNetworkClient class];
}

- (CRNForwardingNetworkClient*)
    clientHandlingResponse:(NSURLResponse*)response
                   request:(const net::URLRequest&)request {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  // Some requests (e.g., for chrome:// urls) will not have response headers.
  net::HttpResponseHeaders* responseHeaders = request.response_headers();
  if (!responseHeaders)
    return nil;

  std::string mimeType;
  BOOL hasMimeType = responseHeaders->GetMimeType(&mimeType);
  if (hasMimeType && mimeType == "application/vnd.apple.pkpass") {
    return [[[CRWPassKitNetworkClient alloc]
        initWithTracker:_tracker->GetWeakPtr()
               delegate:_delegate] autorelease];
  }

  return nil;
}

- (instancetype)initWithDelegate:(id<CRWPassKitDelegate>)delegate
                         tracker:(web::RequestTrackerImpl*)tracker {
  if ((self = [super init])) {
    DCHECK(delegate);
    _delegate = delegate;
    _tracker = tracker;
  }
  return self;
}

@end
