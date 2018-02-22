// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_toolbars_view_controller.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCToolbarsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];

  TabGridTopToolbar* topToolbar = [[TabGridTopToolbar alloc] init];
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topToolbar];

  TabGridBottomToolbar* bottomToolbar1 = [[TabGridBottomToolbar alloc] init];
  bottomToolbar1.translatesAutoresizingMaskIntoConstraints = NO;
  bottomToolbar1.theme = TabGridBottomToolbarThemeWhiteRoundButton;
  [self.view addSubview:bottomToolbar1];

  TabGridBottomToolbar* bottomToolbar2 = [[TabGridBottomToolbar alloc] init];
  bottomToolbar2.translatesAutoresizingMaskIntoConstraints = NO;
  bottomToolbar2.theme = TabGridBottomToolbarThemeBlueRoundButton;
  [self.view addSubview:bottomToolbar2];

  TabGridBottomToolbar* bottomToolbar3 = [[TabGridBottomToolbar alloc] init];
  bottomToolbar3.translatesAutoresizingMaskIntoConstraints = NO;
  bottomToolbar3.theme = TabGridBottomToolbarThemePartiallyDisabled;
  [self.view addSubview:bottomToolbar3];

  NSArray* constraints = @[
    [topToolbar.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                         constant:10.0f],
    [topToolbar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [topToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [topToolbar.heightAnchor
        constraintEqualToConstant:topToolbar.intrinsicContentSize.height],
    [bottomToolbar1.topAnchor constraintEqualToAnchor:topToolbar.bottomAnchor
                                             constant:10.0f],
    [bottomToolbar1.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar1.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [bottomToolbar1.heightAnchor
        constraintEqualToConstant:bottomToolbar1.intrinsicContentSize.height],
    [bottomToolbar2.topAnchor
        constraintEqualToAnchor:bottomToolbar1.bottomAnchor
                       constant:10.0f],
    [bottomToolbar2.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar2.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [bottomToolbar2.heightAnchor
        constraintEqualToConstant:bottomToolbar2.intrinsicContentSize.height],
    [bottomToolbar3.topAnchor
        constraintEqualToAnchor:bottomToolbar2.bottomAnchor
                       constant:10.0f],
    [bottomToolbar3.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar3.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [bottomToolbar3.heightAnchor
        constraintEqualToConstant:bottomToolbar3.intrinsicContentSize.height],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}
@end
