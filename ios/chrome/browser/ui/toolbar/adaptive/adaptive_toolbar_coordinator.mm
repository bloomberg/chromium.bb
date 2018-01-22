// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_coordinator.h"

#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AdaptiveToolbarCoordinator ()

// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// Mediator for updating the toolbar when the WebState changes.
@property(nonatomic, strong) ToolbarMediator* mediator;

@end

@implementation AdaptiveToolbarCoordinator
@synthesize dispatcher = _dispatcher;
@synthesize mediator = _mediator;
@synthesize started = _started;
@synthesize viewController = _viewController;
@synthesize webStateList = _webStateList;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  return [super initWithBaseViewController:nil browserState:browserState];
}

- (void)start {
  if (self.started)
    return;

  self.started = YES;

  self.viewController.dispatcher = self.dispatcher;

  self.mediator = [[ToolbarMediator alloc] init];
  self.mediator.voiceSearchProvider =
      ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
  self.mediator.consumer = self.viewController;
  self.mediator.webStateList = self.webStateList;
  self.mediator.bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(self.browserState);
}

#pragma mark - ToolbarCoordinating

- (void)updateToolbarState {
  // TODO(crbug.com/803386): This call is needed for interstitials. Check if it
  // is possible to remove it.
}

- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
  // TODO(crbug.com/803379): Implement that.
}

#pragma mark - ToolbarCommands

- (void)contractToolbar {
  // TODO(crbug.com/801082): Implement that.
}

- (void)triggerToolsMenuButtonAnimation {
  // TODO(crbug.com/801083): Implement that.
}

#pragma mark - Protected

- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type {
  BOOL isIncognito = self.browserState->IsOffTheRecord();
  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;

  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  buttonFactory.dispatcher = self.dispatcher;
  buttonFactory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];

  return buttonFactory;
}

@end
