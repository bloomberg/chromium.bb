// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_consumer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
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
}

@synthesize consumer = _consumer;
@synthesize webState = _webState;
@synthesize webStateList = _webStateList;

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)updateConsumerForWebState:(web::WebState*)webState {
  [self updateNavigationBackAndForwardStateForWebState:webState];
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updateConsumerForWebState:self.webState];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateConsumerForWebState:self.webState];
}

- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count {
  DCHECK_EQ(_webState, webState);
  [self updateConsumerForWebState:self.webState];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self.consumer setLoadingProgressFraction:progress];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webState = nullptr;
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
  DCHECK_EQ(_webStateList, webStateList);
  [self.consumer setTabCount:_webStateList->count()];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);
  [self.consumer setTabCount:_webStateList->count()];
}

#pragma mark - Setters

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    if (self.consumer) {
      [self updateConsumer];
    }
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
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  // TODO(crbug.com/727427):Add support for DCHECK(webStateList).
  _webStateList = webStateList;

  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());

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
  [self updateConsumerForWebState:self.webState];
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

// Updates the consumer with the new forward and back states.
- (void)updateNavigationBackAndForwardStateForWebState:
    (web::WebState*)webState {
  [self.consumer
      setCanGoForward:webState->GetNavigationManager()->CanGoForward()];
  [self.consumer setCanGoBack:webState->GetNavigationManager()->CanGoBack()];
}

@end
