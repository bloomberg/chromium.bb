// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The multiplier for the base system spacing at the top margin.
static const CGFloat TopSystemSpacingMultiplier = 1.58;

// The multiplier for the base system spacing between elements (vertical).
static const CGFloat MiddleSystemSpacingMultiplier = 1.83;

// The multiplier for the base system spacing at the bottom margin.
static const CGFloat BottomSystemSpacingMultiplier = 2.26;

}  // namespace

UIButton* CreateButtonWithSelectorAndTarget(SEL action, id target) {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitleColor:UIColor.cr_manualFillTintColor
               forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

void HorizontalConstraintsMarginForButtonWithWidth(UIButton* button,
                                                   CGFloat width) {
  [NSLayoutConstraint activateConstraints:@[
    [button.leadingAnchor
        constraintEqualToAnchor:button.titleLabel.leadingAnchor
                       constant:-width],
    [button.trailingAnchor
        constraintEqualToAnchor:button.titleLabel.trailingAnchor
                       constant:width],
  ]];
}

NSArray<NSLayoutConstraint*>* VerticalConstraintsSpacingForViewsInContainer(
    NSArray<UIView*>* views,
    UIView* container) {
  NSMutableArray* verticalConstraints = [[NSMutableArray alloc] init];

  // Multipliers of these constraints are calculated based on a 24 base
  // system spacing.
  NSLayoutYAxisAnchor* previousAnchor = container.topAnchor;
  CGFloat multiplier = TopSystemSpacingMultiplier;
  for (UIView* view in views) {
    [verticalConstraints
        addObject:[view.firstBaselineAnchor
                      constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                     multiplier:multiplier]];
    multiplier = MiddleSystemSpacingMultiplier;
    previousAnchor = view.lastBaselineAnchor;
  }
  multiplier = BottomSystemSpacingMultiplier;
  [verticalConstraints
      addObject:[container.bottomAnchor
                    constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                   multiplier:multiplier]];

  [NSLayoutConstraint activateConstraints:verticalConstraints];
  return verticalConstraints;
}

NSArray<NSLayoutConstraint*>* HorizontalConstraintsForViewsOnGuideWithShift(
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat shift) {
  NSMutableArray* horizontalConstraints = [[NSMutableArray alloc] init];
  NSLayoutXAxisAnchor* previousAnchor = guide.leadingAnchor;
  for (UIView* view in views) {
    [horizontalConstraints
        addObject:[view.leadingAnchor constraintEqualToAnchor:previousAnchor
                                                     constant:shift]];
    previousAnchor = view.trailingAnchor;
    shift = 0;
  }
  if (views.count > 0) {
    [horizontalConstraints
        addObject:[views.lastObject.trailingAnchor
                      constraintLessThanOrEqualToAnchor:guide.trailingAnchor
                                               constant:shift]];
  }
  [NSLayoutConstraint activateConstraints:horizontalConstraints];
  return horizontalConstraints;
}

NSArray<NSLayoutConstraint*>* SyncBaselinesForViewsOnView(
    NSArray<UIView*>* views,
    UIView* onView) {
  NSMutableArray* baselinesConstraints = [[NSMutableArray alloc] init];
  for (UIView* view in views) {
    [baselinesConstraints
        addObject:[view.firstBaselineAnchor
                      constraintEqualToAnchor:onView.firstBaselineAnchor]];
  }
  [NSLayoutConstraint activateConstraints:baselinesConstraints];
  return baselinesConstraints;
}

UILabel* CreateLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  return label;
}
