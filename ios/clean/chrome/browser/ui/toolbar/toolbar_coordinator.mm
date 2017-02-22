// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<ToolbarCommands>
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
  self.viewController.toolbarCommandHandler = self;
  self.mediator.consumer = self.viewController;

  [self.context.baseViewController presentViewController:self.viewController
                                                animated:self.context.animated
                                              completion:nil];
}

#pragma mark - ToolbarCommands

- (void)showToolsMenu {
  ToolsCoordinator* toolsCoordinator = [[ToolsCoordinator alloc] init];
  toolsCoordinator.toolbarCommandHandler = self;
  [self addChildCoordinator:toolsCoordinator];
  [toolsCoordinator start];
  self.toolsMenuCoordinator = toolsCoordinator;
}

- (void)closeToolsMenu {
  [self.toolsMenuCoordinator stop];
  [self removeChildCoordinator:self.toolsMenuCoordinator];
}

@end
