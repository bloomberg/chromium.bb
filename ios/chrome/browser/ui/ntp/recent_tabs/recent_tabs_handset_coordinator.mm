// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_handset_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_handset_view_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsHandsetCoordinator ()<
    RecentTabsHandsetViewControllerCommand>

@property(nonatomic, strong)
    RecentTabsHandsetViewController* recentTabsViewController;
@property(nonatomic, strong) RecentTabsPanelController* panelController;

@end

@implementation RecentTabsHandsetCoordinator

@synthesize recentTabsViewController = _recentTabsViewController;
@synthesize panelController = _panelController;
@synthesize browserState = _browserState;
@synthesize dispatcher = _dispatcher;
@synthesize loader = _loader;

- (void)start {
  DCHECK(self.browserState);

  self.panelController =
      [[RecentTabsPanelController alloc] initWithLoader:self.loader
                                           browserState:self.browserState
                                             dispatcher:self.dispatcher];

  self.recentTabsViewController = [[RecentTabsHandsetViewController alloc]
      initWithViewController:[self.panelController viewController]];
  self.recentTabsViewController.commandHandler = self;
  self.recentTabsViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.recentTabsViewController.modalPresentationCapturesStatusBarAppearance =
      YES;
  [self.baseViewController presentViewController:self.recentTabsViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.recentTabsViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
  [self.panelController dismissKeyboard];
  [self.panelController dismissModals];
}

#pragma mark - RecentTabsHandsetViewControllerCommand

- (void)dismissRecentTabs {
  [self stop];
}

@end
