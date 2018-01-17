// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_coordinator.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_coordinator+protected.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AdaptiveToolbarCoordinator ()

// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;

@end

@implementation AdaptiveToolbarCoordinator
@synthesize dispatcher = _dispatcher;
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
}

#pragma mark - ToolbarCoordinating

- (void)updateToolbarState {
  // TODO(crbug.com/801082): Implement that.
}

- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
  // TODO(crbug.com/801082): Implement that.
}

#pragma mark - ToolbarCommands

- (void)contractToolbar {
  // TODO(crbug.com/801082): Implement that.
}

- (void)triggerToolsMenuButtonAnimation {
  // TODO(crbug.com/801083): Implement that.
}

- (void)navigateToMemexTabSwitcher {
  // TODO(crbug.com/799601): Delete this once it is not needed.
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
