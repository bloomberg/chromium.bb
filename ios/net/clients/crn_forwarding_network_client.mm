// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/clients/crn_forwarding_network_client.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/threading/thread_checker.h"

@implementation CRNForwardingNetworkClient {
  // Next client in the client chain.
  base::scoped_nsprotocol<id<CRNNetworkClientProtocol>> _underlyingClient;
  base::ThreadChecker _threadChecker;
}

#pragma mark CRNNetworkClientProtocol methods

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient didFailWithNSErrorCode:nsErrorCode
                               netErrorCode:netErrorCode];
}

- (void)didLoadData:(NSData*)data {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient didLoadData:data];
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient didReceiveResponse:response];
}

- (void)wasRedirectedToRequest:(NSURLRequest*)request
                 nativeRequest:(net::URLRequest*)nativeRequest
              redirectResponse:(NSURLResponse*)response {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient wasRedirectedToRequest:request
                              nativeRequest:nativeRequest
                           redirectResponse:response];
}

- (void)didFinishLoading {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient didFinishLoading];
}

- (void)didCreateNativeRequest:(net::URLRequest*)nativeRequest {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient didCreateNativeRequest:nativeRequest];
}

- (void)didRecieveAuthChallenge:(net::AuthChallengeInfo*)authInfo
                  nativeRequest:(const net::URLRequest&)nativeRequest
                       callback:(const network_client::AuthCallback&)callback {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient didRecieveAuthChallenge:authInfo
                               nativeRequest:nativeRequest
                                    callback:callback];
}

- (void)cancelAuthRequest {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(_underlyingClient);
  [_underlyingClient cancelAuthRequest];
}

- (id<CRNNetworkClientProtocol>)underlyingClient {
  return _underlyingClient;
}

- (void)setUnderlyingClient:(id<CRNNetworkClientProtocol>)underlyingClient {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(underlyingClient);
  _underlyingClient.reset([underlyingClient retain]);
}

@end
