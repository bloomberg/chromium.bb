// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TOOLS_MENU_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TOOLS_MENU_COMMANDS_H_

// Command protocol for commands related to the ToolsMenu.
@protocol ToolsMenuCommands<NSObject>
@optional
// Shows the tools menu.
- (void)showToolsMenu;

// Closes the tools menu.
- (void)closeToolsMenu;

// Displays the Share menu.
- (void)showShareMenu;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TOOLS_MENU_COMMANDS_H_
