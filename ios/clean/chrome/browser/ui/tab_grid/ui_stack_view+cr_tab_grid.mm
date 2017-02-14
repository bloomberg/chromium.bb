// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/ui_stack_view+cr_tab_grid.h"

#import "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/clean/chrome/browser/ui/tab_grid/ui_button+cr_tab_grid.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSpacing = 16.0f;
}

@implementation UIStackView (CRTabGrid)

+ (instancetype)cr_tabGridToolbarStackView {
  UIView* spacerView = [[UIView alloc] init];
  [spacerView setContentHuggingPriority:UILayoutPriorityDefaultLow
                                forAxis:UILayoutConstraintAxisHorizontal];
  NSArray* items = @[
    [UIButton cr_tabGridToolbarDoneButton],
    [UIButton cr_tabGridToolbarIncognitoButton], spacerView,
    [UIButton cr_tabGridToolbarMenuButton]
  ];
  UIStackView* toolbar = [[self alloc] initWithArrangedSubviews:items];
  toolbar.layoutMarginsRelativeArrangement = YES;
  toolbar.layoutMargins = UIEdgeInsetsMakeDirected(0, kSpacing, 0, kSpacing);
  toolbar.spacing = kSpacing;
  toolbar.distribution = UIStackViewDistributionFill;
  return toolbar;
}

@end
