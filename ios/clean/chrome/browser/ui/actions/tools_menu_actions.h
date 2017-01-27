// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_TOOLS_MENU_ACTIONS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_TOOLS_MENU_ACTIONS_H_

#import <Foundation/Foundation.h>

// Target/Action methods relating to the Tools menu  UI.
// (Actions should only be used to communicate into or between the View
// Controller layer).
@protocol ToolsMenuActions

@optional

// Closes the Tools menu. Most menu items should send this message
// in addition to other messages they send.
- (void)closeToolsMenu:(id)sender;

// Displays the Tools menu.
- (void)showToolsMenu:(id)sender;

// Displays the Share menu.
- (void)showShareMenu:(id)sender;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_TOOLS_MENU_ACTIONS_H_
