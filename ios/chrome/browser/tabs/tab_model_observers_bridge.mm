// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_observers_bridge.h"

#include "base/logging.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_model_observers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabModelObserversBridge {
  // The TabModel owning self.
  __weak TabModel* _tabModel;

  // The TabModelObservers that forward events to TabModelObserver instances
  // registered with owning TabModel.
  __weak TabModelObservers* _tabModelObservers;
}

- (instancetype)initWithTabModel:(TabModel*)tabModel
               tabModelObservers:(TabModelObservers*)tabModelObservers {
  DCHECK(tabModel);
  DCHECK(tabModelObservers);
  if ((self = [super init])) {
    _tabModel = tabModel;
    _tabModelObservers = tabModelObservers;
  }
  return self;
}

#pragma mark WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_GE(atIndex, 0);
  [_tabModelObservers tabModel:_tabModel
                 didReplaceTab:LegacyTabHelper::GetTabForWebState(oldWebState)
                       withTab:LegacyTabHelper::GetTabForWebState(newWebState)
                       atIndex:static_cast<NSUInteger>(atIndex)];
}

@end
