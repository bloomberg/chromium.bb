// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CLIENTS_CRW_PASSKIT_NETWORK_CLIENT_FACTORY_H_
#define IOS_WEB_NET_CLIENTS_CRW_PASSKIT_NETWORK_CLIENT_FACTORY_H_

#import <Foundation/Foundation.h>

#import "ios/net/clients/crn_forwarding_network_client_factory.h"

namespace web {
class RequestTrackerImpl;
}

@protocol CRWPassKitDelegate;

// Factory that creates CRWPassKitDelegateNetworkClient instances
@interface CRWPassKitNetworkClientFactory : CRNForwardingNetworkClientFactory

// Designated initializer. A weak pointer to |delegate| will be set on each
// client created.
- (instancetype)initWithDelegate:(id<CRWPassKitDelegate>)delegate
                         tracker:(web::RequestTrackerImpl*)tracker;

@end

#endif  // IOS_WEB_NET_CLIENTS_CRW_PASSKIT_NETWORK_CLIENT_FACTORY_H_
