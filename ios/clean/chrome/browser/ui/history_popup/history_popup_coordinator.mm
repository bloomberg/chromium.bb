// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/history_popup/history_popup_coordinator.h"

#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/history_popup_commands.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_positioner.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_presentation.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_ui_updater.h"
#import "ios/chrome/browser/ui/history_popup/tab_history_popup_controller.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryPopupCoordinator ()<PopupMenuDelegate>
// Coordinator callableDispatcher.
@property(nonatomic, readonly) id<TabHistoryPopupCommands> callableDispatcher;
// The TabHistoryPopupController instance that this coordinator will be
// presenting.
@property(nonatomic, strong)
    TabHistoryPopupController* tabHistoryPopupController;
// The View Controller managed by this coordinator.
@property(nonatomic, strong) UIViewController* viewController;
@end

@implementation HistoryPopupCoordinator

@dynamic callableDispatcher;
@synthesize navigationItems = _navigationItems;
@synthesize positionProvider = _positionProvider;
@synthesize presentationProvider = _presentationProvider;
@synthesize presentingButton = _presentingButton;
@synthesize tabHistoryPopupController = _tabHistoryPopupController;
@synthesize tabHistoryUIUpdater = _tabHistoryUIUpdater;
@synthesize viewController = _viewController;
@synthesize webState = _webState;

#pragma mark - BrowserCoordinator

- (void)start {
  if (self.started)
    return;

  CGPoint historyPopupOrigin = [
      [self.presentationProvider viewForTabHistoryPresentation].window
      convertPoint:[self.positionProvider
                       originPointForToolbarButton:self.presentingButton]
            toView:[self.presentationProvider viewForTabHistoryPresentation]];

  // Initializing also displays the Tab History Popup VC.
  self.tabHistoryPopupController = [[TabHistoryPopupController alloc]
      initWithOrigin:historyPopupOrigin
          parentView:[self.presentationProvider viewForTabHistoryPresentation]
               items:self.navigationItems
          dispatcher:self.callableDispatcher];

  [self.tabHistoryUIUpdater
      updateUIForTabHistoryPresentationFrom:self.presentingButton];
  [self.tabHistoryPopupController setDelegate:self];

  [super start];
}

- (void)stop {
  [super stop];

  if (!self.tabHistoryPopupController)
    return;
  [self.tabHistoryPopupController containerView].userInteractionEnabled = NO;
  [self.tabHistoryPopupController dismissAnimatedWithCompletion:^{
    [self.tabHistoryUIUpdater updateUIForTabHistoryWasDismissed];
  }];
  // Reset _tabHistoryPopupController to prevent -applicationDidEnterBackground
  // from posting another kTabHistoryPopupWillHideNotification.
  self.tabHistoryPopupController = nil;
}

#pragma mark - PopupMenuDelegate

- (void)dismissPopupMenu:(PopupMenuController*)controller {
  [self stop];
}

@end
