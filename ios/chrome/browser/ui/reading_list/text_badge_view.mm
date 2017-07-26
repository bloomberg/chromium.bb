// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/text_badge_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kFontSize = 10.0f;
const CGFloat kLabelMargin = 2.5f;
}

@interface TextBadgeView ()
@property(nonatomic, readonly, weak) UILabel* label;
@end

@implementation TextBadgeView

@synthesize label = _label;

- (instancetype)initWithText:(NSString*)text {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self setAccessibilityLabel:text];
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    _label = label;
    [label setFont:[[MDCTypography fontLoader] boldFontOfSize:kFontSize]];
    [label setTranslatesAutoresizingMaskIntoConstraints:NO];
    [label setTextColor:[UIColor whiteColor]];
    [label setText:text];
    [self addSubview:label];

    [NSLayoutConstraint activateConstraints:@[
      // Center label on badge.
      [label.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      [label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],

      // Make the badge height fit the label.
      [self.heightAnchor constraintEqualToAnchor:label.heightAnchor
                                        constant:kLabelMargin * 2],
      // Ensure that the badge will never be taller than it is wide. For
      // a tall label, the badge will look like a circle instead of an ellipse.
      [self.widthAnchor constraintGreaterThanOrEqualToAnchor:self.heightAnchor],
      // Make the badge width fit the label. Adding a constant offset equal to
      // the label's height positions the label within the rectangular portion
      // of the badge.
      [self.widthAnchor
          constraintGreaterThanOrEqualToAnchor:label.widthAnchor
                                      constant:label.intrinsicContentSize
                                                   .height],
    ]];

    [self setBackgroundColor:[[MDCPalette cr_bluePalette] tint500]];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  // Set the badge's corner radius to be one half of its height. This allows the
  // ends of the badge to be circular.
  self.layer.cornerRadius = self.bounds.size.height / 2.0f;
}

#pragma mark - Public properties

- (NSString*)text {
  return self.label.text;
}

- (void)setText:(NSString*)text {
  DCHECK(text.length);
  self.label.text = text;
}

@end
