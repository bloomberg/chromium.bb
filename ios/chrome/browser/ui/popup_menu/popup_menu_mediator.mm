// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
PopupMenuToolsItem* CreateTableViewItem(int titleID, PopupMenuAction action) {
  PopupMenuToolsItem* item =
      [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
  item.title = l10n_util::GetNSString(titleID);
  item.actionIdentifier = action;
  return item;
}
}

@interface PopupMenuMediator () {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

// Items to be displayed in the popup menu.
@property(nonatomic, strong)
    NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>* items;

// Type of this mediator.
@property(nonatomic, assign) PopupMenuType type;

// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

#pragma mark*** Specific Items ***

@property(nonatomic, strong) PopupMenuToolsItem* reloadStop;
@property(nonatomic, strong) PopupMenuToolsItem* readLater;
@property(nonatomic, strong) PopupMenuToolsItem* findInPage;
@property(nonatomic, strong) PopupMenuToolsItem* siteInformation;

@end

@implementation PopupMenuMediator

@synthesize items = _items;
@synthesize popupMenu = _popupMenu;
@synthesize type = _type;
@synthesize webState = _webState;
@synthesize webStateList = _webStateList;
@synthesize reloadStop = _reloadStop;
@synthesize readLater = _readLater;
@synthesize findInPage = _findInPage;
@synthesize siteInformation = _siteInformation;

#pragma mark - Public

- (instancetype)initWithType:(PopupMenuType)type {
  self = [super init];
  if (self) {
    _type = type;
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updatePopupMenu];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  self.webState = nullptr;
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  DCHECK_EQ(_webStateList, webStateList);
  self.webState = newWebState;
}

#pragma mark - Properties

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    if (self.popupMenu) {
      [self updatePopupMenu];
    }
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;
  self.webState = nil;

  if (_webStateList) {
    self.webState = self.webStateList->GetActiveWebState();
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

- (void)setPopupMenu:(PopupMenuTableViewController*)popupMenu {
  _popupMenu = popupMenu;
  [_popupMenu setPopupMenuItems:self.items];
  if (self.webState) {
    [self updatePopupMenu];
  }
}

- (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items {
  if (!_items) {
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
  return _items;
}

#pragma mark - Popup updates (Private)

// Updates the popup menu to have its state in sync with the current page
// status.
- (void)updatePopupMenu {
  // TODO(crbug.com/804773): update the items to take into account the state.
  // TODO(crbug.com/804773): update the popupMenu cells associated with those
  // items.
}

#pragma mark - Item creation (Private)

// Creates the menu items for the tools menu.
- (void)createToolsMenuItem {
  // Reload page action.
  self.reloadStop =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_RELOAD, PopupMenuActionReload);

  NSArray* tabActions = [@[ self.reloadStop ]
      arrayByAddingObjectsFromArray:[self itemsForNewTab]];

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
  self.readLater = CreateTableViewItem(IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST,
                                       PopupMenuActionReadLater);

  // Find in Pad.
  self.findInPage = CreateTableViewItem(IDS_IOS_TOOLS_MENU_FIND_IN_PAGE,
                                        PopupMenuActionFindInPage);

  // Request Desktop Site.
  // TODO(crbug.com/804773): display only if the user agent is correct.
  TableViewItem* requestDesktopSite = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE, PopupMenuActionRequestDesktop);

  // Request Mobile Site.
  // TODO(crbug.com/804773): display only if the user agent is correct.
  TableViewItem* requestMobileSite = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE, PopupMenuActionRequestMobile);

  // Site Information.
  self.siteInformation = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_SITE_INFORMATION, PopupMenuActionSiteInformation);

  // Report an Issue.
  TableViewItem* reportIssue = CreateTableViewItem(
      IDS_IOS_OPTIONS_REPORT_AN_ISSUE, PopupMenuActionReportIssue);

  // Help.
  TableViewItem* help =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_HELP_MOBILE, PopupMenuActionHelp);

  return @[
    self.readLater, self.findInPage, requestDesktopSite, requestMobileSite,
    self.siteInformation, reportIssue, help
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
