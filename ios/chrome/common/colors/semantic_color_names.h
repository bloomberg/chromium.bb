// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_COLORS_SEMANTIC_COLOR_NAMES_H_
#define IOS_CHROME_COMMON_COLORS_SEMANTIC_COLOR_NAMES_H_

#import <UIKit/UIKit.h>

// Element Colors

extern NSString* const kBackgroundColor;
extern NSString* const kDisabledTintColor;
// Background color used in the rounded squares behind favicons.
extern NSString* const kFaviconBackgroundColor;
extern NSString* const kMDCInkColor;
extern NSString* const kScrimBackgroundColor;
extern NSString* const kSolidButtonTextColor;
extern NSString* const kTableViewRowHighlightColor;
extern NSString* const kTextPrimaryColor;
extern NSString* const kTextSecondaryColor;
extern NSString* const kTextfieldBackgroundColor;
extern NSString* const kTextfieldPlaceholderColor;

// Standard Colors

// Standard blue color. This is most commonly used for the tint color on
// standard buttons and controls.
extern NSString* const kBlueColor;
// Standard green color.
extern NSString* const kGreenColor;
// Standard red color. This is most commonly used for the tint color on
// destructive controls.
extern NSString* const kRedColor;

// Temporary colors for iOS 12. Because overridePreferredInterfaceStyle isn't
// available in iOS 12, any views that should always be dark (e.g. incognito)
// need to use colorsets that always use the dark variant.
// TODO(crbug.com/981889): Clean up after iOS 12 support is dropped.

extern NSString* const kBackgroundDarkColor;
extern NSString* const kTableViewRowHighlightDarkColor;
extern NSString* const kTextPrimaryDarkColor;
extern NSString* const kTextfieldBackgroundDarkColor;
extern NSString* const kTextfieldPlaceholderDarkColor;

extern NSString* const kBlueDarkColor;
extern NSString* const kGreenDarkColor;
extern NSString* const kRedDarkColor;

#endif  // IOS_CHROME_COMMON_COLORS_SEMANTIC_COLOR_NAMES_H_
