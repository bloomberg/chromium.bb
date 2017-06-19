// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_STRIP_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_STRIP_COMMANDS_H_

// Commands relating to tab strip UI.
@protocol TabStripCommands
@optional
// Display the tab corresponding to |index|.
- (void)showTabStripTabAtIndex:(int)index;
// Remove tab corresponding to |index|.
- (void)closeTabStripTabAtIndex:(int)index;
// Display the tab strip.
- (void)showTabStrip;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_TAB_STRIP_COMMANDS_H_
