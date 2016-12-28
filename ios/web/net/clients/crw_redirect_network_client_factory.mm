// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/clients/crw_redirect_network_client_factory.h"

#include "base/location.h"
#include "ios/web/public/web_thread.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWRedirectNetworkClientFactory () {
  // Delegate passed to each vended CRWRedirectClient.
  __weak id<CRWRedirectClientDelegate> _client_delegate;
}

@end

@implementation CRWRedirectNetworkClientFactory

- (instancetype)initWithDelegate:(id<CRWRedirectClientDelegate>)delegate {
  self = [super init];
  if (self) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    DCHECK(delegate);
    _client_delegate = delegate;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

#pragma mark - CRNForwardingNetworkClientFactory

- (Class)clientClass {
  return [CRWRedirectNetworkClient class];
}

- (CRNForwardingNetworkClient*)clientHandlingRedirect:
                                   (const net::URLRequest&)request
                                                  url:(const GURL&)url
                                             response:(NSURLResponse*)response {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return [[CRWRedirectNetworkClient alloc] initWithDelegate:_client_delegate];
}

@end
