// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_CONSTRAINTS_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_CONSTRAINTS_UI_UTIL_H_

#import <UIKit/UIKit.h>

// UI util for adding constraints.

// General note on the following constraint utility functions:
// Directly adding constraints to views has been deprecated in favor of just
// activating constrainst since iOS8. All of these methods now use
// [NSLayoutConstraint activateConstraints:] for efficiency. The superview
// arguments are thus superfluous, but the methods that use them are retained
// here for backwards compatibility until all downstream code can be updated.

// Applies all |constraints| to views in |subviewsDictionary|.
void ApplyVisualConstraints(NSArray* constraints,
                            NSDictionary* subviewsDictionary);
// Deprecated version:
void ApplyVisualConstraints(NSArray* constraints,
                            NSDictionary* subviewsDictionary,
                            UIView* unused_parentView);

// Applies all |constraints| with |options| to views in |subviewsDictionary|.
void ApplyVisualConstraintsWithOptions(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSLayoutFormatOptions options);
// Deprecated version:
void ApplyVisualConstraintsWithOptions(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSLayoutFormatOptions options,
                                       UIView* unused_parentView);

// Applies all |constraints| with |metrics| to views in |subviewsDictionary|.
void ApplyVisualConstraintsWithMetrics(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSDictionary* metrics);
// Deprecated version:
void ApplyVisualConstraintsWithMetrics(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSDictionary* metrics,
                                       UIView* unused_parentView);

// Applies all |constraints| with |metrics| and |options| to views in
// |subviewsDictionary|.
void ApplyVisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options);
// Deprecated version:
void ApplyVisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options,
    UIView* unused_parentView);

// Returns constraints based on the visual constraints described with
// |constraints| and |metrics| to views in |subviewsDictionary|.
NSArray* VisualConstraintsWithMetrics(NSArray* constraints,
                                      NSDictionary* subviewsDictionary,
                                      NSDictionary* metrics);

// Returns constraints based on the visual constraints described with
// |constraints|, |metrics| and |options| to views in |subviewsDictionary|.
NSArray* VisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options);

// Adds a constraint that |view1| and |view2| are center-aligned horizontally
// and vertically.
void AddSameCenterConstraints(UIView* view1, UIView* view2);

// Adds a constraint that |view1| and |view2| are center-aligned horizontally.
// |view1| and |view2| must be in the same view hierarchy.
void AddSameCenterXConstraint(UIView* view1, UIView* view2);
// Deprecated version:
void AddSameCenterXConstraint(UIView* unused_parentView,
                              UIView* subview1,
                              UIView* subview2);

// Adds a constraint that |view1| and |view2| are center-aligned vertically.
// |view1| and |view2| must be in the same view hierarchy.
void AddSameCenterYConstraint(UIView* view1, UIView* view2);
// Deprecated version:
void AddSameCenterYConstraint(UIView* unused_parentView,
                              UIView* subview1,
                              UIView* subview2);

// Adds constraints to make two views' size and center equal by pinning leading,
// trailing, top and bottom anchors.
void AddSameConstraints(UIView* view1, UIView* view2);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_CONSTRAINTS_UI_UTIL_H_
