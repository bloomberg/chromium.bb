// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebStateList {
  NSMutableArray<id<WebStateHandle>>* _list;
}

#pragma mark - Lifecycle

- (instancetype)init {
  if ((self = [super init])) {
    _list = [[NSMutableArray alloc] init];
  }
  return self;
}

#pragma mark - NSFastEnumeration

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id __unsafe_unretained*)buffer
                                    count:(NSUInteger)len {
  return [_list countByEnumeratingWithState:state objects:buffer count:len];
}

#pragma mark - Properties

- (NSUInteger)count {
  return _list.count;
}

- (id<WebStateHandle>)firstWebState {
  return _list.firstObject;
}

#pragma mark - Public methods, mutation

- (void)addWebState:(id<WebStateHandle>)webState {
  [_list addObject:webState];
}

- (void)insertWebState:(id<WebStateHandle>)webState atIndex:(NSUInteger)index {
  [_list insertObject:webState atIndex:index];
}

- (void)replaceWebStateAtIndex:(NSUInteger)index
                  withWebState:(id<WebStateHandle>)webState {
  [_list replaceObjectAtIndex:index withObject:webState];
}

- (void)removeWebState:(id<WebStateHandle>)webState {
  [_list removeObject:webState];
}

#pragma mark - Public methods, queries

- (BOOL)containsWebState:(id<WebStateHandle>)webState {
  return [_list containsObject:webState];
}

- (id<WebStateHandle>)webStateAtIndex:(NSUInteger)index {
  DCHECK_LT(index, _list.count);
  return [_list objectAtIndex:index];
}

- (NSUInteger)indexOfWebState:(id<WebStateHandle>)webState {
  return [_list indexOfObject:webState];
}

@end
