// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TAB_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TAB_CELL_H_

#import <UIKit/UIKit.h>

// Placeholder cell implementation for use in the tab grid.
// A square cell with rounded corners and a label placed in the center.
@interface TabGridTabCell : UICollectionViewCell

// The label in the center of the tab cell.
@property(nonatomic, readonly) UILabel* label;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TAB_CELL_H_
