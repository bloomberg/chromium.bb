// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_panel_controller.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/sessions_sync_user_state.h"

namespace sessions {
class TabRestoreService;
}

namespace ios {
class ChromeBrowserState;
}

@protocol RecentTabsTableViewControllerDelegate<NSObject>
// Tells the delegate when the table view content scrolled or changed size.
- (void)recentTabsTableViewContentMoved:(UITableView*)tableView;
@end

// Controls the content of a UITableView.
//
// The UITableView can contain the following different sections:
// A/ Closed tabs section.
//   This section lists all the local tabs that were recently closed.
// A*/ Separator section.
//   This section contains only a single cell that acts as a separator.
// B/ Other Devices section.
//   Depending on the user state, the section will either contain a view
//   offering the user to sign in, a view to activate sync, or a view to inform
//   the user that they can turn on sync on other devices.
// C/ Session section.
//   This section shows the sessions from other devices.
//
// Section A is always present, followed by section A*.
// Depending on the user sync state, either section B or section C will be
// presented.
@interface RecentTabsTableViewController
    : UITableViewController<UIGestureRecognizerDelegate>

@property(nonatomic, assign) id<RecentTabsTableViewControllerDelegate>
    delegate;  // weak

// Designated initializer. The controller opens link with |loader|.
// |browserState|
// and |loader| must not be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              loader:(id<UrlLoader>)loader;

// Refreshes the table view to match the current sync state.
- (void)refreshUserState:(SessionsSyncUserState)state;

// Refreshes the recently closed tab section.
- (void)refreshRecentlyClosedTabs;

// Sets the service used to populate the closed tab section. Can be used to nil
// the service in case it is not available anymore.
- (void)setTabRestoreService:(sessions::TabRestoreService*)tabRestoreService;

// Sets whether scroll to top is enabled.
- (void)setScrollsToTop:(BOOL)enabled;

// Dismisses any outstanding modal user interface elements.
- (void)dismissModals;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_TABLE_VIEW_CONTROLLER_H_
