// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/history_popup_commands.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"

@class OpenNewTabCommand;
@class ReadingListAddCommand;

// Protocol for commands that will generally be handled by the "current tab",
// which in practice is the BrowserViewController instance displaying the tab.
@protocol BrowserCommands<NSObject,
                          ActivityServiceCommands,
                          QRScannerCommands,
                          TabHistoryPopupCommands>

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

// Shows the Reading List UI.
- (void)showReadingList;

// Asks the active tab to enter into reader mode, presenting a streamlined view
// of the current content.
- (void)switchToReaderMode;

// Preloads voice search on the current BVC.
- (void)preloadVoiceSearch;

// Closes all tabs.
- (void)closeAllTabs;

// Closes all incognito tabs.
- (void)closeAllIncognitoTabs;

#if !defined(NDEBUG)
// Shows the source of the current page.
- (void)viewSource;
#endif

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
