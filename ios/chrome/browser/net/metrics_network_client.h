// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_METRICS_NETWORK_CLIENT_H_
#define IOS_CHROME_BROWSER_NET_METRICS_NETWORK_CLIENT_H_

#import "ios/net/clients/crn_forwarding_network_client.h"

@class MetricsNetworkClientManager;

// MetricsNetworkClient records UMA metrics about the network requests.
@interface MetricsNetworkClient : CRNForwardingNetworkClient

- (instancetype)initWithManager:(MetricsNetworkClientManager*)manager;

@end

#endif  // IOS_CHROME_BROWSER_NET_METRICS_NETWORK_CLIENT_H_
