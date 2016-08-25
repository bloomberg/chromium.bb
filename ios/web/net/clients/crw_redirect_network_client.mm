// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/clients/crw_redirect_network_client.h"

#include "base/location.h"
#include "base/mac/bind_objc_block.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWRedirectNetworkClient () {
  // The delegate.
  __weak id<CRWRedirectClientDelegate> _delegate;
}

@end

@implementation CRWRedirectNetworkClient

- (instancetype)initWithDelegate:(id<CRWRedirectClientDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)wasRedirectedToRequest:(NSURLRequest*)request
                 nativeRequest:(net::URLRequest*)nativeRequest
              redirectResponse:(NSURLResponse*)redirectResponse {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  web::WebThread::PostTask(web::WebThread::UI, FROM_HERE, base::BindBlock(^{
                             [_delegate
                                 wasRedirectedToRequest:request
                                       redirectResponse:redirectResponse];
                           }));
  [super wasRedirectedToRequest:request
                  nativeRequest:nativeRequest
               redirectResponse:redirectResponse];
}

@end
