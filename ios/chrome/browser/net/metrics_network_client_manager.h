// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_METRICS_NETWORK_CLIENT_MANAGER_H_
#define IOS_CHROME_BROWSER_NET_METRICS_NETWORK_CLIENT_MANAGER_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"
#import "ios/net/clients/crn_forwarding_network_client_factory.h"
#include "url/gurl.h"

@interface PageLoadTimeRecord : NSObject
@property(nonatomic, assign) BOOL dataProxyUsed;
@end

// Factory that creates MetricsNetworkClient instances.
// Each Tab that cares to report page load metrics should create an instance
// of this class add add it to its request tracker.
@interface MetricsNetworkClientManager : CRNForwardingNetworkClientFactory

// Called by the tab when a new page load is about to start, to signal the
// current page URL.
- (void)pageLoadStarted:(GURL)url;

// Called by the tab when the page load is complete.
- (void)pageLoadCompleted;

// Return a page load time record that will be used to record a load of |url|
// that started at |time|. Returns nil if |url| doesn't match the current page
// url. The page load record is owned by the target, and the caller should not
// take ownership of it.
// This method should only be called on the IO thread.
- (PageLoadTimeRecord*)recordForPageLoad:(const GURL&)url
                                    time:(base::TimeTicks)time;
@end

#endif  // IOS_CHROME_BROWSER_NET_METRICS_NETWORK_CLIENT_MANAGER_H_
