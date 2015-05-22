// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CLIENTS_CRW_REDIRECT_NETWORK_CLIENT_FACTORY_H_
#define IOS_WEB_NET_CLIENTS_CRW_REDIRECT_NETWORK_CLIENT_FACTORY_H_

#import <Foundation/Foundation.h>

#import "ios/web/net/clients/crw_redirect_network_client.h"
#import "ios/net/clients/crn_forwarding_network_client_factory.h"

// Factory that creates CRWRedirectNetworkClient instances.
@interface CRWRedirectNetworkClientFactory : CRNForwardingNetworkClientFactory

// Designated initializer.  |delegate| is expected to be non-nil.
- (instancetype)initWithDelegate:(id<CRWRedirectClientDelegate>)delegate;

@end

#endif  // IOS_WEB_NET_CLIENTS_CRW_REDIRECT_NETWORK_CLIENT_FACTORY_H_
