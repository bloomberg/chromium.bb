// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Accessibility identifier prefix of a grid cell.
NSString* const kGridCellIdentifierPrefix = @"GridCellIdentifierPrefix";

// Accessibility identifier for the close button in a grid cell.
NSString* const kGridCellCloseButtonIdentifier =
    @"GridCellCloseButtonIdentifier";

// GridCell styling.
// Common colors.
const CGFloat kGridCellIconBackgroundColor = 0xF1F3F4;
const CGFloat kGridCellSnapshotBackgroundColor = 0xE8EAED;
// Light theme colors.
const CGFloat kGridLightThemeCellTitleColor = 0x000000;
const CGFloat kGridLightThemeCellHeaderColor = 0xF8F9FA;
const CGFloat kGridLightThemeCellSelectionColor = 0x1A73E8;
const CGFloat kGridLightThemeCellCloseButtonTintColor = 0x3C4043;
// Dark theme colors.
const CGFloat kGridDarkThemeCellTitleColor = 0xFFFFFF;
const CGFloat kGridDarkThemeCellHeaderColor = 0x5F6368;
const CGFloat kGridDarkThemeCellSelectionColor = 0x9AA0A6;
const CGFloat kGridDarkThemeCellCloseButtonTintColor = 0xFFFFFF;
