// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarMediator ()<CRWWebStateObserver>
@end

@implementation ToolbarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

@synthesize consumer = _consumer;
@synthesize webState = _webState;

- (void)dealloc {
  _webStateObserver.reset();
  _webState = nullptr;
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self.consumer
      setCanGoBack:self.webState->GetNavigationManager()->CanGoBack()];
  [self.consumer
      setCanGoForward:self.webState->GetNavigationManager()->CanGoForward()];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  [self.consumer setLoadingProgress:progress];
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
}

#pragma mark - Helper methods

// Updates the consumer to match the current WebState.
- (void)updateConsumer {
  DCHECK(self.webState);
  DCHECK(self.consumer);
  [self.consumer
      setCanGoForward:self.webState->GetNavigationManager()->CanGoForward()];
  [self.consumer
      setCanGoBack:self.webState->GetNavigationManager()->CanGoBack()];
  [self.consumer setIsLoading:self.webState->IsLoading()];
}

@end
