// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_consumer.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabCollectionMediator {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserverBridge>>
      _scopedWebStateListObserver;
}

@synthesize webStateList = _webStateList;
@synthesize consumer = _consumer;

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateListObserver = base::MakeUnique<WebStateListObserverBridge>(self);
    _scopedWebStateListObserver = base::MakeUnique<
        ScopedObserver<WebStateList, WebStateListObserverBridge>>(
        _webStateListObserver.get());
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)disconnect {
  self.webStateList = nullptr;
}

#pragma mark - Properties

- (void)setWebStateList:(WebStateList*)webStateList {
  _scopedWebStateListObserver->RemoveAll();
  _webStateList = webStateList;
  if (!_webStateList) {
    return;
  }
  _scopedWebStateListObserver->Add(_webStateList);
}

#pragma mark - TabCollectionDataSource

- (int)numberOfTabs {
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
  [self.consumer insertItemAtIndex:index];
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  int minIndex = std::min(fromIndex, toIndex);
  int length = std::abs(fromIndex - toIndex) + 1;
  NSIndexSet* indexes =
      [[NSIndexSet alloc] initWithIndexesInRange:NSMakeRange(minIndex, length)];
  [self.consumer reloadItemsAtIndexes:indexes];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self.consumer reloadItemsAtIndexes:[NSIndexSet indexSetWithIndex:index]];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self.consumer deleteItemAtIndex:index];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  int fromIndex = webStateList->GetIndexOfWebState(oldWebState);
  NSMutableIndexSet* indexes = [[NSMutableIndexSet alloc] init];
  if (fromIndex >= 0 && fromIndex < [self numberOfTabs]) {
    [indexes addIndex:fromIndex];
  }
  if (atIndex >= 0 && atIndex < [self numberOfTabs]) {
    [indexes addIndex:atIndex];
  }
  [self.consumer reloadItemsAtIndexes:indexes];
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
