// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_VISIBILITY_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_VISIBILITY_COMMANDS_H_

// Command protocol for commands relating to showing and hiding Find In Page.
@protocol FindInPageVisibilityCommands
// Show the find in page UI.
- (void)showFindInPage;

// Hide the find in page UI.
- (void)hideFindInPage;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_VISIBILITY_COMMANDS_H_
