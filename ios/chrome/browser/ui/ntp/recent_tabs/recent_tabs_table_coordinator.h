// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_TABLE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_TABLE_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol BrowserCommands;

@interface RecentTabsTableCoordinator : ChromeCoordinator

// The dispatcher for this Coordinator.
@property(nonatomic, weak) id<BrowserCommands> dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_TABLE_COORDINATOR_H_
