// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_top_toolbar.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_page_control.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the toolbar.
const CGFloat kToolbarHeight = 52.0f;
}  // namespace

@implementation TabGridTopToolbar
@synthesize leadingButton = _leadingButton;
@synthesize trailingButton = _trailingButton;
@synthesize pageControl = _pageControl;

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
    leadingButton.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    leadingButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    leadingButton.tintColor = [UIColor whiteColor];

    // The segmented control has an intrinsic size.
    TabGridPageControl* pageControl = [[TabGridPageControl alloc] init];
    pageControl.translatesAutoresizingMaskIntoConstraints = NO;

    UIButton* trailingButton = [UIButton buttonWithType:UIButtonTypeSystem];
    trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    trailingButton.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    trailingButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    trailingButton.tintColor = [UIColor whiteColor];

    [toolbar.contentView addSubview:leadingButton];
    [toolbar.contentView addSubview:trailingButton];
    [toolbar.contentView addSubview:pageControl];
    _leadingButton = leadingButton;
    _trailingButton = trailingButton;
    _pageControl = pageControl;

    NSArray* constraints = @[
      [toolbar.topAnchor constraintEqualToAnchor:self.topAnchor],
      [toolbar.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [toolbar.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [toolbar.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [leadingButton.heightAnchor constraintEqualToConstant:kToolbarHeight],
      [leadingButton.leadingAnchor
          constraintEqualToAnchor:toolbar.layoutMarginsGuide.leadingAnchor],
      [leadingButton.bottomAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
      [pageControl.centerXAnchor constraintEqualToAnchor:toolbar.centerXAnchor],
      [pageControl.bottomAnchor constraintEqualToAnchor:toolbar.bottomAnchor
                                               constant:-7.0f],
      [trailingButton.heightAnchor constraintEqualToConstant:kToolbarHeight],
      [trailingButton.trailingAnchor
          constraintEqualToAnchor:toolbar.layoutMarginsGuide.trailingAnchor],
      [trailingButton.bottomAnchor
          constraintEqualToAnchor:toolbar.bottomAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kToolbarHeight);
}

@end
