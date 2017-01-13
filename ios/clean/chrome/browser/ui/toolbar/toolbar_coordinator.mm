// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<ToolbarCommands>
@property(nonatomic, weak) ToolsCoordinator* toolsMenuCoordinator;
@property(nonatomic, strong) ToolbarViewController* viewController;
@end

@implementation ToolbarCoordinator
@synthesize toolsMenuCoordinator = _toolsMenuCoordinator;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[ToolbarViewController alloc] init];
  self.viewController.toolbarCommandHandler = self;

  [self.rootViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  const GURL& pageURL = webState->GetVisibleURL();
  [self.viewController
      setCurrentPageText:base::SysUTF8ToNSString(pageURL.spec())];
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
