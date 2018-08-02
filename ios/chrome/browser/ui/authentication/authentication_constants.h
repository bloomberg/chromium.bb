// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Header image sizes.
extern const CGFloat kAuthenticationHeaderImageHeight;
extern const CGFloat kAuthenticationHeaderImageWidth;

// Font sizes
extern const CGFloat kAuthenticationTitleFontSize;
extern const CGFloat kAuthenticationTextFontSize;

// Color displayed in the non-safe area.
extern const int kAuthenticationHeaderBackgroundColor;
// Horizontal margin between the container view and any elements inside.
extern const CGFloat kAuthenticationHorizontalMargin;
// Vertical margin between the header image and the main title.
extern const CGFloat kAuthenticationHeaderTitleMargin;

// Alpha for the title color.
extern const CGFloat kAuthenticationTitleColorAlpha;
// Alpha for the text color.
extern const CGFloat kAuthenticationTextColorAlpha;

// Header image name.
extern NSString* const kAuthenticationHeaderImageName;

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_CONSTANTS_H_
