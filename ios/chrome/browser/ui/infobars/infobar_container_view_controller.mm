// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_view_controller.h"

#include "base/ios/block_types.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
@implementation InfobarContainerViewController

- (void)viewDidLoad {
  self.view = [[UIView alloc] initWithFrame:CGRectMake(20, 100, 360, 75)];
}

#pragma mark - InfobarConsumer

- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate
                      position:(NSInteger)position {
  UIViewController* infoBarViewController =
      static_cast<UIViewController*>(infoBarDelegate);

  [self addChildViewController:infoBarViewController];
  [self.view addSubview:infoBarViewController.view];
  infoBarViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [infoBarViewController didMoveToParentViewController:self];

  [NSLayoutConstraint activateConstraints:@[
    [infoBarViewController.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [infoBarViewController.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [infoBarViewController.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [infoBarViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor]
  ]];
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
  [self.view setUserInteractionEnabled:enabled];
}

- (void)updateLayoutAnimated:(BOOL)animated {
  // NO-OP - This shouldn't be need in the new UI since we use autolayout for
  // the contained Infobars.
}

@end
