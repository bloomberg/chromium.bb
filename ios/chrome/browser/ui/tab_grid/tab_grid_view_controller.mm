// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* topToolbar = [[TabGridTopToolbar alloc] init];
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* bottomToolbar = [[TabGridBottomToolbar alloc] init];
  bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:topToolbar];
  [self.view addSubview:bottomToolbar];

  NSMutableArray* constraints = [NSMutableArray
      arrayWithObjects:[topToolbar.topAnchor
                           constraintEqualToAnchor:self.view.topAnchor],
                       [topToolbar.leadingAnchor
                           constraintEqualToAnchor:self.view.leadingAnchor],
                       [topToolbar.trailingAnchor
                           constraintEqualToAnchor:self.view.trailingAnchor],
                       [bottomToolbar.bottomAnchor
                           constraintEqualToAnchor:self.view.bottomAnchor],
                       [bottomToolbar.leadingAnchor
                           constraintEqualToAnchor:self.view.leadingAnchor],
                       [bottomToolbar.trailingAnchor
                           constraintEqualToAnchor:self.view.trailingAnchor],
                       nil];
  if (@available(iOS 11, *)) {
    // SafeArea is only available in iOS 11+.
    [constraints addObjectsFromArray:@[
      [topToolbar.bottomAnchor
          constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                         constant:topToolbar.intrinsicContentSize.height],
      [bottomToolbar.topAnchor
          constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                         constant:-bottomToolbar.intrinsicContentSize.height],
    ]];
  } else {
    // Top and bottom layout guides are deprecated starting in iOS 11.
    [constraints addObjectsFromArray:@[
      [topToolbar.bottomAnchor
          constraintEqualToAnchor:self.topLayoutGuide.bottomAnchor
                         constant:topToolbar.intrinsicContentSize.height],
      [bottomToolbar.topAnchor
          constraintEqualToAnchor:self.bottomLayoutGuide.topAnchor
                         constant:-bottomToolbar.intrinsicContentSize.height],
    ]];
  }
  [NSLayoutConstraint activateConstraints:constraints];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

@end
