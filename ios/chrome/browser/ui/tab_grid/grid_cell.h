// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CELL_H_

#import <UIKit/UIKit.h>

@class GridItem;

// Theme describing the look of the cell.
typedef NS_ENUM(NSUInteger, GridCellTheme) {
  GridCellThemeInvalid = 0,
  GridCellThemeLight,
  GridCellThemeDark,
};

// A square-ish cell in a grid. Contains an icon, title, snapshot, and close
// button.
@interface GridCell : UICollectionViewCell

// Configures cell with contents of |item|. |theme| specifies the look of the
// cell.
- (void)configureWithItem:(GridItem*)item theme:(GridCellTheme)theme;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_CELL_H_
