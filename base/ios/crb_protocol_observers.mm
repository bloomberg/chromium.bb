// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/crb_protocol_observers.h"

#include <objc/runtime.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

@interface CRBProtocolObservers ()

// Designated initializer.
- (id)initWithProtocol:(Protocol*)protocol;

@end

@implementation CRBProtocolObservers {
  base::scoped_nsobject<Protocol> _protocol;
  base::scoped_nsobject<NSHashTable> _observers;
}

+ (CRBProtocolObservers*)observersWithProtocol:(Protocol*)protocol {
  return [[[self alloc] initWithProtocol:protocol] autorelease];
}

- (id)init {
  NOTREACHED();
  return nil;
}

- (id)initWithProtocol:(Protocol*)protocol {
  self = [super init];
  if (self) {
    _protocol.reset([protocol retain]);
    _observers.reset([[NSHashTable weakObjectsHashTable] retain]);
  }
  return self;
}

- (Protocol*)protocol {
  return _protocol.get();
}

- (void)addObserver:(id)observer {
  DCHECK([observer conformsToProtocol:self.protocol]);
  [_observers addObject:observer];
}

- (void)removeObserver:(id)observer {
  [_observers removeObject:observer];
}

#pragma mark - NSObject

- (NSMethodSignature*)methodSignatureForSelector:(SEL)selector {
  NSMethodSignature* signature = [super methodSignatureForSelector:selector];
  if (signature)
    return signature;

  // Look for a required method in the protocol. protocol_getMethodDescription
  // returns a struct whose fields are null if a method for the selector was
  // not found.
  struct objc_method_description description =
      protocol_getMethodDescription(self.protocol, selector, YES, YES);
  if (description.types)
    return [NSMethodSignature signatureWithObjCTypes:description.types];

  // Look for an optional method in the protocol.
  description = protocol_getMethodDescription(self.protocol, selector, NO, YES);
  if (description.types)
    return [NSMethodSignature signatureWithObjCTypes:description.types];

  // There is neither a required nor optional method with this selector in the
  // protocol, so invoke -[NSObject doesNotRecognizeSelector:] to raise
  // NSInvalidArgumentException.
  [self doesNotRecognizeSelector:selector];
  return nil;
}

- (void)forwardInvocation:(NSInvocation*)invocation {
  SEL selector = [invocation selector];
  base::scoped_nsobject<NSArray> observers([[_observers allObjects] retain]);
  for (id observer in observers.get()) {
    if ([observer respondsToSelector:selector])
      [invocation invokeWithTarget:observer];
  }
}

- (void)executeOnObservers:(ExecutionWithObserverBlock)callback {
  DCHECK(callback);
  base::scoped_nsobject<NSArray> observers([[_observers allObjects] retain]);
  for (id observer in observers.get()) {
    callback(observer);
  }
}

@end
