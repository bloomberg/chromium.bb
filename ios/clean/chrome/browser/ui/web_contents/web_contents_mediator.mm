// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/web_contents/web_contents_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/sad_tab/sad_tab_view.h"
#import "ios/chrome/browser/web/sad_tab_tab_helper.h"
#import "ios/clean/chrome/browser/ui/web_contents/web_contents_consumer.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/ui/crw_generic_content_view.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebContentsMediator
@synthesize webState = _webState;
@synthesize consumer = _consumer;
@synthesize dispatcher = _dispatcher;

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  web::WebState* replacedWebState = _webState;
  _webState = webState;
  [self updateConsumerWithWebState:webState];
  if (self.consumer) {
    if (replacedWebState)
      replacedWebState->WasHidden();
    if (_webState)
      _webState->WasShown();
  }
}

- (void)setConsumer:(id<WebContentsConsumer>)consumer {
  _consumer = consumer;
  if (self.webState) {
    if (_consumer) {
      [self updateConsumerWithWebState:self.webState];
      self.webState->WasShown();
    } else {
      self.webState->WasHidden();
    }
  }
}

#pragma mark - Private

// Updates the consumer's contentView.
- (void)updateConsumerWithWebState:(web::WebState*)webState {
  UIView* updatedView = nil;
  if (webState) {
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

#pragma mark - SadTabTabHelperDelegate

- (void)sadTabTabHelper:(SadTabTabHelper*)tabHelper
    presentSadTabForRepeatedFailure:(BOOL)repeatedFailure {
  // Create a SadTabView so |webstate| presents it.
  SadTabView* sadTabview = [[SadTabView alloc]
           initWithMode:repeatedFailure ? SadTabViewMode::FEEDBACK
                                        : SadTabViewMode::RELOAD
      navigationManager:tabHelper->web_state()->GetNavigationManager()];
  sadTabview.dispatcher = self.dispatcher;
  CRWContentView* contentView =
      [[CRWGenericContentView alloc] initWithView:sadTabview];
  tabHelper->web_state()->ShowTransientContentView(contentView);
}

@end
