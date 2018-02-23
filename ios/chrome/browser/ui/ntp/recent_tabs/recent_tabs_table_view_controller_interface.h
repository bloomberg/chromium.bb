// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/recent_tabs/sessions_sync_user_state.h"

namespace ios {
class ChromeBrowserState;
}

namespace sessions {
class TabRestoreService;
}

@protocol ApplicationCommands;
@protocol RecentTabsHandsetViewControllerCommand;
@protocol LegacyRecentTabsTableViewControllerDelegate;
@protocol UrlLoader;

// RecentTabs TableViewController public interface.
@protocol RecentTabsTableViewControllerInterface<NSObject>

// Refreshes the table view to match the current sync state.
- (void)refreshUserState:(SessionsSyncUserState)state;

// Refreshes the recently closed tab section.
- (void)refreshRecentlyClosedTabs;

// Sets the service used to populate the closed tab section. Can be used to nil
// the service in case it is not available anymore.
- (void)setTabRestoreService:(sessions::TabRestoreService*)tabRestoreService;

// Dismisses any outstanding modal user interface elements.
- (void)dismissModals;

// RecentTabsTableViewControllerDelegate delegate.
@property(nonatomic, weak) id<LegacyRecentTabsTableViewControllerDelegate>
    delegate;

// RecentTabsHandsetViewControllerCommand delegate.
@property(nonatomic, weak) id<RecentTabsHandsetViewControllerCommand>
    handsetCommandHandler;

@end
