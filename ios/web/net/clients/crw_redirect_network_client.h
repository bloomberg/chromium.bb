// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CLIENTS_CRW_REDIRECT_NETWORK_CLIENT_H_
#define IOS_WEB_NET_CLIENTS_CRW_REDIRECT_NETWORK_CLIENT_H_

#import <Foundation/Foundation.h>

#include "base/ios/weak_nsobject.h"
#import "ios/net/clients/crn_forwarding_network_client.h"

@protocol CRWRedirectClientDelegate;

// The CRWRedirectNetworkClient observes redirects and notifies its delegate
// when they occur.
@interface CRWRedirectNetworkClient : CRNForwardingNetworkClient

// Designated initializer.
- (instancetype)initWithDelegate:
    (base::WeakNSProtocol<id<CRWRedirectClientDelegate>>)delegate;

@end

// Delegate for CRWRedirectNetworkClients.
@protocol CRWRedirectClientDelegate<NSObject>

// Notifies the delegate that the current http request recieved |response| and
// was redirected to |request|.
- (void)wasRedirectedToRequest:(NSURLRequest*)request
              redirectResponse:(NSURLResponse*)response;

@end

#endif  // IOS_WEB_NET_CLIENTS_CRW_REDIRECT_NETWORK_CLIENT_H_
