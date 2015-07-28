// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/weak_nsobject_counter.h"

#import <objc/runtime.h>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"

// Used for observing the objects tracked in the WeakNSObjectCounter. This
// object will be dealloced when the tracked object is dealloced and will
// notify the shared counter.
@interface CRBWeakNSObjectDeallocationObserver : NSObject
// Designated initializer. |object| cannot be nil. It registers self as an
// associated object to |object|.
- (instancetype)initWithSharedCounter:(const linked_ptr<NSUInteger>&)counter
                   objectToBeObserved:(id)object;
@end

@implementation CRBWeakNSObjectDeallocationObserver {
  linked_ptr<NSUInteger> _counter;
}

- (instancetype)initWithSharedCounter:(const linked_ptr<NSUInteger>&)counter
                   objectToBeObserved:(id)object {
  self = [super init];
  if (self) {
    DCHECK(counter.get());
    DCHECK(object);
    _counter = counter;
    objc_setAssociatedObject(
        object,
        reinterpret_cast<const void*>(_counter.get()),  // The key.
        self, OBJC_ASSOCIATION_RETAIN);
    (*_counter)++;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  DCHECK(_counter.get());
  (*_counter)--;
  _counter.reset();
  [super dealloc];
}

@end

namespace web {

WeakNSObjectCounter::WeakNSObjectCounter() : counter_(new NSUInteger(0)) {
}

WeakNSObjectCounter::~WeakNSObjectCounter() {
  DCHECK(CalledOnValidThread());
}

void WeakNSObjectCounter::Insert(id object) {
  DCHECK(CalledOnValidThread());
  DCHECK(object);
  // Create an associated object and register it with |object|.
  base::scoped_nsobject<CRBWeakNSObjectDeallocationObserver> observingObject(
      [[CRBWeakNSObjectDeallocationObserver alloc]
          initWithSharedCounter:counter_ objectToBeObserved:object]);
}

NSUInteger WeakNSObjectCounter::Size() const {
  DCHECK(CalledOnValidThread());
  return *counter_;
}

}  // namespace web
