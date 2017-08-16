// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history_popup/tab_history_coordinator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/history_popup_commands.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_constants.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_positioner.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_presentation.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_ui_updater.h"
#import "ios/chrome/browser/ui/history_popup/tab_history_popup_controller.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface TabHistoryCoordinator ()<PopupMenuDelegate>

// The TabHistoryPopupController instance that this coordinator will be
// presenting.
@property(nonatomic, strong)
    TabHistoryPopupController* tabHistoryPopupController;

@end

@implementation TabHistoryCoordinator

@synthesize dispatcher = _dispatcher;
@synthesize positionProvider = _positionProvider;
@synthesize presentationProvider = _presentationProvider;
@synthesize tabHistoryPopupController = _tabHistoryPopupController;
@synthesize tabHistoryUIUpdater = _tabHistoryUIUpdater;
@synthesize tabModel = _tabModel;

- (void)disconnect {
  self.dispatcher = nil;
}

- (void)setDispatcher:(CommandDispatcher*)dispatcher {
  if (dispatcher == self.dispatcher) {
    return;
  }
  if (self.dispatcher) {
    [self.dispatcher stopDispatchingToTarget:self];
  }
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(TabHistoryPopupCommands)];
  _dispatcher = dispatcher;
}

#pragma mark - TabHistoryPopupCommands

- (void)showTabHistoryPopupForBackwardHistory {
  Tab* tab = [self.tabModel currentTab];
  web::NavigationItemList backwardItems =
      [tab navigationManager]->GetBackwardItems();
  CGPoint origin = [self.baseViewController.view.window
      convertPoint:[self.positionProvider
                       originPointForToolbarButton:ToolbarButtonBack]
            toView:self.baseViewController.view];

  [self.tabHistoryUIUpdater
      updateUIForTabHistoryPresentationFrom:ToolbarButtonBack];
  [self presentTabHistoryPopupWithItems:backwardItems origin:origin];
}

- (void)showTabHistoryPopupForForwardHistory {
  Tab* tab = [self.tabModel currentTab];
  web::NavigationItemList forwardItems =
      [tab navigationManager]->GetForwardItems();
  CGPoint origin = [self.baseViewController.view.window
      convertPoint:[self.positionProvider
                       originPointForToolbarButton:ToolbarButtonForward]
            toView:self.baseViewController.view];

  [self.tabHistoryUIUpdater
      updateUIForTabHistoryPresentationFrom:ToolbarButtonForward];
  [self presentTabHistoryPopupWithItems:forwardItems origin:origin];
}

- (void)navigateToHistoryItem:(const web::NavigationItem*)item {
  [[self.tabModel currentTab] goToItem:item];
  [self dismissPopupMenu:nil];
}

#pragma mark - Helper Methods

// Present a Tab History Popup that displays |items|, and its view is presented
// from |origin|.
- (void)presentTabHistoryPopupWithItems:(const web::NavigationItemList&)items
                                 origin:(CGPoint)historyPopupOrigin {
  if (self.tabHistoryPopupController)
    return;
  base::RecordAction(UserMetricsAction("MobileToolbarShowTabHistoryMenu"));

  [self.presentationProvider prepareForTabHistoryPresentation];

  // Initializing also displays the Tab History Popup VC.
  self.tabHistoryPopupController = [[TabHistoryPopupController alloc]
      initWithOrigin:historyPopupOrigin
          parentView:self.baseViewController.view
               items:items
          dispatcher:static_cast<id<TabHistoryPopupCommands>>(self.dispatcher)];

  [self.tabHistoryPopupController setDelegate:self];

  // Notify observers that TabHistoryPopup will show.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabHistoryPopupWillShowNotification
                    object:nil];

  // Register to receive notification for when the App is backgrounded so we can
  // dismiss the TabHistoryPopup.
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];
}

// Dismiss the presented Tab History Popup.
- (void)dismissHistoryPopup {
  if (!self.tabHistoryPopupController)
    return;
  __block TabHistoryPopupController* tempController =
      self.tabHistoryPopupController;
  [tempController containerView].userInteractionEnabled = NO;
  [tempController dismissAnimatedWithCompletion:^{
    [self.tabHistoryUIUpdater updateUIForTabHistoryWasDismissed];
    // Reference tempTHPC so the block retains it.
    tempController = nil;
  }];
  // Reset _tabHistoryPopupController to prevent -applicationDidEnterBackground
  // from posting another kTabHistoryPopupWillHideNotification.
  self.tabHistoryPopupController = nil;

  // Stop listening for notifications since these are only used to dismiss the
  // Tab History Popup.
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kTabHistoryPopupWillHideNotification
                    object:nil];
}

#pragma mark - PopupMenuDelegate

- (void)dismissPopupMenu:(PopupMenuController*)controller {
  [self dismissHistoryPopup];
}

#pragma mark - Observer Methods

- (void)applicationDidEnterBackground:(NSNotification*)notify {
  if (self.tabHistoryPopupController) {
    // Dismiss the tab history popup without animation.
    [self.tabHistoryUIUpdater updateUIForTabHistoryWasDismissed];
    self.tabHistoryPopupController = nil;
    [[NSNotificationCenter defaultCenter]
        postNotificationName:kTabHistoryPopupWillHideNotification
                      object:nil];
  }
}

@end
