// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the toolbar.
const CGFloat kToolbarHeight = 44.0f;
}  // namespace

@implementation TabGridBottomToolbar
@synthesize leadingButton = _leadingButton;
@synthesize trailingButton = _trailingButton;
@synthesize centerButton = _centerButton;

- (instancetype)init {
  if (self = [super initWithFrame:CGRectZero]) {
    UIVisualEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* toolbar =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    toolbar.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:toolbar];

    UIButton* leadingButton = [UIButton buttonWithType:UIButtonTypeSystem];
    leadingButton.translatesAutoresizingMaskIntoConstraints = NO;
    UIButton* trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
    trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    UIButton* centerButton = [UIButton buttonWithType:UIButtonTypeSystem];
    centerButton.translatesAutoresizingMaskIntoConstraints = NO;

    [toolbar.contentView addSubview:leadingButton];
    [toolbar.contentView addSubview:trailingButton];
    [toolbar.contentView addSubview:centerButton];
    _leadingButton = leadingButton;
    _trailingButton = trailingButton;
    _centerButton = centerButton;

    NSArray* constraints = @[
      [toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
      [toolbar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [leadingButton.heightAnchor constraintEqualToConstant:kToolbarHeight],
      [leadingButton.leadingAnchor
          constraintEqualToAnchor:toolbar.layoutMarginsGuide.leadingAnchor],
      [leadingButton.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
      [centerButton.centerXAnchor
          constraintEqualToAnchor:toolbar.centerXAnchor],
      [centerButton.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
      [trailingButton.heightAnchor constraintEqualToConstant:kToolbarHeight],
      [trailingButton.trailingAnchor
          constraintEqualToAnchor:toolbar.layoutMarginsGuide.trailingAnchor],
      [trailingButton.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kToolbarHeight);
}

@end
