// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_bridges.h"

#import "ios/chrome/browser/ui/ntp/recent_tabs/recent_tabs_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace recent_tabs {

#pragma mark - ClosedTabsObserverBridge

ClosedTabsObserverBridge::ClosedTabsObserverBridge(
    RecentTabsTableCoordinator* owner)
    : owner_(owner) {}

ClosedTabsObserverBridge::~ClosedTabsObserverBridge() {}

void ClosedTabsObserverBridge::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  [owner_ reloadClosedTabsList];
}

void ClosedTabsObserverBridge::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  [owner_ tabRestoreServiceDestroyed];
}

}  // namespace recent_tabs
