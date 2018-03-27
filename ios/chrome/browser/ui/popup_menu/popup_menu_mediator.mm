// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_mediator.h"

#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/cells/popup_menu_tools_item.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_table_view_controller.h"
#include "ios/chrome/browser/ui/tools_menu/public/tools_menu_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/user_agent.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
PopupMenuToolsItem* CreateTableViewItem(int titleID,
                                        PopupMenuAction action,
                                        NSString* accessibilityID) {
  PopupMenuToolsItem* item =
      [[PopupMenuToolsItem alloc] initWithType:kItemTypeEnumZero];
  item.title = l10n_util::GetNSString(titleID);
  item.actionIdentifier = action;
  item.accessibilityIdentifier = accessibilityID;
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
// Array containing all the nonnull items/
@property(nonatomic, strong) NSArray<TableViewItem*>* specificItems;

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
@synthesize specificItems = _specificItems;

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
        self.items = @[ [self itemsForNewTab] ];
        break;
      case PopupMenuTypeSearch:
        break;
    }
    NSMutableArray* specificItems = [NSMutableArray array];
    if (self.reloadStop)
      [specificItems addObject:self.reloadStop];
    if (self.readLater)
      [specificItems addObject:self.readLater];
    if (self.findInPage)
      [specificItems addObject:self.findInPage];
    if (self.siteInformation)
      [specificItems addObject:self.siteInformation];
    self.specificItems = specificItems;
  }
  return _items;
}

#pragma mark - Popup updates (Private)

// Updates the popup menu to have its state in sync with the current page
// status.
- (void)updatePopupMenu {
  // TODO(crbug.com/804773): update the items to take into account the state.
  self.readLater.enabled = [self isWebURL];
  self.findInPage.enabled = [self isFindInPageEnabled];
  if ([self isPageLoading]) {
    self.reloadStop.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_STOP);
    self.reloadStop.actionIdentifier = PopupMenuActionStop;
    self.reloadStop.accessibilityIdentifier = kToolsMenuStop;
  } else {
    self.reloadStop.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_RELOAD);
    self.reloadStop.actionIdentifier = PopupMenuActionReload;
    self.reloadStop.accessibilityIdentifier = kToolsMenuReload;
  }

  // Reload the items.
  [self.popupMenu reconfigureCellsForItems:self.specificItems];
}

// Whether the actions associated with the share menu can be enabled.
- (BOOL)isWebURL {
  if (!self.webState)
    return NO;
  const GURL& URL = self.webState->GetLastCommittedURL();
  return URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
}

// Whether find in page is enabled.
- (BOOL)isFindInPageEnabled {
  if (!self.webState)
    return NO;
  auto* helper = FindTabHelper::FromWebState(self.webState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

// Whether the page is currently loading.
- (BOOL)isPageLoading {
  if (!self.webState)
    return NO;
  return self.webState->IsLoading();
}

#pragma mark - Item creation (Private)

// Creates the menu items for the tools menu.
- (void)createToolsMenuItem {
  // Reload or stop page action, created as reload.
  self.reloadStop = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_RELOAD, PopupMenuActionReload, kToolsMenuReload);

  NSArray* tabActions = [@[ self.reloadStop ]
      arrayByAddingObjectsFromArray:[self itemsForNewTab]];

  NSArray* browserActions = [self actionItems];

  NSArray* collectionActions = [self collectionItems];

  self.items = @[ tabActions, browserActions, collectionActions ];
}

- (NSArray<TableViewItem*>*)itemsForNewTab {
  // Open New Tab.
  TableViewItem* openNewTabItem =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_NEW_TAB, PopupMenuActionOpenNewTab,
                          kToolsMenuNewTabId);

  // Open New Incogntio Tab.
  TableViewItem* openNewIncognitoTabItem = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB, PopupMenuActionOpenNewIncognitoTab,
      kToolsMenuNewIncognitoTabId);

  return @[ openNewTabItem, openNewIncognitoTabItem ];
}

- (NSArray<TableViewItem*>*)actionItems {
  NSMutableArray* actionsArray = [NSMutableArray array];
  // Read Later.
  self.readLater =
      CreateTableViewItem(IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST,
                          PopupMenuActionReadLater, kToolsMenuReadLater);
  [actionsArray addObject:self.readLater];

  // Find in Pad.
  self.findInPage =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_FIND_IN_PAGE,
                          PopupMenuActionFindInPage, kToolsMenuFindInPageId);
  [actionsArray addObject:self.findInPage];

  if ([self userAgentType] != web::UserAgentType::DESKTOP) {
    // Request Desktop Site.
    PopupMenuToolsItem* requestDesktopSite = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE, PopupMenuActionRequestDesktop,
        kToolsMenuRequestDesktopId);
    // Disable the action if the user agent is not mobile.
    requestDesktopSite.enabled =
        [self userAgentType] == web::UserAgentType::MOBILE;
    [actionsArray addObject:requestDesktopSite];
  } else {
    // Request Mobile Site.
    TableViewItem* requestMobileSite = CreateTableViewItem(
        IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE, PopupMenuActionRequestMobile,
        kToolsMenuRequestMobileId);
    [actionsArray addObject:requestMobileSite];
  }

  // Site Information.
  self.siteInformation = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_SITE_INFORMATION, PopupMenuActionSiteInformation,
      kToolsMenuSiteInformation);
  [actionsArray addObject:self.siteInformation];

  // Report an Issue.
  if (ios::GetChromeBrowserProvider()
          ->GetUserFeedbackProvider()
          ->IsUserFeedbackEnabled()) {
    TableViewItem* reportIssue = CreateTableViewItem(
        IDS_IOS_OPTIONS_REPORT_AN_ISSUE, PopupMenuActionReportIssue,
        kToolsMenuReportAnIssueId);
    [actionsArray addObject:reportIssue];
  }

  // Help.
  TableViewItem* help = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_HELP_MOBILE, PopupMenuActionHelp, kToolsMenuHelpId);
  [actionsArray addObject:help];

  return actionsArray;
}

- (NSArray<TableViewItem*>*)collectionItems {
  // Bookmarks.
  TableViewItem* bookmarks =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_BOOKMARKS,
                          PopupMenuActionBookmarks, kToolsMenuBookmarksId);

  // Reading List.
  TableViewItem* readingList =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_READING_LIST,
                          PopupMenuActionReadingList, kToolsMenuReadingListId);

  // Recent Tabs.
  TableViewItem* recentTabs =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_RECENT_TABS,
                          PopupMenuActionRecentTabs, kToolsMenuOtherDevicesId);

  // History.
  TableViewItem* history = CreateTableViewItem(
      IDS_IOS_TOOLS_MENU_HISTORY, PopupMenuActionHistory, kToolsMenuHistoryId);

  // Settings.
  TableViewItem* settings =
      CreateTableViewItem(IDS_IOS_TOOLS_MENU_SETTINGS, PopupMenuActionSettings,
                          kToolsMenuSettingsId);

  return @[ bookmarks, readingList, recentTabs, history, settings ];
}

// Returns the UserAgentType currently in use.
- (web::UserAgentType)userAgentType {
  if (!self.webState)
    return web::UserAgentType::NONE;
  web::NavigationItem* visibleItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem)
    return web::UserAgentType::NONE;

  return visibleItem->GetUserAgentType();
}

@end
