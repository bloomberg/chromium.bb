// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_handset_view_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_coordinator.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsCoordinator ()
@property(nonatomic, strong) UIViewController* viewController;
@property(nonatomic, strong) RecentTabsTableCoordinator* wrapperCoordinator;
@end

@implementation RecentTabsCoordinator
@synthesize viewController = _viewController;
@synthesize wrapperCoordinator = _wrapperCoordinator;

- (void)start {
  // HACK: Re-using old view controllers for now.
  self.wrapperCoordinator = [[RecentTabsTableCoordinator alloc]
      initWithLoader:nil
        browserState:self.browser->browser_state()
          dispatcher:nil];
  [self.wrapperCoordinator start];
  if (!IsIPadIdiom()) {
    self.viewController = [[RecentTabsHandsetViewController alloc]
        initWithViewController:[self.wrapperCoordinator viewController]];
    self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
    self.viewController.modalPresentationCapturesStatusBarAppearance = YES;
  } else {
    self.viewController = [self.wrapperCoordinator viewController];
  }
  [super start];
}

- (void)stop {
  [super stop];
  [self.wrapperCoordinator stop];
  self.wrapperCoordinator = nil;
  self.viewController = nil;
}

@end
