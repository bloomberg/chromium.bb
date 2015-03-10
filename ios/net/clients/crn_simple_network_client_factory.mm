// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/clients/crn_simple_network_client_factory.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/net/clients/crn_forwarding_network_client.h"

@interface CRNSimpleNetworkClientFactory () {
  base::scoped_nsprotocol<Class> _clientClass;
}
@end

@implementation CRNSimpleNetworkClientFactory

- (instancetype)initWithClass:(Class)clientClass {
  if (self = [super init]) {
    DCHECK([clientClass isSubclassOfClass:[CRNForwardingNetworkClient class]]);
    _clientClass.reset([clientClass retain]);
  }
  return self;
}

- (CRNForwardingNetworkClient*)clientHandlingAnyRequest {
  return [[[_clientClass alloc] init] autorelease];
}

- (Class)clientClass {
  return _clientClass;
}

@end
