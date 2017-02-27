// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_tab_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSelectedBorderCornerRadius = 8.0f;
const CGFloat kSelectedBorderWidth = 4.0f;
}

@implementation TabGridTabCell

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.selectedBackgroundView = [[UIView alloc] init];
    self.selectedBackgroundView.backgroundColor = [UIColor blackColor];
    self.selectedBackgroundView.layer.cornerRadius =
        kSelectedBorderCornerRadius;
    self.selectedBackgroundView.layer.borderWidth = kSelectedBorderWidth;
    self.selectedBackgroundView.layer.borderColor = self.tintColor.CGColor;
    self.selectedBackgroundView.transform = CGAffineTransformScale(
        self.selectedBackgroundView.transform, 1.08, 1.08);
  }
  return self;
}

@end
