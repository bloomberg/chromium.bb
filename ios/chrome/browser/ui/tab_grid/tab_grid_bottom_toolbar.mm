// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the toolbar.
const CGFloat kToolbarHeight = 44.0f;
// Diameter of the center round button.
const CGFloat kRoundButtonDiameter = 44.0f;
}  // namespace

@implementation TabGridBottomToolbar
@synthesize leadingButton = _leadingButton;
@synthesize trailingButton = _trailingButton;
@synthesize roundButton = _roundButton;

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
    leadingButton.tintColor = [UIColor whiteColor];
    [leadingButton setTitle:@"Button" forState:UIControlStateNormal];

    UIButton* trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
    trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    trailingButton.tintColor = [UIColor whiteColor];
    [trailingButton setTitle:@"Button" forState:UIControlStateNormal];

    UIButton* roundButton = [UIButton buttonWithType:UIButtonTypeSystem];
    roundButton.translatesAutoresizingMaskIntoConstraints = NO;
    roundButton.tintColor = [UIColor whiteColor];
    [roundButton setTitle:@"Btn" forState:UIControlStateNormal];
    roundButton.backgroundColor = [UIColor colorWithRed:63 / 255.0f
                                                  green:81 / 255.0f
                                                   blue:181 / 255.0f
                                                  alpha:1.0f];
    roundButton.layer.cornerRadius = 22.f;
    roundButton.layer.masksToBounds = YES;

    [toolbar.contentView addSubview:leadingButton];
    [toolbar.contentView addSubview:trailingButton];
    [toolbar.contentView addSubview:roundButton];
    _leadingButton = leadingButton;
    _trailingButton = trailingButton;
    _roundButton = roundButton;

    NSArray* constraints = @[
      [toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
      [toolbar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [leadingButton.heightAnchor constraintEqualToConstant:kToolbarHeight],
      [leadingButton.leadingAnchor
          constraintEqualToAnchor:toolbar.layoutMarginsGuide.leadingAnchor],
      [leadingButton.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
      [roundButton.widthAnchor constraintEqualToConstant:kRoundButtonDiameter],
      [roundButton.heightAnchor constraintEqualToConstant:kRoundButtonDiameter],
      [roundButton.centerXAnchor constraintEqualToAnchor:toolbar.centerXAnchor],
      [roundButton.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
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
