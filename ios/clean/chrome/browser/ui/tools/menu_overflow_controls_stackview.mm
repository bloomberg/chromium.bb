// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/menu_overflow_controls_stackview.h"

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button+factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kStackSpacing = 15.0;
}

@implementation MenuOverflowControlsStackView
@synthesize shareButton = _shareButton;
@synthesize reloadButton = _reloadButton;
@synthesize stopButton = _stopButton;
@synthesize starButton = _starButton;

- (instancetype)init {
  if ((self = [super init])) {
    [self setUpToolbarButtons];
    [self addArrangedSubview:self.shareButton];
    [self addArrangedSubview:self.starButton];
    [self addArrangedSubview:self.stopButton];
    [self addArrangedSubview:self.reloadButton];

    self.spacing = kStackSpacing;
    self.axis = UILayoutConstraintAxisHorizontal;
    self.distribution = UIStackViewDistributionFillEqually;
  }
  return self;
}

#pragma mark - Components Setup

- (void)setUpToolbarButtons {
  // Share button.
  self.shareButton = [ToolbarButton shareToolbarButton];

  // Reload button.
  self.reloadButton = [ToolbarButton reloadToolbarButton];

  // Stop button.
  self.stopButton = [ToolbarButton stopToolbarButton];

  // Star button.
  self.starButton = [ToolbarButton starToolbarButton];
}

@end
