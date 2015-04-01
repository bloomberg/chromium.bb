// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CLIENTS_CRW_PASSKIT_NETWORK_CLIENT_H_
#define IOS_WEB_NET_CLIENTS_CRW_PASSKIT_NETWORK_CLIENT_H_

#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"
#import "ios/net/clients/crn_forwarding_network_client.h"

namespace net {
class RequestTracker;
}

@protocol CRWPassKitDelegate;

// The CRWPassKitNetworkClient intercepts data from the network stack, caches it
// as it is read from the network, and on completion asks the delegate to
// handle the complete object.
// The CRWPassKitNetworkClient is inserted into the HttpProtocolHandlerCore's
// clients list when it detects that a PassKit object is being downloaded.
@interface CRWPassKitNetworkClient : CRNForwardingNetworkClient

// Designated initializer. This class assumes that |delegate| in not nil and
// will remain alive for the duration of the request this client is handling.
- (instancetype)initWithTracker:(base::WeakPtr<net::RequestTracker>)tracker
                       delegate:(id<CRWPassKitDelegate>)delegate;

@end

#endif  // IOS_WEB_NET_CLIENTS_CRW_PASSKIT_NETWORK_CLIENT_H_
