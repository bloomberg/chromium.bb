// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_MENU_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_MENU_CONSTANTS_H_

#import <Foundation/Foundation.h>

// New Tab item accessibility Identifier.
extern NSString* const kToolsMenuNewTabId;
// New incognito Tab item accessibility Identifier.
extern NSString* const kToolsMenuNewIncognitoTabId;
// Close all Tabs item accessibility Identifier.
extern NSString* const kToolsMenuCloseAllTabsId;
// Close all incognito Tabs item accessibility Identifier.
extern NSString* const kToolsMenuCloseAllIncognitoTabsId;
// Bookmarks item accessibility Identifier.
extern NSString* const kToolsMenuBookmarksId;
// Reading List item accessibility Identifier.
extern NSString* const kToolsMenuReadingListId;
// Other Devices item accessibility Identifier.
extern NSString* const kToolsMenuOtherDevicesId;
// History item accessibility Identifier.
extern NSString* const kToolsMenuHistoryId;
// Report an issue item accessibility Identifier.
extern NSString* const kToolsMenuReportAnIssueId;
// Find in Page item accessibility Identifier.
extern NSString* const kToolsMenuFindInPageId;
// Reader Mode item accessibility Identifier.
extern NSString* const kToolsMenuReaderMode;
// Request desktop item accessibility Identifier.
extern NSString* const kToolsMenuRequestDesktopId;
// Settings item accessibility Identifier.
extern NSString* const kToolsMenuSettingsId;
// Help item accessibility Identifier.
extern NSString* const kToolsMenuHelpId;
// Request mobile item accessibility Identifier.
extern NSString* const kToolsMenuRequestMobileId;

// Identifiers for tools menu items (for metrics purposes).
typedef NS_ENUM(int, ToolsMenuItemID) {
  // All of these values must be < 0.
  TOOLS_STOP_ITEM = -1,
  TOOLS_RELOAD_ITEM = -2,
  TOOLS_BOOKMARK_ITEM = -3,
  TOOLS_BOOKMARK_EDIT = -4,
  TOOLS_SHARE_ITEM = -5,
  TOOLS_MENU_ITEM = -6,
  TOOLS_SETTINGS_ITEM = -7,
  TOOLS_NEW_TAB_ITEM = -8,
  TOOLS_NEW_INCOGNITO_TAB_ITEM = -9,
  TOOLS_READING_LIST = -10,
  TOOLS_READER_MODE = -11,
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_MENU_CONSTANTS_H_
