// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_BRIDGES_H_
#define IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_BRIDGES_H_

#import <UIKit/UIKit.h>

#import "base/ios/weak_nsobject.h"
#include "base/macros.h"
#include "components/sessions/core/tab_restore_service_observer.h"

@class RecentTabsPanelController;

namespace recent_tabs {

// Bridge class to forward events from the sessions::TabRestoreService to
// Objective-C class RecentTabsPanelController.
class ClosedTabsObserverBridge : public sessions::TabRestoreServiceObserver {
 public:
  explicit ClosedTabsObserverBridge(RecentTabsPanelController* owner);
  ~ClosedTabsObserverBridge() override;

  // sessions::TabRestoreServiceObserver implementation.
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  base::WeakNSObject<RecentTabsPanelController> owner_;

  DISALLOW_COPY_AND_ASSIGN(ClosedTabsObserverBridge);
};

}  // namespace recent_tabs

#endif  // IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_BRIDGES_H_
