// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_collapsible_item.h"

#include "base/numerics/math_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Chevron asset for expand/collapse.
NSString* const kTableViewCellChevron = @"table_view_cell_chevron";
// Rotation to apply to the chevron to point down.
constexpr CGFloat kPointingDownAngle = (90 / 180.0) * M_PI;
// Rotation to apply to the chevron to point up.
constexpr CGFloat kPointingUpAngle = -(90 / 180.0) * M_PI;
// Duration of the chevron rotation.
const NSTimeInterval kChevronFlipDuration = .15f;

}  // namespace

@interface SettingsCollapsibleCell ()

// Animator that handles all cell animations.
@property(nonatomic, strong) UIViewPropertyAnimator* cellAnimator;

@end

@implementation SettingsCollapsibleItem

@synthesize collapsed = _collapsed;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SettingsCollapsibleCell class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureCell:(SettingsCollapsibleCell*)cell {
  [super configureCell:cell];
  cell.accessoryView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:kTableViewCellChevron]];
  [cell setCollapsed:self.collapsed animated:NO];
}

@end

@implementation SettingsCollapsibleCell

@synthesize collapsed = _collapsed;
@synthesize cellAnimator = _cellAnimator;

- (void)prepareForReuse {
  [super prepareForReuse];
  if (self.cellAnimator.isRunning) {
    [self.cellAnimator stopAnimation:YES];
    self.cellAnimator = nil;
  }
}

- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated {
  _collapsed = collapsed;
  __weak SettingsCollapsibleCell* weakSelf = self;
  void (^rotateChevron)(void) = ^{
    CGFloat angle = self.collapsed ? kPointingUpAngle : kPointingDownAngle;
    weakSelf.accessoryView.transform =
        CGAffineTransformRotate(CGAffineTransformIdentity, angle);
  };
  if (!animated) {
    rotateChevron();
  } else {
    self.cellAnimator = [[UIViewPropertyAnimator alloc]
        initWithDuration:kChevronFlipDuration
                   curve:UIViewAnimationCurveLinear
              animations:rotateChevron];
    [self.cellAnimator startAnimation];
  }
}

@end
