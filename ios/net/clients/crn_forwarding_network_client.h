// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CLIENTS_CRN_FORWARDING_NETWORK_CLIENT_H_
#define IOS_NET_CLIENTS_CRN_FORWARDING_NETWORK_CLIENT_H_

#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"
#import "ios/net/clients/crn_network_client_protocol.h"

// Basic CRNNetworkClientProtocol that forwards all the calls to the underlying
// client. Suitable for injecting via RequestTracker.
@interface CRNForwardingNetworkClient : NSObject<CRNNetworkClientProtocol>

@property(nonatomic, assign) id<CRNNetworkClientProtocol> underlyingClient;

@end

#endif  // IOS_NET_CLIENTS_CRN_FORWARDING_NETWORK_CLIENT_H_
