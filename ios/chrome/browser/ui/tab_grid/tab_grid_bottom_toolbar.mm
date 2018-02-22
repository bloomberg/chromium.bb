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
// Diameter of the center round button.
const CGFloat kRoundButtonDiameter = 44.0f;
}  // namespace

@implementation TabGridBottomToolbar
@synthesize theme = _theme;
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

    [leadingButton setTitle:@"Button" forState:UIControlStateNormal];

    UIButton* trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
    trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    [trailingButton setTitle:@"Button" forState:UIControlStateNormal];

    UIButton* roundButton = [UIButton buttonWithType:UIButtonTypeSystem];
    roundButton.translatesAutoresizingMaskIntoConstraints = NO;
    [roundButton setTitle:@"Btn" forState:UIControlStateNormal];
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

#pragma mark - Public

- (void)setTheme:(TabGridBottomToolbarTheme)theme {
  if (_theme == theme)
    return;
  switch (theme) {
    case TabGridBottomToolbarThemeWhiteRoundButton:
      self.trailingButton.enabled = YES;
      self.roundButton.enabled = YES;
      self.roundButton.backgroundColor = [UIColor whiteColor];
      self.roundButton.tintColor = [UIColor blackColor];
      self.leadingButton.tintColor = [UIColor whiteColor];
      self.trailingButton.tintColor = [UIColor whiteColor];
      break;
    case TabGridBottomToolbarThemeBlueRoundButton:
      self.trailingButton.enabled = YES;
      self.roundButton.enabled = YES;
      self.roundButton.backgroundColor =
          [UIColor colorWithRed:0.0 green:122.0 / 255.0 blue:1.0 alpha:1.0];
      self.roundButton.tintColor = [UIColor whiteColor];
      self.leadingButton.tintColor = [UIColor whiteColor];
      self.trailingButton.tintColor = [UIColor whiteColor];
      break;
    case TabGridBottomToolbarThemePartiallyDisabled:
      self.trailingButton.enabled = NO;
      self.roundButton.enabled = NO;
      self.roundButton.backgroundColor = [UIColor grayColor];
      self.roundButton.tintColor = [UIColor blackColor];
      self.leadingButton.tintColor = [UIColor whiteColor];
      self.trailingButton.tintColor = [UIColor whiteColor];
      break;
    default:
      NOTREACHED() << "Invalid theme for TabGridBottomToolbar.";
      break;
  }
  _theme = theme;
}

@end
