// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_PANEL_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_PANEL_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"

namespace ios {
class ChromeBrowserState;
}

@class RecentTabsTableViewController;
@protocol UrlLoader;

// This is the controller for the Recent Tabs panel on the New Tab Page.
// RecentTabsPanelController controls the RecentTabTableViewDataSource, based on
// the user's signed-in and chrome-sync states.
//
// RecentTabsPanelController listens for notifications about Chrome Sync
// and ChromeToDevice and changes/updates the view accordingly.
//
@interface RecentTabsPanelController : NSObject<NewTabPagePanelProtocol>

// Public initializer.
- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState;

// Private initializer, exposed for testing.
- (instancetype)initWithController:(RecentTabsTableViewController*)controller
                      browserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Mark super designated initializer as unavailable.
- (instancetype)initWithNibNamed:(NSString*)nibName NS_UNAVAILABLE;

// Reloads the closed tab list and updates the content of the tableView.
- (void)reloadClosedTabsList;

// Reloads the session data and updates the content of the tableView.
- (void)reloadSessions;

// Sets the tab restore service to null.
- (void)tabRestoreServiceDestroyed;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_PANEL_CONTROLLER_H_
