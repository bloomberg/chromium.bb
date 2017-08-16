// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsCoordinator ()
@property(nonatomic, strong) UIViewController* viewController;
@property(nonatomic, strong) RecentTabsPanelController* wrapperController;
@end

@implementation RecentTabsCoordinator
@synthesize viewController = _viewController;
@synthesize wrapperController = _wrapperController;

- (void)start {
  // HACK: Re-using old view controllers for now.
  if (!IsIPadIdiom()) {
    self.viewController = [RecentTabsPanelViewController
        controllerToPresentForBrowserState:self.browser->browser_state()
                                    loader:nil
                                dispatcher:nil];
    self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
    self.viewController.modalPresentationCapturesStatusBarAppearance = YES;
  } else {
    self.wrapperController = [[RecentTabsPanelController alloc]
        initWithLoader:nil
          browserState:self.browser->browser_state()
            dispatcher:nil];
    self.viewController = [self.wrapperController viewController];
  }
  [super start];
}

@end
