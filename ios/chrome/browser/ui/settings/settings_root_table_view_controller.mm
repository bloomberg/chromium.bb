// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsRootTableViewController ()

// The AppBar for this view controller.
@property(nonatomic, readwrite, strong) MDCAppBar* appBar;

@end

@implementation SettingsRootTableViewController
@synthesize appBar = _appBar;

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    // Configure the AppBar.
    _appBar = [[MDCAppBar alloc] init];
    [self addChildViewController:_appBar.headerViewController];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Configure the app bar.  This cannot be in a shared base class because
  // UITableViewControllers normally do not use MDC styling, but this one does
  // in order to match the other Settings screens.
  ConfigureAppBarWithCardStyle(self.appBar);
  self.appBar.headerViewController.headerView.trackingScrollView =
      self.tableView;
  // Add the AppBar's views after all other views have been registered.
  [self.appBar addSubviewsToParent];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Set up the "Done" button in the upper right of the nav bar.
  SettingsNavigationController* navigationController =
      base::mac::ObjCCast<SettingsNavigationController>(
          self.navigationController);
  UIBarButtonItem* doneButton = [navigationController doneButton];
  self.navigationItem.rightBarButtonItem = doneButton;
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.appBar.headerViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.appBar.headerViewController;
}

#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidScroll];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDecelerating];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDraggingWillDecelerate:decelerate];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  MDCFlexibleHeaderView* headerView =
      self.appBar.headerViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView
        trackingScrollViewWillEndDraggingWithVelocity:velocity
                                  targetContentOffset:targetContentOffset];
  }
}

@end
