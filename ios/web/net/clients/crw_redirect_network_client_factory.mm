// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/clients/crw_redirect_network_client_factory.h"

#include "base/location.h"
#import "base/ios/weak_nsobject.h"
#include "base/mac/bind_objc_block.h"
#include "ios/web/public/web_thread.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

@interface CRWRedirectNetworkClientFactory () {
  // Delegate passed to each vended CRWRedirectClient.
  base::WeakNSProtocol<id<CRWRedirectClientDelegate>> client_delegate_;
}

@end

@implementation CRWRedirectNetworkClientFactory

- (instancetype)initWithDelegate:(id<CRWRedirectClientDelegate>)delegate {
  self = [super init];
  if (self) {
    DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
    DCHECK(delegate);
    client_delegate_.reset(delegate);
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
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  return [[[CRWRedirectNetworkClient alloc]
      initWithDelegate:client_delegate_] autorelease];
}

@end
