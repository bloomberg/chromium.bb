// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_consumer.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_observer_bridge.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabGridMediator ()<WebStateListObserving>
@end

@implementation TabGridMediator {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

@synthesize webStateList = _webStateList;
@synthesize consumer = _consumer;

- (void)setWebStateList:(WebStateList*)webStateList {
  _webStateList = webStateList;
  _webStateListObserver = base::MakeUnique<WebStateListObserverBridge>(self);
  _webStateList->AddObserver(_webStateListObserver.get());
}

#pragma mark - TabGridDataSource

- (int)numberOfTabsInTabGrid {
  return self.webStateList->count();
}

- (NSString*)titleAtIndex:(int)index {
  return [self titleFromWebState:self.webStateList->GetWebStateAt(index)];
}

- (int)indexOfActiveTab {
  return self.webStateList->active_index();
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self.consumer insertTabGridItemAtIndex:index];
  [self.consumer removeNoTabsOverlay];
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  NSMutableIndexSet* indexes = [[NSMutableIndexSet alloc] init];
  [indexes addIndex:fromIndex];
  [indexes addIndex:toIndex];
  [self.consumer reloadTabGridItemsAtIndexes:indexes];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self.consumer
      reloadTabGridItemsAtIndexes:[NSIndexSet indexSetWithIndex:index]];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self.consumer deleteTabGridItemAtIndex:index];
  if (webStateList->empty()) {
    [self.consumer addNoTabsOverlay];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  int fromIndex = webStateList->GetIndexOfWebState(oldWebState);
  NSMutableIndexSet* indexes = [[NSMutableIndexSet alloc] init];
  if (fromIndex >= 0 && fromIndex < [self numberOfTabsInTabGrid]) {
    [indexes addIndex:fromIndex];
  }
  if (atIndex >= 0 && atIndex < [self numberOfTabsInTabGrid]) {
    [indexes addIndex:atIndex];
  }
  [self.consumer reloadTabGridItemsAtIndexes:indexes];
}

#pragma mark - Private

- (NSString*)titleFromWebState:(const web::WebState*)webState {
  // PLACEHOLDER: Use real webstate title in the future.
  GURL url = webState->GetVisibleURL();
  NSString* urlText = @"<New Tab>";
  if (url.is_valid()) {
    urlText = base::SysUTF8ToNSString(url.spec());
  }
  return urlText;
}

@end
