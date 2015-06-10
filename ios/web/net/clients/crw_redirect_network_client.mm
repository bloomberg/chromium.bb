// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/clients/crw_redirect_network_client.h"

#include "base/location.h"
#include "base/mac/bind_objc_block.h"
#include "ios/web/public/web_thread.h"

@interface CRWRedirectNetworkClient () {
  // The delegate.
  base::WeakNSProtocol<id<CRWRedirectClientDelegate>> delegate_;
}

@end

@implementation CRWRedirectNetworkClient

- (instancetype)initWithDelegate:
    (base::WeakNSProtocol<id<CRWRedirectClientDelegate>>)delegate {
  self = [super init];
  if (self) {
    // CRWRedirectNetworkClients are created on the IO thread, but due to the
    // threading restrictions of WeakNSObjects, |delegate_| may only be
    // dereferenced on the UI thread.
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
    delegate_ = delegate;
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
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  web::WebThread::PostTask(web::WebThread::UI, FROM_HERE, base::BindBlock(^{
    // |delegate_| can only be dereferenced from the UI thread.
    [delegate_ wasRedirectedToRequest:request
                     redirectResponse:redirectResponse];
  }));
  [super wasRedirectedToRequest:request
                  nativeRequest:nativeRequest
               redirectResponse:redirectResponse];
}

@end
