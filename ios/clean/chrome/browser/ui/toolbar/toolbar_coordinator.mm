// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_coordinator.h"

#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/history_popup_commands.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_constants.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_configuration.h"
#import "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#import "ios/clean/chrome/browser/ui/history_popup/history_popup_coordinator.h"
#import "ios/clean/chrome/browser/ui/omnibox/location_bar_coordinator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_mediator.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_view_controller.h"
#import "ios/clean/chrome/browser/ui/tools/tools_coordinator.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<ToolsMenuCommands, TabHistoryPopupCommands>
// Location Bar contains the omnibox amongst other views.
@property(nonatomic, weak) LocationBarCoordinator* locationBarCoordinator;
// History Popup displays the forward or backward history in a popup menu.
@property(nonatomic, weak) HistoryPopupCoordinator* historyPopupCoordinator;
// Tools Menu coordinator.
@property(nonatomic, weak) ToolsCoordinator* toolsMenuCoordinator;
// The View Controller managed by this coordinator.
@property(nonatomic, strong) ToolbarViewController* viewController;
// The mediator owned by this coordinator.
@property(nonatomic, strong) ToolbarMediator* mediator;
@end

@implementation ToolbarCoordinator
@synthesize locationBarCoordinator = _locationBarCoordinator;
@synthesize historyPopupCoordinator = _historyPopupCoordinator;
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
  if (self.started)
    return;

  self.viewController = [[ToolbarViewController alloc]
      initWithDispatcher:static_cast<id>(self.browser->dispatcher())];

  CommandDispatcher* dispatcher = self.browser->dispatcher();
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(showToolsMenu)];
  [dispatcher startDispatchingToTarget:self
                           forSelector:@selector(closeToolsMenu)];
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(TabHistoryPopupCommands)];

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
  menuConfiguration.noOpenedTabs = NO;
  menuConfiguration.inNewTabPage =
      (self.webState->GetLastCommittedURL() == GURL(kChromeUINewTabURL));

  toolsCoordinator.toolsMenuConfiguration = menuConfiguration;
  toolsCoordinator.webState = self.webState;
  [toolsCoordinator start];
  self.toolsMenuCoordinator = toolsCoordinator;
}

- (void)closeToolsMenu {
  [self.toolsMenuCoordinator stop];
  [self removeChildCoordinator:self.toolsMenuCoordinator];
}

#pragma mark - HistoryPopupCommands Implementation

- (void)showTabHistoryPopupForBackwardHistory {
  if (!self.historyPopupCoordinator) {
    HistoryPopupCoordinator* historyPopupCoordinator =
        [[HistoryPopupCoordinator alloc] init];
    historyPopupCoordinator.positionProvider = self.viewController;
    historyPopupCoordinator.presentationProvider = self.viewController;
    historyPopupCoordinator.tabHistoryUIUpdater = self.viewController;
    historyPopupCoordinator.webState = self.webState;
    historyPopupCoordinator.presentingButton = ToolbarButtonTypeBack;
    historyPopupCoordinator.navigationItems =
        self.webState->GetNavigationManager()->GetBackwardItems();
    self.historyPopupCoordinator = historyPopupCoordinator;
    [self addChildCoordinator:self.historyPopupCoordinator];
  }
  [self.historyPopupCoordinator start];
}

- (void)showTabHistoryPopupForForwardHistory {
  if (!self.historyPopupCoordinator) {
    HistoryPopupCoordinator* historyPopupCoordinator =
        [[HistoryPopupCoordinator alloc] init];
    historyPopupCoordinator.positionProvider = self.viewController;
    historyPopupCoordinator.presentationProvider = self.viewController;
    historyPopupCoordinator.tabHistoryUIUpdater = self.viewController;
    historyPopupCoordinator.webState = self.webState;
    historyPopupCoordinator.presentingButton = ToolbarButtonTypeForward;
    historyPopupCoordinator.navigationItems =
        self.webState->GetNavigationManager()->GetForwardItems();
    self.historyPopupCoordinator = historyPopupCoordinator;
    [self addChildCoordinator:self.historyPopupCoordinator];
  }
  [self.historyPopupCoordinator start];
}

- (void)navigateToHistoryItem:(const web::NavigationItem*)item {
  DCHECK(item);
  int index = self.webState->GetNavigationManager()->GetIndexOfItem(item);
  DCHECK_GT(index, -1);
  self.webState->GetNavigationManager()->GoToIndex(index);
  [self.historyPopupCoordinator stop];
}

@end
