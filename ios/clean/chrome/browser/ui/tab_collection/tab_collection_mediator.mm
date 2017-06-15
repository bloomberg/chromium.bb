// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_consumer.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_item.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabCollectionMediator ()<CRWWebStateObserver>
@end

@implementation TabCollectionMediator {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserverBridge>>
      _scopedWebStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
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
  _webStateList = nullptr;
  _webStateObserver.reset();
  _scopedWebStateListObserver->RemoveAll();
}

#pragma mark - Properties

- (void)setWebStateList:(WebStateList*)webStateList {
  DCHECK(webStateList);
  _scopedWebStateListObserver->RemoveAll();
  _webStateList = webStateList;
  _scopedWebStateListObserver->Add(_webStateList);
  _webStateObserver = base::MakeUnique<web::WebStateObserverBridge>(
      self.webStateList->GetActiveWebState(), self);
  [self populateConsumerItems];
}

- (void)setConsumer:(id<TabCollectionConsumer>)consumer {
  DCHECK(consumer);
  _consumer = consumer;
  [self populateConsumerItems];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index {
  DCHECK(self.consumer);
  [self.consumer insertItem:[self tabCollectionItemFromWebState:webState]
                    atIndex:index
              selectedIndex:webStateList->active_index()];
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  DCHECK(self.consumer);
  [self.consumer moveItemFromIndex:fromIndex
                           toIndex:toIndex
                     selectedIndex:webStateList->active_index()];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  DCHECK(self.consumer);
  [self.consumer
      replaceItemAtIndex:index
                withItem:[self tabCollectionItemFromWebState:newWebState]];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  DCHECK(self.consumer);
  [self.consumer deleteItemAtIndex:index
                     selectedIndex:webStateList->active_index()];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  DCHECK(self.consumer);
  [self.consumer setSelectedIndex:atIndex];
  _webStateObserver =
      base::MakeUnique<web::WebStateObserverBridge>(newWebState, self);
}

#pragma mark - CRWWebStateObserver

// Navigational changes to the web state update the tab collection, such as
// the title and snapshot.
- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK(self.webStateList);
  DCHECK(self.consumer);
  int index = self.webStateList->GetIndexOfWebState(webState);
  [self.consumer
      replaceItemAtIndex:index
                withItem:[self tabCollectionItemFromWebState:webState]];
}

#pragma mark - Private

// Constructs a TabCollectionItem from a |webState|.
- (TabCollectionItem*)tabCollectionItemFromWebState:
    (const web::WebState*)webState {
  // PLACEHOLDER: Use real webstate title in the future.
  DCHECK(webState);
  GURL url = webState->GetVisibleURL();
  NSString* urlText = @"<New Tab>";
  if (url.is_valid()) {
    urlText = base::SysUTF8ToNSString(url.spec());
  }
  TabCollectionItem* item = [[TabCollectionItem alloc] init];
  item.title = urlText;
  return item;
}

// Constructs an array of TabCollectionItems from a |webStateList|.
- (NSArray<TabCollectionItem*>*)tabCollectionItemsFromWebStateList:
    (const WebStateList*)webStateList {
  DCHECK(webStateList);
  NSMutableArray<TabCollectionItem*>* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    [items addObject:[self tabCollectionItemFromWebState:webState]];
  }
  return [items copy];
}

// Constructs an array of TabCollectionItems from the current webStateList
// and pushes them to the consumer.
- (void)populateConsumerItems {
  if (self.consumer && self.webStateList) {
    [self.consumer
        populateItems:[self
                          tabCollectionItemsFromWebStateList:self.webStateList]
        selectedIndex:self.webStateList->active_index()];
  }
}

@end
