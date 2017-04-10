// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list_fast_enumeration_helper.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - WebStateListFastEnumerationHelperObserver

// Observer for WebStateListFastEnumerationHelper that will increment the
// mutation counter provided in the constructor every time the WebStateList
// it is tracking is modified.
@interface WebStateListFastEnumerationHelperObserver
    : NSObject<WebStateListObserving>

// Initializes the observer with a pointer to the mutation counter to increment
// when the WebStateList is mutated.
- (instancetype)initWithMutationCounter:(unsigned long*)mutationCounter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation WebStateListFastEnumerationHelperObserver {
  // Pointer to the mutation counter to increment when the WebStateList is
  // mutated.
  unsigned long* _mutationCounter;
}

- (instancetype)initWithMutationCounter:(unsigned long*)mutationCounter {
  DCHECK(mutationCounter);
  if ((self = [super init]))
    _mutationCounter = mutationCounter;
  return self;
}

#pragma mark WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index {
  ++*_mutationCounter;
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  ++*_mutationCounter;
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
            byWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  ++*_mutationCounter;
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  ++*_mutationCounter;
}

@end

#pragma mark - WebStateListFastEnumerationHelper

@implementation WebStateListFastEnumerationHelper {
  // The wrapped WebStateList.
  WebStateList* _webStateList;

  // Helper that returns Objective-C proxies for WebState objects.
  id<WebStateProxyFactory> _proxyFactory;

  // WebStateListObserverBridge forwarding the events of WebStateList to self.
  std::unique_ptr<WebStateListObserverBridge> _observerBridge;

  // Counter incremented each time the WebStateList is mutated.
  unsigned long _mutationCounter;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                        proxyFactory:(id<WebStateProxyFactory>)proxyFactory {
  DCHECK(webStateList);
  DCHECK(proxyFactory);
  if ((self = [super init])) {
    _webStateList = webStateList;
    _proxyFactory = proxyFactory;
    _observerBridge = base::MakeUnique<WebStateListObserverBridge>(
        [[WebStateListFastEnumerationHelperObserver alloc]
            initWithMutationCounter:&_mutationCounter]);
    _webStateList->AddObserver(_observerBridge.get());
  }
  return self;
}

- (void)dealloc {
  _webStateList->RemoveObserver(_observerBridge.get());
}

#pragma mark NSFastEnumeration

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id __unsafe_unretained*)buffer
                                    count:(NSUInteger)len {
  // The number of objects already returned is stored in |state->state|. For
  // the first iteration, the counter will be zero and the rest of the state
  // structure need to be initialised.
  DCHECK_LE(state->state, static_cast<unsigned long>(INT_MAX));
  const int offset = state->state;
  if (offset == 0)
    state->mutationsPtr = &_mutationCounter;

  if (len > static_cast<unsigned long>(INT_MAX))
    len = static_cast<unsigned long>(INT_MAX);

  DCHECK_LE(offset, _webStateList->count());
  const int count =
      std::min(static_cast<int>(len), _webStateList->count() - offset);

  for (int index = 0; index < count; ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(offset + index);
    __autoreleasing id wrapper = [_proxyFactory proxyForWebState:webState];
    buffer[index] = wrapper;
  }

  state->state += count;
  state->itemsPtr = buffer;

  return static_cast<NSUInteger>(count);
}

@end
