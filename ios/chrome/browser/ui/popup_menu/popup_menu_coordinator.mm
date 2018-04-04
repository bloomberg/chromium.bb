// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PopupMenuCoordinator ()<PopupMenuCommands>

// Presenter for the popup menu, managing the animations.
@property(nonatomic, strong) PopupMenuPresenter* presenter;
// Mediator for the popup menu.
@property(nonatomic, strong) PopupMenuMediator* mediator;

@end

@implementation PopupMenuCoordinator

@synthesize dispatcher = _dispatcher;
@synthesize mediator = _mediator;
@synthesize presenter = _presenter;
@synthesize webStateList = _webStateList;

#pragma mark - ChromeCoordinator

- (void)start {
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(PopupMenuCommands)];
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];
}

- (void)stop {
  [self.dispatcher stopDispatchingToTarget:self];
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Public

- (BOOL)isShowingPopupMenu {
  return self.presenter != nil;
}

#pragma mark - PopupMenuCommands

- (void)showNavigationHistoryBackPopupMenu {
  base::RecordAction(
      base::UserMetricsAction("MobileToolbarShowTabHistoryMenu"));
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Back";
  viewController.view = label;
  // TODO(crbug.com/804779): Use the Navigation menu instead of a label.
  [self presentPopupForContent:viewController fromNamedGuide:kBackButtonGuide];
}

- (void)showNavigationHistoryForwardPopupMenu {
  base::RecordAction(
      base::UserMetricsAction("MobileToolbarShowTabHistoryMenu"));
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Forward";
  viewController.view = label;
  // TODO(crbug.com/804779): Use the Navigation menu instead of a label.
  [self presentPopupForContent:viewController
                fromNamedGuide:kForwardButtonGuide];
}

- (void)showToolsMenuPopup {
  PopupMenuTableViewController* tableViewController =
      [[PopupMenuTableViewController alloc]
          initWithStyle:UITableViewStyleGrouped];
  tableViewController.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands>>(self.dispatcher);
  tableViewController.baseViewController = self.baseViewController;

  self.mediator = [[PopupMenuMediator alloc]
          initWithType:PopupMenuTypeToolsMenu
           isIncognito:self.browserState->IsOffTheRecord()
      readingListModel:ReadingListModelFactory::GetForBrowserState(
                           self.browserState)];
  self.mediator.webStateList = self.webStateList;
  self.mediator.popupMenu = tableViewController;
  self.mediator.dispatcher = static_cast<id<BrowserCommands>>(self.dispatcher);

  [self presentPopupForContent:tableViewController
                fromNamedGuide:kToolsMenuGuide];
}

- (void)showTabGridButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowTabGridMenu"));
  PopupMenuTableViewController* tableViewController =
      [[PopupMenuTableViewController alloc] init];
  tableViewController.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands>>(self.dispatcher);
  tableViewController.baseViewController = self.baseViewController;

  self.mediator = [[PopupMenuMediator alloc]
          initWithType:PopupMenuTypeTabGrid
           isIncognito:self.browserState->IsOffTheRecord()
      readingListModel:ReadingListModelFactory::GetForBrowserState(
                           self.browserState)];
  self.mediator.webStateList = self.webStateList;
  self.mediator.popupMenu = tableViewController;

  [self presentPopupForContent:tableViewController
                fromNamedGuide:kTabSwitcherGuide];
}

- (void)searchButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowSearchMenu"));
  UIViewController* viewController = [[UIViewController alloc] init];
  UILabel* label = [[UILabel alloc] init];
  label.text = @"Search";
  viewController.view = label;
  // TODO(crbug.com/821560): Use the search menu instead of a label.
  [self presentPopupForContent:viewController fromNamedGuide:nil];
}

- (void)dismissPopupMenu {
  [self.presenter dismissAnimated:YES];
  self.presenter = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)note {
  [self dismissPopupMenu];
}

#pragma mark - Private

// Presents the |content| with an animation starting from |guideName|.
- (void)presentPopupForContent:(UIViewController*)content
                fromNamedGuide:(GuideName*)guideName {
  DCHECK(!self.presenter);
  self.presenter = [[PopupMenuPresenter alloc] init];
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.commandHandler = self;
  self.presenter.presentedViewController = content;
  self.presenter.guideName = guideName;

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];
  return;
}

@end
