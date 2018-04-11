// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_coordinator.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_presenter.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the corresponding command type for a Popup menu |type|.
PopupMenuCommandType CommandTypeFromPopupType(PopupMenuType type) {
  if (type == PopupMenuTypeToolsMenu)
    return PopupMenuCommandTypeToolsMenu;
  return PopupMenuCommandTypeDefault;
}
}  // namespace

@interface PopupMenuCoordinator ()<ContainedPresenterDelegate,
                                   PopupMenuCommands>

// Presenter for the popup menu, managing the animations.
@property(nonatomic, strong) PopupMenuPresenter* presenter;
// Mediator for the popup menu.
@property(nonatomic, strong) PopupMenuMediator* mediator;
// Time when the presentation of the popup menu is requested.
@property(nonatomic, assign) NSTimeInterval requestStartTime;

@end

@implementation PopupMenuCoordinator

@synthesize dispatcher = _dispatcher;
@synthesize mediator = _mediator;
@synthesize presenter = _presenter;
@synthesize requestStartTime = _requestStartTime;
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
  [self presentPopupOfType:PopupMenuTypeNavigationBackward
            fromNamedGuide:kBackButtonGuide];
}

- (void)showNavigationHistoryForwardPopupMenu {
  base::RecordAction(
      base::UserMetricsAction("MobileToolbarShowTabHistoryMenu"));
  [self presentPopupOfType:PopupMenuTypeNavigationForward
            fromNamedGuide:kForwardButtonGuide];
}

- (void)showToolsMenuPopup {
  // The metric is registered at the toolbar level.
  [self presentPopupOfType:PopupMenuTypeToolsMenu
            fromNamedGuide:kToolsMenuGuide];
}

- (void)showTabGridButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowTabGridMenu"));
  [self presentPopupOfType:PopupMenuTypeTabGrid
            fromNamedGuide:kTabSwitcherGuide];
}

- (void)searchButtonPopup {
  base::RecordAction(base::UserMetricsAction("MobileToolbarShowSearchMenu"));
  [self presentPopupOfType:PopupMenuTypeSearch fromNamedGuide:nil];
}

- (void)dismissPopupMenu {
  [self.presenter dismissAnimated:YES];
  self.presenter = nil;
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidPresent:(id<ContainedPresenter>)presenter {
  DCHECK(presenter == self.presenter);
  if (self.requestStartTime != 0) {
    base::TimeDelta elapsed = base::TimeDelta::FromSecondsD(
        [NSDate timeIntervalSinceReferenceDate] - self.requestStartTime);
    UMA_HISTOGRAM_TIMES("Toolbar.ShowToolsMenuResponsiveness", elapsed);
    // Reset the start time to ensure that whatever happens, we only record
    // this once.
    self.requestStartTime = 0;
  }
}

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  // No-op.
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)note {
  [self dismissPopupMenu];
}

#pragma mark - Private

// Presents a popup menu of type |type| with an animation starting from
// |guideName|.
- (void)presentPopupOfType:(PopupMenuType)type
            fromNamedGuide:(GuideName*)guideName {
  DCHECK(!self.presenter);
  id<BrowserCommands> callableDispatcher =
      static_cast<id<BrowserCommands>>(self.dispatcher);
  [callableDispatcher
      prepareForPopupMenuPresentation:CommandTypeFromPopupType(type)];

  self.requestStartTime = [NSDate timeIntervalSinceReferenceDate];

  PopupMenuTableViewController* tableViewController =
      [[PopupMenuTableViewController alloc]
          initWithStyle:UITableViewStyleGrouped];
  tableViewController.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCommands>>(self.dispatcher);
  tableViewController.baseViewController = self.baseViewController;

  self.mediator = [[PopupMenuMediator alloc]
          initWithType:type
           isIncognito:self.browserState->IsOffTheRecord()
      readingListModel:ReadingListModelFactory::GetForBrowserState(
                           self.browserState)];
  self.mediator.engagementTracker =
      feature_engagement::TrackerFactory::GetForBrowserState(self.browserState);
  self.mediator.webStateList = self.webStateList;
  self.mediator.popupMenu = tableViewController;
  self.mediator.dispatcher = static_cast<id<BrowserCommands>>(self.dispatcher);
  self.mediator.bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(self.browserState);

  self.presenter = [[PopupMenuPresenter alloc] init];
  self.presenter.baseViewController = self.baseViewController;
  self.presenter.commandHandler = self;
  self.presenter.presentedViewController = tableViewController;
  self.presenter.guideName = guideName;
  self.presenter.delegate = self;

  [self.presenter prepareForPresentation];
  [self.presenter presentAnimated:YES];
  return;
}

@end
