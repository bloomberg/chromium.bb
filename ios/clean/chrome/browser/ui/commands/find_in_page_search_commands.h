// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_SEARCH_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_SEARCH_COMMANDS_H_

// Command protocol for commands relating to Find In Page.
@protocol FindInPageSearchCommands

// Searches for the given string in the current page.
- (void)findStringInPage:(NSString*)searchTerm;

// Finds the previous and next matches, using the current search string. Shows
// the Find in Page UI without initiating a search if there is no current search
// string.
- (void)findNextInPage;
- (void)findPreviousInPage;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_FIND_IN_PAGE_SEARCH_COMMANDS_H_
