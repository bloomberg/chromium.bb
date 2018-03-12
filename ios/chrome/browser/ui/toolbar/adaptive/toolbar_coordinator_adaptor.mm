// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/toolbar_coordinator_adaptor.h"

#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_ui_updater.h"
#import "ios/chrome/browser/ui/toolbar/adaptive/toolbar_coordinatee.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinatorAdaptor ()<TabHistoryUIUpdater, ToolbarCommands>
@property(nonatomic, strong)
    NSMutableArray<id<NewTabPageControllerDelegate, ToolbarCommands>>*
        coordinators;
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
    (id<NewTabPageControllerDelegate, ToolbarCommands>)toolbarCoordinator {
  [self.coordinators addObject:toolbarCoordinator];
}

#pragma mark - NewTabPageControllerDelegate

- (void)setToolbarBackgroundToIncognitoNTPColorWithAlpha:(CGFloat)alpha {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    [coordinator setToolbarBackgroundToIncognitoNTPColorWithAlpha:alpha];
  }
}

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    [coordinator setScrollProgressForTabletOmnibox:progress];
  }
}

#pragma mark - ToolbarCommands

- (void)triggerToolsMenuButtonAnimation {
  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator triggerToolsMenuButtonAnimation];
  }
}

#pragma mark - ToolsMenuPresentationStateProvider

- (BOOL)isShowingToolsMenu {
  return [self.toolsMenuCoordinator isShowingToolsMenu];
}

#pragma mark - SideSwipeToolbarInteracting

- (BOOL)isInsideToolbar:(CGPoint)point {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    // The toolbar frame is inset by -1 because CGRectContainsPoint does include
    // points on the max X and Y edges, which will happen frequently with edge
    // swipes from the right side.
    CGRect toolbarFrame =
        CGRectInset([coordinator viewController].view.frame, -1, -1);
    if (CGRectContainsPoint(toolbarFrame, point))
      return YES;
  }
  return NO;
}

#pragma mark - ToolbarCoordinating

- (void)updateToolsMenu {
  [self.toolsMenuCoordinator updateConfiguration];
}

- (id<TabHistoryUIUpdater>)tabHistoryUIUpdater {
  return self;
}

#pragma mark - TabHistoryUIUpdater

- (void)updateUIForTabHistoryPresentationFrom:(ToolbarButtonType)button {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.tabHistoryUIUpdater
        updateUIForTabHistoryPresentationFrom:button];
  }
}

- (void)updateUIForTabHistoryWasDismissed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.tabHistoryUIUpdater updateUIForTabHistoryWasDismissed];
  }
}

@end
