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
const CGFloat kGridLightThemeCellCloseButtonTintColor = 0x3C4043;
const CGFloat kGridDarkThemeCellCloseButtonTintColor = 0xFFFFFF;
