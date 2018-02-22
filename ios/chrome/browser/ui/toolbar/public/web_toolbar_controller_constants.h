// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_CONSTANTS_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/ui/rtl_geometry.h"

// The brightness of the omnibox placeholder text in regular mode,
// on an iPhone.
extern const CGFloat kiPhoneOmniboxPlaceholderColorBrightness;

// The histogram recording CLAuthorizationStatus for omnibox queries.
extern const char* const kOmniboxQueryLocationAuthorizationStatusHistogram;
// The number of possible CLAuthorizationStatus values to report.
const int kLocationAuthorizationStatusCount = 4;

// The brightness of the toolbar's background color (visible on NTPs when the
// background view is hidden).
extern const CGFloat kNTPBackgroundColorBrightnessIncognito;

// How far below the omnibox to position the popup.
extern const CGFloat kiPadOmniboxPopupVerticalOffset;

// The default position animation is 10 pixels toward the trailing side, so
// that's a negative leading offset.
extern const LayoutOffset kPositionAnimationLeadingOffset;

extern const CGFloat kIPadToolbarY;
extern const CGFloat kScrollFadeDistance;

// Last button in accessory view for keyboard, commonly used TLD.
extern NSString* const kDotComTLD;

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_CONSTANTS_H_
