// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
@implementation InfobarContainerViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view = [[UIView alloc] initWithFrame:CGRectZero];
}

- (void)addInfobarViewController:(UIViewController*)infobarViewController {
  [self addChildViewController:infobarViewController];
  [self.view addSubview:infobarViewController.view];
  [infobarViewController didMoveToParentViewController:self];
  infobarViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [infobarViewController.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:10],
    [infobarViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [infobarViewController.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [infobarViewController.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor]
  ]];
}

@end
