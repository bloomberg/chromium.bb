// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const CGFloat kToolbarBackgroundColor = 0xF2F2F2;
const CGFloat kIncognitoToolbarBackgroundColor = 0x505050;

const CGFloat kLocationBarVerticalMargin = 7.0f;
const CGFloat kExpandedLocationBarVerticalMargin = 2.0f;
const CGFloat kButtonVerticalMargin = 4.0f;
const CGFloat kLeadingMarginIPad = 4.0f;
const CGFloat kHorizontalMargin = 1.0f;
const CGFloat kStackViewSpacing = -2.0f;

const CGFloat kLocationBarBorderWidth = 0.33f;
const CGFloat kLocationBarBorderColor = 0xA3A3A3;
const CGFloat kLocationBarCornerRadius = 2.0f;
const CGFloat kLocationBarShadowHeight = 2.0f;
const CGFloat kLocationBarShadowInset = 1.0f;
const CGFloat kIcongnitoLocationBackgroundColor = 0x737373;

const CGFloat kProgressBarHeight = 2.0f;

const CGFloat kToolsMenuButtonWidth = 44.0f;
const CGFloat kToolbarButtonWidth = 48.0f;
const CGFloat kLeadingLocationBarButtonWidth = 40.0f;
const CGFloat kToolbarButtonTitleNormalColor = 0x555555;
const CGFloat kIncognitoLocationBarBorderColor = 0x4C4C4C;
const CGFloat kToolbarButtonTitleHighlightedColor = 0x4285F4;
const CGFloat kIncognitoToolbarButtonTitleNormalColor = 0xEEEEEE;
const CGFloat kIncognitoToolbarButtonTitleHighlightedColor = 0x888a8c;
const CGFloat kBackButtonImageInset = -9;
const CGFloat kForwardButtonImageInset = -7;
const CGFloat kLeadingLocationBarButtonImageInset = 15;

// The priority is not UILayoutPriorityHigh as it should be
// UILayoutPriorityRequired aside from the very small time interval during
// which they are conflicting. Also, it allows to have potentially more
// priorities.
const UILayoutPriority kPrimaryToolbarLeadingButtonPriority =
    UILayoutPriorityRequired;
const UILayoutPriority kPrimaryToolbarTrailingButtonPriority =
    kPrimaryToolbarLeadingButtonPriority - 1;
const UILayoutPriority kSecondaryToolbarButtonPriority =
    kPrimaryToolbarTrailingButtonPriority - 1;

const NSInteger kShowTabStripButtonMaxTabCount = 99;

const LayoutOffset kToolbarButtonAnimationOffset = -10.0;

const CGFloat kAdaptiveToolbarBackgroundColor = 0xE4E4E4;
const CGFloat kAdaptiveLocationBarCornerRadius = 12;
const CGFloat kAdaptiveLocationBackgroundColor = 0xCCCCCC;
const CGFloat kIcongnitoAdaptiveLocationBackgroundColor = 0x6A6A6A;
