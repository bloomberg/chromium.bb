// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/menu_overflow_controls_stackview.h"

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button.h"
#import "ios/clean/chrome/browser/ui/toolbar/toolbar_button_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kStackSpacing = 15.0;
}

@interface MenuOverflowControlsStackView ()
// Factory creating buttons with the correct style.
@property(nonatomic, strong) CleanToolbarButtonFactory* factory;
@end

@implementation MenuOverflowControlsStackView
@synthesize factory = _factory;
@synthesize shareButton = _shareButton;
@synthesize reloadButton = _reloadButton;
@synthesize stopButton = _stopButton;
@synthesize starButton = _starButton;

- (instancetype)initWithFactory:(CleanToolbarButtonFactory*)buttonFactory {
  if ((self = [super initWithFrame:CGRectZero])) {
    _factory = buttonFactory;
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
  self.shareButton = [self.factory shareToolbarButton];

  // Reload button.
  self.reloadButton = [self.factory reloadToolbarButton];

  // Stop button.
  self.stopButton = [self.factory stopToolbarButton];

  // Star button.
  self.starButton = [self.factory starToolbarButton];
}

@end
