// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/omnibox/location_bar_coordinator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"
#import "ios/shared/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/shared/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<ToolsMenuCommands>
@property(nonatomic, weak) LocationBarCoordinator* locationBarCoordinator;
@property(nonatomic, weak) ToolsCoordinator* toolsMenuCoordinator;
@property(nonatomic, strong) ToolbarViewController* viewController;
@property(nonatomic, strong) ToolbarMediator* mediator;
@end

@implementation ToolbarCoordinator
@synthesize locationBarCoordinator = _locationBarCoordinator;
@synthesize toolsMenuCoordinator = _toolsMenuCoordinator;
@synthesize viewController = _viewController;
@synthesize webState = _webState;
@synthesize mediator = _mediator;

- (instancetype)init {
  if ((self = [super init])) {
    _mediator = [[ToolbarMediator alloc] init];
  }
  return self;
}

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  self.mediator.webState = self.webState;
}

- (void)start {
  self.viewController = [[ToolbarViewController alloc]
      initWithDispatcher:static_cast<id>(self.browser->dispatcher())];

  CommandDispatcher* dispatcher = self.browser->dispatcher();
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showToolsMenu)];
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(closeToolsMenu)];
  [dispatcher startDispatchingToTarget:self forSelector:@selector(goBack)];
  [dispatcher startDispatchingToTarget:self forSelector:@selector(goForward)];
  [dispatcher startDispatchingToTarget:self forSelector:@selector(reloadPage)];
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(stopLoadingPage)];

  self.mediator.consumer = self.viewController;
  self.mediator.webStateList = &self.browser->web_state_list();

  [self.browser->broadcaster()
      addObserver:self.mediator
      forSelector:@selector(broadcastTabStripVisible:)];
  LocationBarCoordinator* locationBarCoordinator =
      [[LocationBarCoordinator alloc] init];
  self.locationBarCoordinator = locationBarCoordinator;
  [self addChildCoordinator:locationBarCoordinator];
  [locationBarCoordinator start];

  [super start];
}

- (void)stop {
  [super stop];
  [self.browser->broadcaster()
      removeObserver:self.mediator
         forSelector:@selector(broadcastTabStripVisible:)];
  [self.browser->dispatcher() stopDispatchingToTarget:self];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  if ([childCoordinator isKindOfClass:[LocationBarCoordinator class]]) {
    self.viewController.locationBarViewController =
        self.locationBarCoordinator.viewController;
  } else if ([childCoordinator isKindOfClass:[ToolsCoordinator class]]) {
    [self.viewController presentViewController:childCoordinator.viewController
                                      animated:YES
                                    completion:nil];
  }
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  if ([childCoordinator isKindOfClass:[ToolsCoordinator class]]) {
    [childCoordinator.viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
}

#pragma mark - ToolsMenuCommands Implementation

- (void)showToolsMenu {
  ToolsCoordinator* toolsCoordinator = [[ToolsCoordinator alloc] init];
  [self addChildCoordinator:toolsCoordinator];
  ToolsMenuConfiguration* menuConfiguration =
      [[ToolsMenuConfiguration alloc] initWithDisplayView:nil];
  menuConfiguration.inTabSwitcher = NO;
  toolsCoordinator.toolsMenuConfiguration = menuConfiguration;
  toolsCoordinator.webState = self.webState;
  [toolsCoordinator start];
  self.toolsMenuCoordinator = toolsCoordinator;
}

- (void)closeToolsMenu {
  [self.toolsMenuCoordinator stop];
  [self removeChildCoordinator:self.toolsMenuCoordinator];
}

// PLACEHOLDER: These will move to an object that handles navigation.
#pragma mark - NavigationCommands

- (void)goBack {
  if (self.webState->GetNavigationManager()->CanGoBack()) {
    self.webState->GetNavigationManager()->GoBack();
  }
}

- (void)goForward {
  if (self.webState->GetNavigationManager()->CanGoForward()) {
    self.webState->GetNavigationManager()->GoForward();
  }
}

- (void)stopLoadingPage {
  self.webState->Stop();
}

- (void)reloadPage {
  self.webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                                false /*check_for_repost*/);
}

@end
