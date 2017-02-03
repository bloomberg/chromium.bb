// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_SYNCED_SESSIONS_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_SYNCED_SESSIONS_BRIDGE_H_

#import <UIKit/UIKit.h>

#import "base/ios/weak_nsobject.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/driver/sync_service_observer.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/ui/ntp/recent_tabs/synced_sessions_bridge.h"

namespace ios {
class ChromeBrowserState;
}
class SigninManager;

@class RecentTabsPanelController;

@protocol SyncedSessionsObserver<SyncObserverModelBridge>
- (void)reloadSessions;
@end

namespace synced_sessions {

// Bridge class that will notify the panel when the remote sessions content
// change.
class SyncedSessionsObserverBridge : public SyncObserverBridge,
                                     public SigninManagerBase::Observer {
 public:
  SyncedSessionsObserverBridge(id<SyncedSessionsObserver> owner,
                               ios::ChromeBrowserState* browserState);
  ~SyncedSessionsObserverBridge() override;
  // SyncObserverBridge implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;
  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override;
  void OnForeignSessionUpdated(syncer::SyncService* sync) override;
  // SigninManagerBase::Observer implementation.
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;
  // Returns true if the first sync cycle that contains session information is
  // completed. Returns false otherwise.
  bool IsFirstSyncCycleCompleted();

 private:
  base::WeakNSProtocol<id<SyncedSessionsObserver>> owner_;
  SigninManager* signin_manager_;
  syncer::SyncService* sync_service_;
  ScopedObserver<SigninManagerBase, SigninManagerBase::Observer>
      signin_manager_observer_;
  // Stores whether the first sync cycle that contains session information is
  // completed.
  bool first_sync_cycle_is_completed_;
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_SYNCED_SESSIONS_BRIDGE_H_
