// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_

#import <UIKit/UIKit.h>

namespace {

// The multiplier for the base system spacing at the top margin.
static const CGFloat TopSystemSpacingMultiplier = 1.58;

// The multiplier for the base system spacing between elements (vertical).
static const CGFloat MiddleSystemSpacingMultiplier = 1.83;

// The multiplier for the base system spacing at the bottom margin.
static const CGFloat BottomSystemSpacingMultiplier = 2.26;

// Top and bottom margins for buttons.
static const CGFloat ButtonVerticalMargin = 12;

// Left and right margins of the cell content and buttons.
static const CGFloat ButtonHorizontalMargin = 16;

}  // namespace

// Creates a blank button in fallback style, for the given |action| and
// |target|.
UIButton* CreateButtonWithSelectorAndTarget(SEL action, id target);

// Sets vertical constraints on firstBaselineAnchor for the button or label rows
// in |views| inside |container| starting at its topAnchor. Returns the applied
// constrainst to allow caller to deactivate them later.  Defaults multipliers
// are applied.
NSArray<NSLayoutConstraint*>* VerticalConstraintsSpacingForViewsInContainer(
    NSArray<UIView*>* views,
    UIView* container);

// Sets vertical constraints on firstBaselineAnchor for the button or label rows
// in |views| inside |container| starting at its topAnchor. Returns the applied
// constrainst to allow caller to deactivate them later.
NSArray<NSLayoutConstraint*>*
VerticalConstraintsSpacingForViewsInContainerWithMultipliers(
    NSArray<UIView*>* views,
    UIView* container,
    CGFloat topSystemSpacingMultiplier,
    CGFloat middleSystemSpacingMultiplier,
    CGFloat BottomSystemSpacingMultiplier);

// Sets constraints for the given |views|, so as to lay them out horizontally,
// parallel to the given |guide| view, and applying the given constant |shift|
// to the whole row. Returns the applied constraints to allow caller to
// deactivate them later.
NSArray<NSLayoutConstraint*>* HorizontalConstraintsForViewsOnGuideWithShift(
    NSArray<UIView*>* views,
    UIView* guide,
    CGFloat shift);

// Sets all baseline anchors for the gievn |views| to match the one on |onView|.
// Returns the applied constrainst to allow caller to deactivate them later.
NSArray<NSLayoutConstraint*>* SyncBaselinesForViewsOnView(
    NSArray<UIView*>* views,
    UIView* onView);

// Creates a blank label with autoresize mask off and adjustable font size.
UILabel* CreateLabel();

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
