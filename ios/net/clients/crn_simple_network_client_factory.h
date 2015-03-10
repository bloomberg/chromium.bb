// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CLIENTS_CRN_SIMPLE_NETWORK_CLIENT_FACTORY_H_
#define IOS_NET_CLIENTS_CRN_SIMPLE_NETWORK_CLIENT_FACTORY_H_

#import <Foundation/Foundation.h>

#import "ios/net/clients/crn_forwarding_network_client_factory.h"

// Simple factory that returns a new instance of the given class on each call to
// |clientHandlingAnyRequestWithTracker:|.
// Since all CRNSimpleNetworkClientFactory share the same type, even if their
// underlying client class is different, their support for factory ordering
// through |+mustApplyAfter| and |+mustApplyBefore| is limited.
@interface CRNSimpleNetworkClientFactory : CRNForwardingNetworkClientFactory

// |clientClass| must be a subclass of CRNForwardingNetworkClient.
- (instancetype)initWithClass:(Class)clientClass;

@end

#endif  // IOS_NET_CLIENTS_CRN_SIMPLE_NETWORK_CLIENT_FACTORY_H_
