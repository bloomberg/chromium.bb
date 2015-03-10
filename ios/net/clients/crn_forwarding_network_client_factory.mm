// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/clients/crn_forwarding_network_client_factory.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#import "ios/net/clients/crn_forwarding_network_client.h"

@implementation CRNForwardingNetworkClientFactory

// Init just sanity checks that the factory class has sane ordering rules.
- (instancetype)init {
  if ((self = [super init])) {
    // Type checks.
    DCHECK(![[self class] mustApplyBefore] ||
           [[[self class] mustApplyBefore]
               isSubclassOfClass:[CRNForwardingNetworkClientFactory class]]);
    DCHECK(![[self class] mustApplyAfter] ||
           [[[self class] mustApplyAfter]
               isSubclassOfClass:[CRNForwardingNetworkClientFactory class]]);
    // Order check.
    DCHECK([[self class] orderedOK]);
  }
  return self;
}

- (CRNForwardingNetworkClient*)clientHandlingAnyRequest {
  return nil;
}

- (CRNForwardingNetworkClient*)
    clientHandlingRequest:(const net::URLRequest&)request {
   return nil;
}

- (CRNForwardingNetworkClient*)
    clientHandlingResponse:(NSURLResponse*)response
                   request:(const net::URLRequest&)request {
  return nil;
}

- (CRNForwardingNetworkClient*)
    clientHandlingRedirect:(const net::URLRequest&)request
                       url:(const GURL&)url
                  response:(NSURLResponse*)response {
  return nil;
}

- (Class)clientClass {
  return nil;
}

+ (Class)mustApplyBefore {
  return nil;
}

+ (Class)mustApplyAfter {
  return nil;
}

- (BOOL)requiresOrdering {
  return [[self class] mustApplyAfter] || [[self class] mustApplyBefore];
}

// Verify that the ordering is sensible. If the calling class encounters itself
// walking dependencies in either direction, it's broken. If any of the classes
// it must appear after are also classes it must appear before, it's broken.
+ (BOOL)orderedOK {
  Class precursor = [self class];
  NSMutableArray* precursors = [NSMutableArray array];
  while ((precursor = [precursor mustApplyAfter])) {
    if (precursor == [self class])
      return NO;
    [precursors addObject:precursor];
  }
  id successor = [self class];
  while ((successor = [successor mustApplyBefore])) {
    if (successor == [self class])
      return NO;
    if ([precursors indexOfObject:successor] != NSNotFound)
      return NO;
  }
  return YES;
}

- (NSComparisonResult)
    orderRelativeTo:(CRNForwardingNetworkClientFactory*)factory {
  NSComparisonResult result = NSOrderedSame;
  if ([[self class] mustApplyBefore] == [factory class] ||
      [[factory class] mustApplyAfter] == [self class]) {
    result = NSOrderedAscending;
  }
  if ([[self class] mustApplyAfter] == [factory class] ||
      [[factory class] mustApplyBefore] == [self class]) {
    result = NSOrderedDescending;
  }
  return result;
}

@end
