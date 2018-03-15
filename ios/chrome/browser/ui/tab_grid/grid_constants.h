// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifier prefix of a grid cell. To reference a specific cell,
// concatenate |kGridCellIdentifierPrefix| with the index of the cell. For
// example, [NSString stringWithFormat:@"%@%d", kGridCellIdentifierPrefix,
// index].
extern NSString* const kGridCellIdentifierPrefix;

// Accessibility identifier for the close button in a grid cell.
extern NSString* const kGridCellCloseButtonIdentifier;

// All kxxxColor constants are RGB values stored in a Hex integer. These will be
// converted into UIColors using the UIColorFromRGB() function, from
// uikit_ui_util.h

// GridCell styling.
// Common colors.
extern const CGFloat kGridCellIconBackgroundColor;
extern const CGFloat kGridCellSnapshotBackgroundColor;
// Light theme colors.
extern const CGFloat kGridLightThemeCellTitleColor;
extern const CGFloat kGridLightThemeCellHeaderColor;
extern const CGFloat kGridLightThemeCellSelectionColor;
extern const CGFloat kGridLightThemeCellCloseButtonTintColor;
// Dark theme colors.
extern const CGFloat kGridDarkThemeCellTitleColor;
extern const CGFloat kGridDarkThemeCellHeaderColor;
extern const CGFloat kGridDarkThemeCellSelectionColor;
extern const CGFloat kGridDarkThemeCellCloseButtonTintColor;

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CONSTANTS_H_
