// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
TableViewItem<PopupMenuItem>* CreateTableViewItem(int titleID,
                                                  PopupMenuAction action) {
  PopupMenuToolsItem* item =
      [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
  item.title = l10n_util::GetNSString(titleID);
  item.actionIdentifier = action;
  return item;
}
}

@interface PopupMenuMediator ()

// Items to be displayed in the popup menu.
@property(nonatomic, strong)
    NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>* items;

// Type of this mediator.
@property(nonatomic, assign) PopupMenuType type;

@end

@implementation PopupMenuMediator

@synthesize items = _items;
@synthesize type = _type;

#pragma mark - Public

- (instancetype)initWithType:(PopupMenuType)type {
  self = [super init];
  if (self) {
    _type = type;
  }
  return self;
}

- (void)setUp {
  switch (self.type) {
    case PopupMenuTypeToolsMenu:
      [self createToolsMenuItem];
      break;
    case PopupMenuTypeNavigationForward:
      break;
    case PopupMenuTypeNavigationBackward:
      break;
    case PopupMenuTypeTabGrid:
      break;
    case PopupMenuTypeSearch:
      break;
  }
}

- (void)configurePopupMenu:(PopupMenuTableViewController*)popupMenu {
  [popupMenu setPopupMenuItems:self.items];
}

#pragma mark - Private

// Creates the menu items for the tools menu.
- (void)createToolsMenuItem {
  // Reload page action.
  TableViewItem* reload =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_RELOAD, PopupMenuActionReload);

  NSArray* tabActions =
      [@[ reload ] arrayByAddingObjectsFromArray:[self itemsForNewTab]];

  NSArray* browserActions = [self actionItems];

  NSArray* collectionActions = [self collectionItems];

  self.items = @[ tabActions, browserActions, collectionActions ];
}

- (NSArray<TableViewItem*>*)itemsForNewTab {
  // Open New Tab.
  TableViewItem* openNewTabItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_TAB, PopupMenuActionOpenNewTab);

  // Open New Incogntio Tab.
  TableViewItem* openNewIncognitoTabItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, PopupMenuActionOpenNewIncognitoTab);

  return @[ openNewTabItem, openNewIncognitoTabItem ];
}

- (NSArray<TableViewItem*>*)actionItems {
  // Read Later.
  TableViewItem* readLater = CreateTableViewItem(
      IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST, PopupMenuActionReadLater);

  // Request Desktop Site.
  TableViewItem* requestDesktopSite = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE, PopupMenuActionRequestDesktop);

  // Request Mobile Site.
  TableViewItem* requestMobileSite = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE, PopupMenuActionRequestMobile);

  // Site Information.
  TableViewItem* siteInformation = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_SITE_INFORMATION, PopupMenuActionSiteInformation);

  // Report an Issue.
  TableViewItem* reportIssue = CreateTableViewItem(
      IDS_IOS_OPTIONS_REPORT_AN_ISSUE, PopupMenuActionReportIssue);

  // Help.
  TableViewItem* help =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_HELP_MOBILE, PopupMenuActionHelp);

  return @[
    readLater, requestDesktopSite, requestMobileSite, siteInformation,
    reportIssue, help
  ];
}

- (NSArray<TableViewItem*>*)collectionItems {
  // Bookmarks.
  TableViewItem* bookmarks = CreateTableViewItem(IDS_IOS_TOOLS_MENU_BOOKMARKS,
                                                 PopupMenuActionBookmarks);

  // Reading List.
  TableViewItem* readingList = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_READING_LIST, PopupMenuActionReadingList);

  // Recent Tabs.
  TableViewItem* recentTabs = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_RECENT_TABS, PopupMenuActionRecentTabs);

  // History.
  TableViewItem* history =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_HISTORY, PopupMenuActionHistory);

  // Settings.
  TableViewItem* settings =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_SETTINGS, PopupMenuActionSettings);

  return @[ bookmarks, readingList, recentTabs, history, settings ];
}

@end
