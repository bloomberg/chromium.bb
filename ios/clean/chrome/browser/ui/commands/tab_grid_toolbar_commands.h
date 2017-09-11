// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_GRID_TOOLBAR_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_GRID_TOOLBAR_COMMANDS_H_

// Command protocol for commands related to the Toolbar displayed in the
// TabGrid.
@protocol TabGridToolbarCommands<NSObject>
// Shows the tools menu.
- (void)showToolsMenu;

// Closes the tools menu.
- (void)closeToolsMenu;

// Toggles the incognito mode.
- (void)toggleIncognito;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_GRID_TOOLBAR_COMMANDS_H_
