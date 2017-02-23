// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_navigation_states.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Holds a pair of state and creation order index.
@interface CRWWKNavigationsStateRecord : NSObject
// Navigation state.
@property(nonatomic, assign) web::WKNavigationState state;
// Numerical index representing creation order (smaller index denotes earlier
// navigations).
@property(nonatomic, assign, readonly) NSUInteger index;

- (instancetype)init NS_UNAVAILABLE;

// Initializes record with state and index values.
- (instancetype)initWithState:(web::WKNavigationState)state
                        index:(NSUInteger)index NS_DESIGNATED_INITIALIZER;

@end

@implementation CRWWKNavigationsStateRecord
@synthesize state = _state;
@synthesize index = _index;

- (NSString*)description {
  return [NSString stringWithFormat:@"state: %d, index: %ld", _state,
                                    static_cast<long>(_index)];
}

- (instancetype)initWithState:(web::WKNavigationState)state
                        index:(NSUInteger)index {
  if ((self = [super init])) {
    _state = state;
    _index = index;
  }
  return self;
}

@end

@interface CRWWKNavigationStates () {
  NSMapTable* _records;
  NSUInteger _lastStateIndex;
}
@end

@implementation CRWWKNavigationStates

- (instancetype)init {
  if ((self = [super init])) {
    _records = [NSMapTable weakToStrongObjectsMapTable];
  }
  return self;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"records: %@, lastAddedNavigation: %@",
                                    _records, self.lastAddedNavigation];
}

- (void)setState:(web::WKNavigationState)state
    forNavigation:(WKNavigation*)navigation {
  if (!navigation) {
    // WKWebView may call WKNavigationDelegate callbacks with nil.
    return;
  }

  CRWWKNavigationsStateRecord* record = [_records objectForKey:navigation];
  if (!record) {
    DCHECK(state == web::WKNavigationState::REQUESTED ||
           state == web::WKNavigationState::STARTED ||
           state == web::WKNavigationState::COMMITTED);
    record =
        [[CRWWKNavigationsStateRecord alloc] initWithState:state
                                                     index:++_lastStateIndex];
  } else {
    DCHECK(
        record.state < state ||
        (record.state == state && state == web::WKNavigationState::REDIRECTED));
    record.state = state;
  }
  [_records setObject:record forKey:navigation];
}

- (WKNavigation*)lastAddedNavigation {
  WKNavigation* result = nil;
  NSUInteger lastAddedIndex = 0;  // record indices start with 1.
  for (WKNavigation* navigation in _records) {
    CRWWKNavigationsStateRecord* record = [_records objectForKey:navigation];
    if (lastAddedIndex < record.index) {
      result = navigation;
      lastAddedIndex = record.index;
    }
  }
  return result;
}

- (web::WKNavigationState)lastAddedNavigationState {
  CRWWKNavigationsStateRecord* lastAddedRecord = nil;
  WKNavigation* lastAddedNavigation = [self lastAddedNavigation];
  if (lastAddedNavigation)
    lastAddedRecord = [_records objectForKey:lastAddedNavigation];

  return lastAddedRecord.state;
}

@end
