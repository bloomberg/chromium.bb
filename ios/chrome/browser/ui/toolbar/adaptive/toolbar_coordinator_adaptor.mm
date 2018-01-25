// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/toolbar_coordinator_adaptor.h"

#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinatorAdaptor ()<ToolbarCommands>
@property(nonatomic, strong)
    NSMutableArray<id<ToolbarCoordinating, ToolbarCommands>>* coordinators;
// Coordinator for the toolsMenu.
@property(nonatomic, strong) ToolsMenuCoordinator* toolsMenuCoordinator;
@end

@implementation ToolbarCoordinatorAdaptor

@synthesize coordinators = _coordinators;
@synthesize toolsMenuCoordinator = _toolsMenuCoordinator;

#pragma mark - Public

- (instancetype)initWithToolsMenuConfigurationProvider:
                    (id<ToolsMenuConfigurationProvider>)configurationProvider
                                            dispatcher:
                                                (CommandDispatcher*)dispatcher {
  self = [super init];
  if (self) {
    _toolsMenuCoordinator = [[ToolsMenuCoordinator alloc] init];
    _toolsMenuCoordinator.dispatcher = dispatcher;
    _toolsMenuCoordinator.configurationProvider = configurationProvider;
    [_toolsMenuCoordinator start];
    [dispatcher startDispatchingToTarget:self
                             forProtocol:@protocol(ToolbarCommands)];
    _coordinators = [NSMutableArray array];
  }
  return self;
}
- (void)addToolbarCoordinator:
    (id<ToolbarCoordinating, ToolbarCommands>)toolbarCoordinator {
  [self.coordinators addObject:toolbarCoordinator];
}

#pragma mark - IncognitoViewControllerDelegate

- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
  for (id<ToolbarCoordinating> coordinator in self.coordinators) {
    [coordinator setToolbarBackgroundAlpha:alpha];
  }
}

#pragma mark - ToolbarCommands

- (void)contractToolbar {
  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator contractToolbar];
  }
}

- (void)triggerToolsMenuButtonAnimation {
  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator triggerToolsMenuButtonAnimation];
  }
}

#pragma mark - ToolsMenuPresentationStateProvider

- (BOOL)isShowingToolsMenu {
  return [self.toolsMenuCoordinator isShowingToolsMenu];
}

@end
