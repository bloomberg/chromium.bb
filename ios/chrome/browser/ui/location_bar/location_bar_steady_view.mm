// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LocationBarSteadyView
@synthesize locationLabel = _locationLabel;
@synthesize locationIconImageView = _locationIconImageView;

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _locationLabel = [[UILabel alloc] init];
    _locationIconImageView = [[UIImageView alloc] init];

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _locationIconImageView, _locationLabel ]];
    [self addSubview:stackView];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.distribution = UIStackViewDistributionEqualCentering;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.userInteractionEnabled = NO;

    AddSameCenterConstraints(stackView, self);
    [NSLayoutConstraint activateConstraints:@[
      [stackView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
      [stackView.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.trailingAnchor],
      [stackView.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:self.bottomAnchor],
      [stackView.topAnchor constraintLessThanOrEqualToAnchor:self.topAnchor],
    ]];
  }
  return self;
}

@end
