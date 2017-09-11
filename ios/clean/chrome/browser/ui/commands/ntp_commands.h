// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_NTP_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_NTP_COMMANDS_H_

// Command protocol for commands relating to the Commands UI.
// (Commands are for communicating into or within the coordinator layer).
@protocol NTPCommands
// Displays a Incognito panel inside the NTP.
- (void)showNTPIncognitoPanel;
// Displays a Chrome Home panel inside the NTP.
- (void)showNTPHomePanel;
// Displays a bookmarks panel inside the NTP on iPad or present a bookmarks
// panel on iPhone.
- (void)showNTPBookmarksPanel;
// Displays a recent tabs panel inside the NTP on iPad or present a bookmarks
// panel on iPhone.
- (void)showNTPRecentTabsPanel;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_NTP_COMMANDS_H_
