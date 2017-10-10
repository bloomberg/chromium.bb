// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSTANTS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// All kxxxColor constants are RGB values stored in a Hex integer. These will be
// converted into UIColors using the UIColorFromRGB() function, from
// uikit_ui_util.h

// Toolbar styling.
extern const CGFloat kToolbarBackgroundColor;
extern const CGFloat kIncognitoToolbarBackgroundColor;

// Stackview constraints.
extern const CGFloat kVerticalMargin;
extern const CGFloat kHorizontalMargin;
extern const CGFloat kStackViewSpacing;

// Location bar styling.
extern const CGFloat kLocationBarBorderWidth;
extern const CGFloat kLocationBarBorderColor;
extern const CGFloat kIncognitoLocationBarBorderColor;
extern const CGFloat kLocationBarShadowRadius;
extern const CGFloat kLocationBarShadowOpacity;
extern const CGFloat kIcongnitoLocationBackgroundColor;

// Progress Bar Height.
extern const CGFloat kProgressBarHeight;

// Toolbar Buttons.
extern const CGFloat kToolbarButtonWidth;
extern const CGFloat kToolbarButtonTitleNormalColor;
extern const CGFloat kToolbarButtonTitleHighlightedColor;
extern const CGFloat kIncognitoToolbarButtonTitleNormalColor;
extern const CGFloat kIncognitoToolbarButtonTitleHighlightedColor;

// Maximum number of tabs displayed by the button containing the tab count.
extern const NSInteger kShowTabStripButtonMaxTabCount;

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONSTANTS_H_
