// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/chrome/browser/ui/tab_grid/tab_grid_tab_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kCornerRadius = 4.0;
const CGFloat kSelectedBorderWidth = 2.0;
}

@implementation TabGridTabCell

@synthesize label = _label;

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.contentView.layer.cornerRadius = kCornerRadius;
    self.selectedBackgroundView.layer.cornerRadius = kCornerRadius;
    self.selectedBackgroundView.layer.borderWidth = kSelectedBorderWidth;
    self.selectedBackgroundView.layer.borderColor = [UIColor blueColor].CGColor;
    self.selectedBackgroundView.transform = CGAffineTransformScale(
        self.selectedBackgroundView.transform, 1.05, 1.05);

    _label = [[UILabel alloc] initWithFrame:CGRectZero];
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    _label.numberOfLines = 1;
    _label.textAlignment = NSTextAlignmentCenter;
    [self.contentView addSubview:_label];
    [NSLayoutConstraint activateConstraints:@[
      [_label.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_label.leadingAnchor
          constraintEqualToAnchor:self.contentView.layoutMarginsGuide
                                      .leadingAnchor],
      [_label.trailingAnchor
          constraintEqualToAnchor:self.contentView.layoutMarginsGuide
                                      .trailingAnchor],
    ]];
  }
  return self;
}

@end
