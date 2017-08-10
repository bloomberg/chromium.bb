// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_mediator.h"

#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_consumer.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridMediator
@dynamic consumer;

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [super webStateList:webStateList
      didInsertWebState:webState
                atIndex:index
             activating:activating];
  [self.consumer removeNoTabsOverlay];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [super webStateList:webStateList didDetachWebState:webState atIndex:index];
  if (webStateList->empty()) {
    [self.consumer addNoTabsOverlay];
  }
}

@end
