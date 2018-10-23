// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SYNCED_SESSIONS_BRIDGE_H_
#define IOS_CHROME_BROWSER_SYNC_SYNCED_SESSIONS_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/sync/driver/sync_service_observer.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace ios {
class ChromeBrowserState;
}

namespace identity {
class IdentityManager;
}

@protocol SyncedSessionsObserver<SyncObserverModelBridge>
// Reloads the session data.
- (void)reloadSessions;
@end

namespace synced_sessions {

// Bridge class that will notify the panel when the remote sessions content
// change.
class SyncedSessionsObserverBridge
    : public SyncObserverBridge,
      public identity::IdentityManager::Observer {
 public:
  SyncedSessionsObserverBridge(id<SyncedSessionsObserver> owner,
                               ios::ChromeBrowserState* browserState);
  ~SyncedSessionsObserverBridge() override;
  // SyncObserverBridge implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;
  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override;
  void OnForeignSessionUpdated(syncer::SyncService* sync) override;
  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

  // Returns true if the first sync cycle that contains session information is
  // completed. Returns false otherwise.
  bool IsFirstSyncCycleCompleted();
  // Returns true if user is signed in.
  bool IsSignedIn();
  // Returns true if it is undergoing the first sync cycle.
  bool IsSyncing();
  // Check if the first sync cycle is completed.  This keeps
  // IsFirstSyncCycleCompleted() and first_sync_cycle_is_completed_ updated.
  void CheckIfFirstSyncIsCompleted();

 private:
  __weak id<SyncedSessionsObserver> owner_ = nil;
  identity::IdentityManager* identity_manager_ = nullptr;
  syncer::SyncService* sync_service_;
  ios::ChromeBrowserState* browser_state_;
  ScopedObserver<identity::IdentityManager, identity::IdentityManager::Observer>
      identity_manager_observer_;
  // Stores whether the first sync cycle that contains session information is
  // completed.
  bool first_sync_cycle_is_completed_;

  DISALLOW_COPY_AND_ASSIGN(SyncedSessionsObserverBridge);
};

}  // namespace synced_sessions

#endif  // IOS_CHROME_BROWSER_SYNC_SYNCED_SESSIONS_BRIDGE_H_
