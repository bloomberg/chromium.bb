// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_fast_enumeration_helper.h"

#include <algorithm>
#include <cstdint>
#include <memory>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebStateListFastEnumeration
    : NSObject<NSFastEnumeration, WebStateListObserving>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                        proxyFactory:(id<WebStateProxyFactory>)proxyFactory;

- (void)shutdown;

@end

@implementation WebStateListFastEnumeration {
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
    _observerBridge = base::MakeUnique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_observerBridge.get());
  }
  return self;
}

- (void)shutdown {
  _webStateList->RemoveObserver(_observerBridge.get());
  _webStateList = nullptr;
  ++_mutationCounter;
}

- (void)dealloc {
  DCHECK(!_webStateList) << "-shutdown must be called before -dealloc";
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

  if (!_webStateList)
    return 0;

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

#pragma mark WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  ++_mutationCounter;
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  ++_mutationCounter;
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
            byWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  ++_mutationCounter;
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  ++_mutationCounter;
}

@end

WebStateListFastEnumerationHelper::WebStateListFastEnumerationHelper(
    WebStateList* web_state_list,
    id<WebStateProxyFactory> proxy_factory)
    : fast_enumeration_([[WebStateListFastEnumeration alloc]
          initWithWebStateList:web_state_list
                  proxyFactory:proxy_factory]) {}

WebStateListFastEnumerationHelper::~WebStateListFastEnumerationHelper() {
  WebStateListFastEnumeration* fast_enumeration =
      base::mac::ObjCCastStrict<WebStateListFastEnumeration>(
          fast_enumeration_.get());
  [fast_enumeration shutdown];
  fast_enumeration_.reset();
}

id<NSFastEnumeration> WebStateListFastEnumerationHelper::GetFastEnumeration() {
  return fast_enumeration_.get();
}
