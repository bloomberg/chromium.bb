// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarMediator ()<CRWWebStateObserver, WebStateListObserving>
@end

@implementation ToolbarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserverBridge>>
      _scopedWebStateListObserver;
}

@synthesize consumer = _consumer;
@synthesize webState = _webState;
@synthesize webStateList = _webStateList;

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)disconnect {
  self.webStateList = nullptr;
  _webStateObserver.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self updateNavigationBackAndForwardState];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  [self updateNavigationBackAndForwardState];
}

- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count {
  [self updateNavigationBackAndForwardState];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  [self.consumer setLoadingProgressFraction:progress];
}

#pragma mark - ChromeBroadcastObserver

- (void)broadcastTabStripVisible:(BOOL)visible {
  [self.consumer setTabStripVisible:visible];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self.consumer setTabCount:_webStateList->count()];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self.consumer setTabCount:_webStateList->count()];
}

#pragma mark - Setters

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  _webStateObserver =
      base::MakeUnique<web::WebStateObserverBridge>(_webState, self);
  if (self.consumer) {
    [self updateConsumer];
  }
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  _consumer = consumer;
  if (self.webState) {
    [self updateConsumer];
  }
  if (self.webStateList) {
    [self.consumer setTabCount:_webStateList->count()];
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  // TODO(crbug.com/727427):Add support for DCHECK(webStateList).
  _webStateList = webStateList;
  _webStateListObserver = base::MakeUnique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver = base::MakeUnique<
      ScopedObserver<WebStateList, WebStateListObserverBridge>>(
      _webStateListObserver.get());
  if (_webStateList) {
    _scopedWebStateListObserver->Add(_webStateList);
    if (self.consumer) {
      [self.consumer setTabCount:_webStateList->count()];
    }
  }
}

#pragma mark - Helper methods

// Updates the consumer to match the current WebState.
- (void)updateConsumer {
  DCHECK(self.webState);
  DCHECK(self.consumer);
  [self updateNavigationBackAndForwardState];
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

// Updates the consumer with the new forward and back states.
- (void)updateNavigationBackAndForwardState {
  [self.consumer
      setCanGoForward:self.webState->GetNavigationManager()->CanGoForward()];
  [self.consumer
      setCanGoBack:self.webState->GetNavigationManager()->CanGoBack()];
}

@end
