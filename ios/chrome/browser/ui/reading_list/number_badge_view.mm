// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/number_badge_view.h"

#import <Foundation/Foundation.h>

#include "base/format_macros.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kAnimationDuration = ios::material::kDuration3;
const CGFloat labelMargin = 2.5f;
}  // namespace

@interface NumberBadgeView ()
@property(nonatomic, readonly, weak) UILabel* label;
// This is a pill-shaped (rounded corners) view used in the background of the
// badge.
@property(nonatomic, readonly, weak) UIView* backgroundCircleView;
@property(nonatomic, assign) NSInteger displayNumber;
@end

@implementation NumberBadgeView
#pragma mark - properties
@synthesize label = _label;
@synthesize backgroundCircleView = _backgroundCircleView;
@synthesize displayNumber = _displayNumber;

#pragma mark - lifecycle
- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];

    UIView* backgroundCircleView = [[UIView alloc] initWithFrame:CGRectZero];
    _backgroundCircleView = backgroundCircleView;
    [backgroundCircleView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [self addSubview:backgroundCircleView];

    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    _label = label;
    [label setFont:[[MDFRobotoFontLoader sharedInstance] boldFontOfSize:10]];
    [label setTranslatesAutoresizingMaskIntoConstraints:NO];
    [label setTextColor:[UIColor whiteColor]];
    [self addSubview:label];

    [NSLayoutConstraint activateConstraints:@[
      // Position bubble.
      [backgroundCircleView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [backgroundCircleView.centerYAnchor
          constraintEqualToAnchor:self.centerYAnchor],

      // Position label on bubble.
      [label.centerXAnchor
          constraintEqualToAnchor:backgroundCircleView.centerXAnchor],
      [label.centerYAnchor
          constraintEqualToAnchor:backgroundCircleView.centerYAnchor],

      // Make bubble fit label.
      [backgroundCircleView.heightAnchor
          constraintEqualToAnchor:label.heightAnchor
                         constant:labelMargin * 2],
      [backgroundCircleView.widthAnchor
          constraintGreaterThanOrEqualToAnchor:backgroundCircleView
                                                   .heightAnchor],
      [backgroundCircleView.widthAnchor
          constraintGreaterThanOrEqualToAnchor:label.widthAnchor
                                      constant:labelMargin * 2]
    ]];

    // Start hidden.
    self.alpha = 0.0;
  }
  return self;
}

#pragma mark - UIView
- (void)layoutSubviews {
  [super layoutSubviews];
  self.backgroundCircleView.layer.cornerRadius =
      self.backgroundCircleView.bounds.size.height / 2.0f;
}

#pragma mark - public
- (void)setNumber:(NSInteger)number animated:(BOOL)animated {
  // If the previous number and current number match, do nothing.
  if (self.displayNumber != number) {
    self.displayNumber = number;
    if (animated) {
      [UIView animateWithDuration:kAnimationDuration
                       animations:^{
                         if (number > 0) {
                           self.alpha = 1.0;
                           // Only setting when > 0 as this makes the animation
                           // look better than switching to 0 then fading out.
                           self.label.text =
                               [NSString stringWithFormat:@"%" PRIdNS, number];
                         } else {
                           self.alpha = 0.0;
                         }
                         [self setNeedsLayout];
                         [self layoutIfNeeded];
                       }];
    } else {
      self.label.text = [NSString stringWithFormat:@"%" PRIdNS, number];
    }
  }
}

- (void)setBackgroundColor:(UIColor*)backgroundColor animated:(BOOL)animated {
  if (animated) {
    [UIView animateWithDuration:kAnimationDuration
                     animations:^{
                       [self.backgroundCircleView
                           setBackgroundColor:backgroundColor];
                     }];
  } else {
    [self.backgroundCircleView setBackgroundColor:backgroundColor];
  }
}

@end
