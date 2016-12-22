// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

// Command protocol for commands relating to tabs.
// (Commands are for communicating into or within the coordinator layer).
@protocol TabCommands
// Display the tab corresponding to |indexPath|. The receiver determines how
// this correspondence relates to the tab model(s) it knows about.
- (void)showTabAtIndexPath:(NSIndexPath*)indexPath;
@end
#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_TAB_COMMANDS_H_
