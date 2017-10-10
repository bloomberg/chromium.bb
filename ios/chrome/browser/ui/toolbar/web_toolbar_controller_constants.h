// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import <UIKit/UIKit.h>
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"

// The brightness of the omnibox placeholder text in regular mode,
// on an iPhone.
extern const CGFloat kiPhoneOmniboxPlaceholderColorBrightness;

// The histogram recording CLAuthorizationStatus for omnibox queries.
extern const char* const kOmniboxQueryLocationAuthorizationStatusHistogram;
// The number of possible CLAuthorizationStatus values to report.
const int kLocationAuthorizationStatusCount = 4;

// The brightness of the toolbar's background color (visible on NTPs when the
// background view is hidden).
extern const CGFloat kNTPBackgroundColorBrightness;
extern const CGFloat kNTPBackgroundColorBrightnessIncognito;

// How far below the omnibox to position the popup.
extern const CGFloat kiPadOmniboxPopupVerticalOffset;

// Padding to place on the sides of the omnibox when expanded.
extern const CGFloat kExpandedOmniboxPadding;

// Padding between the back button and the omnibox when the forward button isn't
// displayed.
extern const CGFloat kBackButtonTrailingPadding;
extern const CGFloat kForwardButtonTrailingPadding;
extern const CGFloat kReloadButtonTrailingPadding;

// Cancel button sizing.
extern const CGFloat kCancelButtonBottomMargin;
extern const CGFloat kCancelButtonTopMargin;
extern const CGFloat kCancelButtonLeadingMargin;
extern const CGFloat kCancelButtonWidth;

// Additional offset to adjust the y coordinate of the determinate progress bar
// up by.
extern const CGFloat kDeterminateProgressBarYOffset;
extern const CGFloat kMaterialProgressBarHeight;
extern const CGFloat kLoadCompleteHideProgressBarDelay;
// The default position animation is 10 pixels toward the trailing side, so
// that's a negative leading offset.
extern const LayoutOffset kPositionAnimationLeadingOffset;

extern const CGFloat kIPadToolbarY;
extern const CGFloat kScrollFadeDistance;
// Offset from the image edge to the beginning of the visible omnibox rectangle.
// The image is symmetrical, so the offset is equal on each side.
extern const CGFloat kBackgroundImageVisibleRectOffset;

extern const CGFloat kWebToolbarWidths[INTERFACE_IDIOM_COUNT];
// UI layouts.  iPhone values followed by iPad values.
// Frame for the WebToolbar-specific container, which is a subview of the
// toolbar view itself.
extern const LayoutRect kWebToolbarFrame[INTERFACE_IDIOM_COUNT];

// Layouts inside the WebToolbar frame
extern const LayoutRect kOmniboxFrame[INTERFACE_IDIOM_COUNT];
extern const LayoutRect kBackButtonFrame[INTERFACE_IDIOM_COUNT];
extern const LayoutRect kForwardButtonFrame[INTERFACE_IDIOM_COUNT];

// iPad-only layouts
// Layout for both the stop and reload buttons, which are in the same location.
extern const LayoutRect kStopReloadButtonFrame;
extern const LayoutRect kStarButtonFrame;
extern const LayoutRect kVoiceSearchButtonFrame;

// Vertical distance between the y-coordinate of the omnibox's center and the
// y-coordinate of the web toolbar's center.
extern const CGFloat kOmniboxCenterOffsetY;

// Last button in accessory view for keyboard, commonly used TLD.
extern NSString* const kDotComTLD;

enum {
  WebToolbarButtonNameBack = NumberOfToolbarButtonNames,
  WebToolbarButtonNameCallingApp,
  WebToolbarButtonNameForward,
  WebToolbarButtonNameReload,
  WebToolbarButtonNameStar,
  WebToolbarButtonNameStop,
  WebToolbarButtonNameTTS,
  WebToolbarButtonNameVoice,
  NumberOfWebToolbarButtonNames,
};

// Helper functions.
void RunBlockAfterDelay(NSTimeInterval delay, void (^block)(void));
CGRect RectShiftedDownForStatusBar(CGRect rect);
CGRect RectShiftedUpForStatusBar(CGRect rect);
CGRect RectShiftedUpAndResizedForStatusBar(CGRect rect);
CGRect RectShiftedDownAndResizedForStatusBar(CGRect rect);

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_CONSTANTS_H_
