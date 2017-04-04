// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"
#import "ios/shared/chrome/browser/ui/browser_list/browser.h"
#import "ios/shared/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<ToolsMenuCommands>
@property(nonatomic, weak) ToolsCoordinator* toolsMenuCoordinator;
@property(nonatomic, strong) ToolbarViewController* viewController;
@property(nonatomic, strong) ToolbarMediator* mediator;
@end

@implementation ToolbarCoordinator
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
  self.viewController = [[ToolbarViewController alloc] init];

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

  self.viewController.dispatcher = static_cast<id>(self.browser->dispatcher());
  self.mediator.consumer = self.viewController;

  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
  [super start];
}

- (void)stop {
  [super stop];
  [self.browser->dispatcher() stopDispatchingToTarget:self];
}

#pragma mark - ToolsMenuCommands Implementation

- (void)showToolsMenu {
  ToolsCoordinator* toolsCoordinator = [[ToolsCoordinator alloc] init];
  [self addChildCoordinator:toolsCoordinator];
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
  self.webState->GetNavigationManager()->GoBack();
}

- (void)goForward {
  self.webState->GetNavigationManager()->GoForward();
}

- (void)stopLoadingPage {
  self.webState->Stop();
}

- (void)reloadPage {
  self.webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                                false /*check_for_repost*/);
}

@end
