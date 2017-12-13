// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

#include "ios/chrome/browser/ui/rtl_geometry.h"

// All kxxxColor constants are RGB values stored in a Hex integer. These will be
// converted into UIColors using the UIColorFromRGB() function, from
// uikit_ui_util.h

// Toolbar styling.
extern const CGFloat kToolbarBackgroundColor;
extern const CGFloat kIncognitoToolbarBackgroundColor;

// Stackview constraints.
extern const CGFloat kLocationBarVerticalMargin;
extern const CGFloat kExpandedLocationBarVerticalMargin;
extern const CGFloat kButtonVerticalMargin;
extern const CGFloat kLeadingMarginIPad;
extern const CGFloat kHorizontalMargin;
extern const CGFloat kStackViewSpacing;

// Location bar styling.
extern const CGFloat kLocationBarBorderWidth;
extern const CGFloat kLocationBarBorderColor;
extern const CGFloat kIncognitoLocationBarBorderColor;
extern const CGFloat kLocationBarCornerRadius;
extern const CGFloat kLocationBarShadowHeight;
extern const CGFloat kLocationBarShadowInset;
extern const CGFloat kIcongnitoLocationBackgroundColor;

// Progress Bar Height.
extern const CGFloat kProgressBarHeight;

// Toolbar Buttons.
extern const CGFloat kToolsMenuButtonWidth;
extern const CGFloat kToolbarButtonWidth;
extern const CGFloat kLeadingLocationBarButtonWidth;
extern const CGFloat kToolbarButtonTitleNormalColor;
extern const CGFloat kToolbarButtonTitleHighlightedColor;
extern const CGFloat kIncognitoToolbarButtonTitleNormalColor;
extern const CGFloat kIncognitoToolbarButtonTitleHighlightedColor;
extern const CGFloat kBackButtonImageInset;
extern const CGFloat kForwardButtonImageInset;
extern const CGFloat kLeadingLocationBarButtonImageInset;

// Maximum number of tabs displayed by the button containing the tab count.
extern const NSInteger kShowTabStripButtonMaxTabCount;

// Animation constants.
extern const LayoutOffset kToolbarButtonAnimationOffset;

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_CONSTANTS_H_
