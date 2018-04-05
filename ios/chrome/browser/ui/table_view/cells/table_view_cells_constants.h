// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The spacing between views inside of a cell.
extern const CGFloat kTableViewCellViewSpacing;

// The vertical spacing between text labels in a stackView.
extern const CGFloat kTableViewVerticalLabelStackSpacing;

// Animation Duration for highlighting selected section header.
extern const CGFloat kTableViewCellSelectionAnimationDuration;

// Color and alpha used to highlight a cell with a middle gray color to
// represent a user tap.
extern const CGFloat kTableViewHighlightedCellColor;
extern const CGFloat kTableViewHighlightedCellColorAlpha;

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_
