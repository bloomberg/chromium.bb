// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/constraints_ui_util.h"

#import <UIKit/UIKit.h>

#include "base/ios/ios_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void ApplyVisualConstraints(NSArray* constraints,
                            NSDictionary* subviewsDictionary) {
  ApplyVisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                              nil, 0);
}

void ApplyVisualConstraints(NSArray* constraints,
                            NSDictionary* subviewsDictionary,
                            UIView* unused_parentView) {
  ApplyVisualConstraints(constraints, subviewsDictionary);
}

void ApplyVisualConstraintsWithOptions(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSLayoutFormatOptions options) {
  ApplyVisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                              nil, options);
}

void ApplyVisualConstraintsWithOptions(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSLayoutFormatOptions options,
                                       UIView* unused_parentView) {
  ApplyVisualConstraintsWithOptions(constraints, subviewsDictionary, options);
}

void ApplyVisualConstraintsWithMetrics(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSDictionary* metrics) {
  ApplyVisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                              metrics, 0);
}

void ApplyVisualConstraintsWithMetrics(NSArray* constraints,
                                       NSDictionary* subviewsDictionary,
                                       NSDictionary* metrics,
                                       UIView* unused_parentView) {
  ApplyVisualConstraintsWithMetrics(constraints, subviewsDictionary, metrics);
}

void ApplyVisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options) {
  NSArray* layoutConstraints = VisualConstraintsWithMetricsAndOptions(
      constraints, subviewsDictionary, metrics, options);
  [NSLayoutConstraint activateConstraints:layoutConstraints];
}

void ApplyVisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options,
    UIView* unused_parentView) {
  ApplyVisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                              metrics, options);
}

NSArray* VisualConstraintsWithMetrics(NSArray* constraints,
                                      NSDictionary* subviewsDictionary,
                                      NSDictionary* metrics) {
  return VisualConstraintsWithMetricsAndOptions(constraints, subviewsDictionary,
                                                metrics, 0);
}

NSArray* VisualConstraintsWithMetricsAndOptions(
    NSArray* constraints,
    NSDictionary* subviewsDictionary,
    NSDictionary* metrics,
    NSLayoutFormatOptions options) {
  NSMutableArray* layoutConstraints = [NSMutableArray array];
  for (NSString* constraint in constraints) {
    DCHECK([constraint isKindOfClass:[NSString class]]);
    [layoutConstraints addObjectsFromArray:
                           [NSLayoutConstraint
                               constraintsWithVisualFormat:constraint
                                                   options:options
                                                   metrics:metrics
                                                     views:subviewsDictionary]];
  }
  return [layoutConstraints copy];
}

void AddSameCenterConstraints(UIView* view1, UIView* view2) {
  AddSameCenterXConstraint(view1, view2);
  AddSameCenterYConstraint(view1, view2);
}

void AddSameCenterXConstraint(UIView* view1, UIView* view2) {
  [view1.centerXAnchor constraintEqualToAnchor:view2.centerXAnchor].active =
      YES;
}

void AddSameCenterXConstraint(UIView* unused_parentView,
                              UIView* subview1,
                              UIView* subview2) {
  AddSameCenterXConstraint(subview1, subview2);
}

void AddSameCenterYConstraint(UIView* view1, UIView* view2) {
  [view1.centerYAnchor constraintEqualToAnchor:view2.centerYAnchor].active =
      YES;
}

void AddSameCenterYConstraint(UIView* unused_parentView,
                              UIView* subview1,
                              UIView* subview2) {
  AddSameCenterYConstraint(subview1, subview2);
}

void AddSameConstraints(UIView* view1, UIView* view2) {
  [NSLayoutConstraint activateConstraints:@[
    [view1.leadingAnchor constraintEqualToAnchor:view2.leadingAnchor],
    [view1.trailingAnchor constraintEqualToAnchor:view2.trailingAnchor],
    [view1.topAnchor constraintEqualToAnchor:view2.topAnchor],
    [view1.bottomAnchor constraintEqualToAnchor:view2.bottomAnchor]
  ]];
}
