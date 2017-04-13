// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/tab_grid_toolbar.h"

#import "base/mac/foundation_util.h"
#import "ios/clean/chrome/browser/ui/tab_grid/ui_stack_view+cr_tab_grid.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kToolbarHeight = 44.0f;
}

@interface TabGridToolbar ()
@property(nonatomic, weak) UIStackView* toolbarContent;
@end

@implementation TabGridToolbar

@synthesize toolbarContent = _toolbarContent;

- (instancetype)init {
  if (self = [super init]) {
    UIVisualEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* toolbarView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    toolbarView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:toolbarView];

    UIStackView* toolbarContent = [UIStackView cr_tabGridToolbarStackView];
    [toolbarView.contentView addSubview:toolbarContent];
    _toolbarContent = toolbarContent;
    // Sets the stackview to a fixed height, anchored to the bottom of the
    // blur view.
    _toolbarContent.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_toolbarContent.heightAnchor constraintEqualToConstant:kToolbarHeight],
      [_toolbarContent.leadingAnchor
          constraintEqualToAnchor:toolbarView.contentView.leadingAnchor],
      [_toolbarContent.trailingAnchor
          constraintEqualToAnchor:toolbarView.contentView.trailingAnchor],
      [_toolbarContent.bottomAnchor
          constraintEqualToAnchor:toolbarView.contentView.bottomAnchor]
    ]];
  }
  return self;
}

#pragma mark - ZoomTransitionDelegate

- (CGRect)rectForZoomWithKey:(NSObject*)key inView:(UIView*)view {
  UIView* toolsMenuButtonView =
      [self.toolbarContent.arrangedSubviews lastObject];
  return [view convertRect:toolsMenuButtonView.bounds
                  fromView:toolsMenuButtonView];
}

#pragma mark - UIView

// Returns an intrinsic height so that explicit height constraints are
// not necessary. Status bar conditional logic can be set using this
// intrinsic size.
- (CGSize)intrinsicContentSize {
  return CGSizeMake(UIViewNoIntrinsicMetric, kToolbarHeight);
}

@end
