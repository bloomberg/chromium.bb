// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_

#import <UIKit/UIKit.h>

// Creates a blank button in fallback style, for the given |action| and
// |target|.
UIButton* CreateButtonWithSelectorAndTarget(SEL action, id target);

// Sets vertical constraints on firstBaselineAnchor for the button or label rows
// in |views| inside |container| starting at its topAnchor. Returns the applied
// constrainst to allow caller to deactivate them later.
NSArray<NSLayoutConstraint*>* VerticalConstraintsSpacingForViewsInContainer(
    NSArray<UIView*>* views,
    UIView* container);

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
