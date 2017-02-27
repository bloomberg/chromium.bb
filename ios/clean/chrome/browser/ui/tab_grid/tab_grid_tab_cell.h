// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TAB_CELL_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TAB_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_panel_cell.h"

// Cell represents a tab for use in the tab grid. It has a title, favicon,
// screenshot image, and close button. Cell selection is represented by a border
// highlight in the tintColor.
// PLACEHOLDER: Create custom implemementation rather than subclassing.
@interface TabGridTabCell : TabSwitcherLocalSessionCell

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_TAB_CELL_H_
