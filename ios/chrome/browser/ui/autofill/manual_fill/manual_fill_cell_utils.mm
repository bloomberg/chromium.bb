// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_button.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/uicolor_manualfill.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIButton* CreateButtonWithSelectorAndTarget(SEL action, id target) {
  UIButton* button = [ManualFillCellButton buttonWithType:UIButtonTypeCustom];
  [button setTitleColor:UIColor.cr_manualFillTintColor
               forState:UIControlStateNormal];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.contentHorizontalAlignment =
      UIControlContentHorizontalAlignmentLeading;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  button.contentEdgeInsets =
      UIEdgeInsetsMake(ButtonVerticalMargin, ButtonHorizontalMargin,
                       ButtonVerticalMargin, ButtonHorizontalMargin);
  return button;
}

NSArray<NSLayoutConstraint*>* VerticalConstraintsSpacingForViewsInContainer(
    NSArray<UIView*>* views,
    UIView* container) {
  return VerticalConstraintsSpacingForViewsInContainerWithMultipliers(
      views, container, TopSystemSpacingMultiplier,
      MiddleSystemSpacingMultiplier, BottomSystemSpacingMultiplier);
}

NSArray<NSLayoutConstraint*>*
VerticalConstraintsSpacingForViewsInContainerWithMultipliers(
    NSArray<UIView*>* views,
    UIView* container,
    CGFloat topSystemSpacingMultiplier,
    CGFloat middleSystemSpacingMultiplier,
    CGFloat bottomSystemSpacingMultiplier) {
  NSMutableArray* verticalConstraints = [[NSMutableArray alloc] init];

  // Multipliers of these constraints are calculated based on a 24 base
  // system spacing.
  NSLayoutYAxisAnchor* previousAnchor = container.topAnchor;
  CGFloat multiplier = topSystemSpacingMultiplier;
  for (UIView* view in views) {
    [verticalConstraints
        addObject:[view.firstBaselineAnchor
                      constraintEqualToSystemSpacingBelowAnchor:previousAnchor
                                                     multiplier:multiplier]];
    multiplier = middleSystemSpacingMultiplier;
    previousAnchor = view.lastBaselineAnchor;
  }
  multiplier = bottomSystemSpacingMultiplier;
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
    [view setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
    [view setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisHorizontal];
    previousAnchor = view.trailingAnchor;
    shift = 0;
  }
  if (views.count > 0) {
    [horizontalConstraints
        addObject:[views.lastObject.trailingAnchor
                      constraintEqualToAnchor:guide.trailingAnchor
                                     constant:shift]];
    // Give all remaining space to the last button, as per UX.
    [views.lastObject
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [views.lastObject
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
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

UIView* CreateGraySeparatorForContainer(UIView* container) {
  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = UIColor.cr_manualFillGrayLineColor;
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:grayLine];

  id<LayoutGuideProvider> safeArea = container.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [grayLine.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    [grayLine.heightAnchor constraintEqualToConstant:1],
    // Horizontal constraints.
    [grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor
                                           constant:ButtonHorizontalMargin],
    [safeArea.trailingAnchor constraintEqualToAnchor:grayLine.trailingAnchor
                                            constant:ButtonHorizontalMargin],
  ]];

  return grayLine;
}
