// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_GRID_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_GRID_COMMANDS_H_

// Commands relating to tab grid UI.
@protocol TabGridCommands
// Display the tab grid.
- (void)showTabGrid;
// Display the tab corresponding to |index|.
- (void)showTabGridTabAtIndex:(int)index;
// Remove tab corresponding to |index|.
- (void)closeTabGridTabAtIndex:(int)index;
// Create new tab and display it.
- (void)createAndShowNewTabInTabGrid;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_GRID_COMMANDS_H_
