// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_tab_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kBorderMargin = 6.0f;
const CGFloat kSelectedBorderCornerRadius = 8.0f;
const CGFloat kSelectedBorderWidth = 4.0f;
}

@implementation TabCollectionTabCell

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self setupSelectedBackgroundView];
  }
  return self;
}

- (void)setupSelectedBackgroundView {
  self.selectedBackgroundView = [[UIView alloc] init];
  self.selectedBackgroundView.backgroundColor = [UIColor blackColor];

  UIView* border = [[UIView alloc] init];
  border.translatesAutoresizingMaskIntoConstraints = NO;
  border.backgroundColor = [UIColor blackColor];
  border.layer.cornerRadius = kSelectedBorderCornerRadius;
  border.layer.borderWidth = kSelectedBorderWidth;
  border.layer.borderColor = self.tintColor.CGColor;
  [self.selectedBackgroundView addSubview:border];
  [NSLayoutConstraint activateConstraints:@[
    [border.topAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.topAnchor
                       constant:-kBorderMargin],
    [border.leadingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.leadingAnchor
                       constant:-kBorderMargin],
    [border.trailingAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.trailingAnchor
                       constant:kBorderMargin],
    [border.bottomAnchor
        constraintEqualToAnchor:self.selectedBackgroundView.bottomAnchor
                       constant:kBorderMargin]
  ]];
}

@end
