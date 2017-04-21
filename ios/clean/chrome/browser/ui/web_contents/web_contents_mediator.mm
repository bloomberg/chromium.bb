// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_consumer.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WebContentsMediator ()<WebStateListObserving>
@end

@implementation WebContentsMediator {
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
  if (!self.webStateList) {
    return;
  }
  [self disableWebUsage:self.webStateList->GetActiveWebState()];
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
  if (_webStateList->GetActiveWebState()) {
    [self updateConsumerWithWebState:_webStateList->GetActiveWebState()];
  }
}

- (void)setConsumer:(id<WebContentsConsumer>)consumer {
  _consumer = consumer;
  if (self.webStateList && self.webStateList->GetActiveWebState()) {
    [self updateConsumerWithWebState:self.webStateList->GetActiveWebState()];
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  [self disableWebUsage:oldWebState];
  [self updateConsumerWithWebState:newWebState];
}

#pragma mark - Private

- (void)disableWebUsage:(web::WebState*)webState {
  if (webState) {
    webState->SetWebUsageEnabled(false);
  }
}

// Sets |webState| webUsageEnabled and updates the consumer's contentView.
- (void)updateConsumerWithWebState:(web::WebState*)webState {
  UIView* updatedView = nil;
  if (webState) {
    webState->SetWebUsageEnabled(true);
    updatedView = webState->GetView();
    // PLACEHOLDER: This navigates the page since the omnibox is not yet
    // hooked up.
    [self navigateToDefaultPage:webState];
  }
  if (self.consumer) {
    [self.consumer contentViewDidChange:updatedView];
  }
}

// PLACEHOLDER: This navigates an empty webstate to the NTP.
- (void)navigateToDefaultPage:(web::WebState*)webState {
  if (!webState->GetNavigationManager()->GetItemCount()) {
    web::NavigationManager::WebLoadParams params((GURL(kChromeUINewTabURL)));
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    webState->GetNavigationManager()->LoadURLWithParams(params);
  }
}

@end
