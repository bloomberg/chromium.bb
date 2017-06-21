// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_menu_model.h"

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"

// Declare all the possible items.
const MenuModelItem itemsModelList[] = {
    // clang-format off
  { IDS_IOS_TOOLS_MENU_NEW_TAB,               kToolsMenuNewTabId,
    ToolbarTypeAll,                           ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB,     kToolsMenuNewIncognitoTabId,
    ToolbarTypeAll,                           ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_CLOSE_ALL_TABS,        kToolsMenuCloseAllTabsId,
    ToolbarTypeSwitcher,                      ItemVisibleNotIncognitoOnly,
    nil },
  { IDS_IOS_TOOLS_MENU_CLOSE_ALL_INCOGNITO_TABS,
    kToolsMenuCloseAllIncognitoTabsId,
    ToolbarTypeSwitcher,                      ItemVisibleIncognitoOnly,
    nil },
  { IDS_IOS_TOOLS_MENU_BOOKMARKS,             kToolsMenuBookmarksId,
    ToolbarTypeNone,                          ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_READING_LIST,          kToolsMenuReadingListId,
    ToolbarTypeNone,                          ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_SUGGESTIONS,           kToolsMenuSuggestionsId,
    ToolbarTypeNone,                          ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_RECENT_TABS,           kToolsMenuOtherDevicesId,
    ToolbarTypeNone,                          ItemVisibleNotIncognitoOnly,
    nil },
  { IDS_HISTORY_SHOW_HISTORY,                 kToolsMenuHistoryId,
    ToolbarTypeWeb,                           ItemVisibleAlways,
    nil },
  { IDS_IOS_OPTIONS_REPORT_AN_ISSUE,          kToolsMenuReportAnIssueId,
    ToolbarTypeAll,                           ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_FIND_IN_PAGE,          kToolsMenuFindInPageId,
    ToolbarTypeWeb,                           ItemVisibleAlways,
    NSStringFromSelector(@selector(showFindInPage)) },
  { IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE,  kToolsMenuRequestDesktopId,
    ToolbarTypeNone,                          ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE,   kToolsMenuRequestMobileId,
    ToolbarTypeNone,                          ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_READER_MODE,           kToolsMenuReaderMode,
    ToolbarTypeNone,                          ItemVisibleAlways,
    nil },
  { IDS_IOS_TOOLS_MENU_SETTINGS,              kToolsMenuSettingsId,
    ToolbarTypeAll,                           ItemVisibleAlways,
    NSStringFromSelector(@selector(showSettings)) },
  { IDS_IOS_TOOLS_MENU_HELP_MOBILE,           kToolsMenuHelpId,
    ToolbarTypeWeb,                           ItemVisibleAlways,
    nil },
    // clang-format on
};
