// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

// Command protocol for commands relating to tabs.
// (Commands are for communicating into or within the coordinator layer).
@protocol TabCommands
// Display the tab corresponding to |index|.
- (void)showTabAtIndex:(int)index;

// Remove tab corresponding to |index|.
- (void)closeTabAtIndex:(int)index;

// Create new tab and display it.
- (void)createAndShowNewTab;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_
