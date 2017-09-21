// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_coordinator.h"

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/clean/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"
#import "ios/clean/chrome/browser/ui/commands/ntp_commands.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_home_coordinator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_incognito_coordinator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_mediator.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_metrics_recorder.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_view_controller.h"
#import "ios/clean/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

class PrefService;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPCoordinator ()<NTPCommands>
@property(nonatomic, strong) NTPMediator* mediator;
@property(nonatomic, strong) NTPViewController* viewController;
@property(nonatomic, strong) NTPHomeCoordinator* homeCoordinator;
@property(nonatomic, strong) BookmarksCoordinator* bookmarksCoordinator;
@property(nonatomic, strong) RecentTabsCoordinator* recentTabsCoordinator;
@property(nonatomic, strong) NTPIncognitoCoordinator* incognitoCoordinator;
@property(nonatomic, strong) NTPMetricsRecorder* metricsRecorder;
@end

@implementation NTPCoordinator
@synthesize mediator = _mediator;
@synthesize viewController = _viewController;
@synthesize homeCoordinator = _homeCoordinator;
@synthesize bookmarksCoordinator = _bookmarksCoordinator;
@synthesize recentTabsCoordinator = _recentTabsCoordinator;
@synthesize incognitoCoordinator = _incognitoCoordinator;
@synthesize metricsRecorder = _metricsRecorder;

- (void)start {
  if (self.started)
    return;

  self.viewController = [[NTPViewController alloc] init];
  self.mediator = [[NTPMediator alloc]
      initWithConsumer:self.viewController
           inIncognito:self.browser->browser_state()->IsOffTheRecord()];

  // NTPCommands
  [self.dispatcher startDispatchingToTarget:self
                                forProtocol:@protocol(NTPCommands)];
  self.viewController.dispatcher = self.callableDispatcher;
  [self.browser->broadcaster()
      broadcastValue:@"selectedNTPPanel"
            ofObject:self.viewController
            selector:@selector(broadcastSelectedNTPPanel:)];

  PrefService* prefs =
      ios::ChromeBrowserState::FromBrowserState(self.browser->browser_state())
          ->GetPrefs();
  self.metricsRecorder = [[NTPMetricsRecorder alloc] initWithPrefService:prefs];
  [self.dispatcher registerMetricsRecorder:self.metricsRecorder
                               forSelector:@selector(showNTPHomePanel)];
  [super start];
}

- (void)stop {
  [super stop];
  [self.browser->broadcaster()
      stopBroadcastingForSelector:@selector(broadcastSelectedNTPPanel:)];
  [self.dispatcher stopDispatchingToTarget:self];
  [self.dispatcher
      deregisterMetricsRecordingForSelector:@selector(showNTPHomePanel)];
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)coordinator {
  if ([coordinator isKindOfClass:[NTPHomeCoordinator class]]) {
    self.viewController.homeViewController = coordinator.viewController;

  } else if (coordinator == self.bookmarksCoordinator) {
    if (self.bookmarksCoordinator.mode == CONTAINED) {
      self.viewController.bookmarksViewController = coordinator.viewController;
    } else {
      coordinator.viewController.modalPresentationStyle =
          UIModalPresentationFormSheet;
      [self.viewController presentViewController:coordinator.viewController
                                        animated:YES
                                      completion:nil];
    }

  } else if (coordinator == self.recentTabsCoordinator) {
    if (self.recentTabsCoordinator.mode == CONTAINED) {
      self.viewController.recentTabsViewController = coordinator.viewController;
    } else {
      coordinator.viewController.modalPresentationStyle =
          UIModalPresentationFormSheet;
      coordinator.viewController.modalPresentationCapturesStatusBarAppearance =
          YES;
      [self.viewController presentViewController:coordinator.viewController
                                        animated:YES
                                      completion:nil];
    }

  } else if (coordinator == self.incognitoCoordinator) {
    self.viewController.incognitoViewController = coordinator.viewController;
  }
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  if ([childCoordinator isKindOfClass:[BookmarksCoordinator class]] &&
      !IsIPadIdiom()) {
    [childCoordinator.viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  } else if ([childCoordinator isKindOfClass:[RecentTabsCoordinator class]] &&
             !IsIPadIdiom()) {
    [childCoordinator.viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
}

#pragma mark - NTPCommands

- (void)showNTPHomePanel {
  NTPHomeCoordinator* panelCoordinator = [[NTPHomeCoordinator alloc] init];
  [self addChildCoordinator:panelCoordinator];
  [panelCoordinator start];
}

- (void)showNTPBookmarksPanel {
  if (!self.bookmarksCoordinator) {
    self.bookmarksCoordinator = [[BookmarksCoordinator alloc] init];
    self.bookmarksCoordinator.mode = IsIPadIdiom() ? CONTAINED : PRESENTED;
    [self addChildCoordinator:self.bookmarksCoordinator];
  }
  [self.bookmarksCoordinator start];
}

- (void)showNTPRecentTabsPanel {
  if (!self.recentTabsCoordinator) {
    self.recentTabsCoordinator = [[RecentTabsCoordinator alloc] init];
    self.recentTabsCoordinator.mode = IsIPadIdiom() ? CONTAINED : PRESENTED;
    [self addChildCoordinator:self.recentTabsCoordinator];
  }
  [self.recentTabsCoordinator start];
}

- (void)showNTPIncognitoPanel {
  if (!self.incognitoCoordinator) {
    self.incognitoCoordinator = [[NTPIncognitoCoordinator alloc] init];
    [self addChildCoordinator:self.incognitoCoordinator];
  }
  [self.incognitoCoordinator start];
}

@end
