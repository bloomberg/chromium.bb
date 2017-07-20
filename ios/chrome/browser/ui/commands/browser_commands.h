// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_

#import <Foundation/Foundation.h>

@class OpenNewTabCommand;
@class ReadingListAddCommand;

// Protocol for commands that will generally be handled by the "current tab",
// which in practice is the BrowserViewController instance displaying the tab.
@protocol BrowserCommands<NSObject>

// Closes the current tab.
- (void)closeCurrentTab;

// Navigates backwards in the current tab's history.
- (void)goBack;

// Navigates forwards in the current tab's history.
- (void)goForward;

// Stops loading the current web page.
- (void)stopLoading;

// Reloads the current web page
- (void)reload;

// Shows the share sheet for the current page.
- (void)sharePage;

// Bookmarks the current page.
- (void)bookmarkPage;

// Shows the tools menu.
- (void)showToolsMenu;

// Opens a new tab as specified by |newTabCommand|.
- (void)openNewTab:(OpenNewTabCommand*)newTabCommand;

// Prints the currently active tab.
- (void)printTab;

// Adds a page to the reading list using data in |command|.
- (void)addToReadingList:(ReadingListAddCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
