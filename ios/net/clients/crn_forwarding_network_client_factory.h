// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_CLIENTS_CRN_FORWARDING_NETWORK_CLIENT_FACTORY_H_
#define IOS_NET_CLIENTS_CRN_FORWARDING_NETWORK_CLIENT_FACTORY_H_

#import <Foundation/Foundation.h>

@class CRNForwardingNetworkClient;
class GURL;

namespace net {
class URLRequest;
}

// Abstract factory that creates CRNForwardingNetworkClient instances.
@interface CRNForwardingNetworkClientFactory : NSObject

// Any subclass of CRNForwardingNetworkClientFactory should implement exactly
// one of the clientHandling... methods. Each method either returns an network
// client, or nil if the request in question isn't eligible for handling
// by the factory's clients. A unit test verifies that all subclasses override
// one of these methods.

// The expectation is that subclasses of this class will not be derived from;
// if this becomes necessary, the unit tests for this class should be updated.

// Return a client to handle any request.
// Factories should implement this method only if their clients need to be able
// to handle didFailWithError: calls that might originate before the native
// request is generated
- (CRNForwardingNetworkClient*)clientHandlingAnyRequest;

// Return a client to handle |request|.
// Factories should implement this method if their clients need to operate on
// the request before it is started (for example, by modifying headers).
- (CRNForwardingNetworkClient*)
    clientHandlingRequest:(const net::URLRequest&)request;

// Return a client to handle |request| and |response|.
// Factories should implement this method if their clients need to operate on
// non-redirected responses.
- (CRNForwardingNetworkClient*)
    clientHandlingResponse:(NSURLResponse*)response
                   request:(const net::URLRequest&)request;

// Returns a client to handle a redirect of |request| to |url| in response to
// |response|.
// Factories should only implement this method if their clients need to operate
// on redirects.
- (CRNForwardingNetworkClient*)
    clientHandlingRedirect:(const net::URLRequest&)request
                       url:(const GURL&)url
                  response:(NSURLResponse*)response;

// Subclasses must implement this method.
// Only used in debug, to check that the ordering is consistent.
- (Class)clientClass;

// Class of another network client that this factory client class must be
// ordered before or after. Default is nil.
+ (Class)mustApplyBefore;
+ (Class)mustApplyAfter;

// YES if this factory may require an array of factories to be ordered.
- (BOOL)requiresOrdering;

// YES if the mustApply rules on this class aren't cyclic. Exposed for testing.
+ (BOOL)orderedOK;

// Comparison function to sort a list of factories.
- (NSComparisonResult)orderRelativeTo:
    (CRNForwardingNetworkClientFactory*)factory;

@end

#endif  // IOS_NET_CLIENTS_CRN_FORWARDING_NETWORK_CLIENT_FACTORY_H_
