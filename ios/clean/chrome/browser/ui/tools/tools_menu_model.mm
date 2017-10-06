// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tools/tools_menu_model.h"

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/tools_menu/tools_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/commands/find_in_page_visibility_commands.h"
#import "ios/clean/chrome/browser/ui/commands/settings_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Declare all the possible items. If adding or removing items update
// the value of kToolsMenuNumberOfItems with the new total count.
const MenuModelItem itemsModelList[kToolsMenuNumberOfItems] = {
    // clang-format off
  { IDS_IOS_TOOLS_MENU_NEW_TAB,                     kToolsMenuNewTabId,
    ToolbarTypeAll,                                 ItemVisibleAlways,
    nil,                                            ItemEnabledAlways},
  { IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB,           kToolsMenuNewIncognitoTabId,
    ToolbarTypeAll,                                 ItemVisibleAlways,
    nil,                                            ItemEnabledAlways},
  { IDS_IOS_TOOLS_MENU_CLOSE_ALL_TABS,              kToolsMenuCloseAllTabsId,
    ToolbarTypeSwitcher,                            ItemVisibleNotIncognitoOnly,
    nil,                                            ItemEnabledWhenOpenTabs},
  { IDS_IOS_TOOLS_MENU_CLOSE_ALL_INCOGNITO_TABS,
    kToolsMenuCloseAllIncognitoTabsId,
    ToolbarTypeSwitcher,                            ItemVisibleIncognitoOnly,
    nil,                                            ItemEnabledWhenOpenTabs},
  { IDS_IOS_TOOLS_MENU_BOOKMARKS,                   kToolsMenuBookmarksId,
    ToolbarTypeNone,                                ItemVisibleAlways,
    nil,                                            ItemEnabledAlways},
  { IDS_IOS_TOOLS_MENU_READING_LIST,                kToolsMenuReadingListId,
    ToolbarTypeNone,                                ItemVisibleAlways,
    nil,                                            ItemEnabledAlways},
  { IDS_IOS_TOOLS_MENU_RECENT_TABS,                 kToolsMenuOtherDevicesId,
    ToolbarTypeNone,                                ItemVisibleNotIncognitoOnly,
    nil,                                            ItemEnabledAlways},
  { IDS_HISTORY_SHOW_HISTORY,                       kToolsMenuHistoryId,
    ToolbarTypeWeb,                                 ItemVisibleAlways,
    nil,                                            ItemEnabledAlways},
  { IDS_IOS_OPTIONS_REPORT_AN_ISSUE,                kToolsMenuReportAnIssueId,
    ToolbarTypeAll,                                 ItemVisibleAlways,
    nil,                                            ItemEnabledAlways},
  { IDS_IOS_TOOLS_MENU_FIND_IN_PAGE,                kToolsMenuFindInPageId,
    ToolbarTypeWeb,                                 ItemVisibleAlways,
    NSStringFromSelector(@selector(showFindInPage)),ItemEnabledNotInNTP},
  { IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE,        kToolsMenuRequestDesktopId,
    ToolbarTypeNone,                                ItemVisibleAlways,
    nil,                                            ItemEnabledNotInNTP},
  { IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE,         kToolsMenuRequestMobileId,
    ToolbarTypeNone,                                ItemVisibleAlways,
    nil,                                            ItemEnabledNotInNTP},
  { IDS_IOS_TOOLS_MENU_SETTINGS,                    kToolsMenuSettingsId,
    ToolbarTypeAll,                                 ItemVisibleAlways,
    NSStringFromSelector(@selector(showSettings)),  ItemEnabledAlways},
  { IDS_IOS_TOOLS_MENU_HELP_MOBILE,                 kToolsMenuHelpId,
    ToolbarTypeWeb,                                 ItemVisibleAlways,
    nil,                                            ItemEnabledAlways},
    // clang-format on
};
