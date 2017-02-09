// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_OBSERVERS_SYNC_DISABLE_OBSERVER_H_
#define COMPONENTS_UKM_OBSERVERS_SYNC_DISABLE_OBSERVER_H_

#include <map>

#include "base/scoped_observer.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

namespace ukm {

// Observes the state of a set of SyncServices for changes to history sync
// preferences.  This is for used to trigger purging of local state when
// sync is disabled on a profile and disabling recording when any non-syncing
// profiles are active.
class SyncDisableObserver : public syncer::SyncServiceObserver {
 public:
  SyncDisableObserver();
  ~SyncDisableObserver() override;

  // Starts observing a service for sync disables.
  void ObserveServiceForSyncDisables(syncer::SyncService* sync_service);

  // Returns if history sync is enabled on all active profiles.
  virtual bool IsHistorySyncEnabledOnAllProfiles();

 protected:
  // Called after state changes and some profile has sync disabled.
  // If |must_purge| is true, sync was disabled for some profile, and
  // local data should be purged.
  virtual void OnSyncPrefsChanged(bool must_purge) = 0;

 private:
  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // Recomputes all_profiles_enabled_ state from previous_states_;
  void UpdateAllProfileEnabled(bool must_purge);

  // Returns true iff all profiles are enabled in previous_states_.
  // If there are no profiles being observed, this returns false.
  bool AreAllProfilesEnabled();

  // Tracks observed history services, for cleanup.
  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_;

  // State data about sync services that we need to remember.
  struct SyncState {
    // If the user has history sync enabled.
    bool history_enabled;
    // Whether the sync service has been initialized.
    bool initialized;
    // Whether user data is hidden by a secondary passphrase.
    // This is not valid if the state is not initialized.
    bool passphrase_protected;
  };

  // Gets the current state of a SyncService.
  static SyncState GetSyncState(syncer::SyncService* sync);

  // The list of services that had sync enabled when we last checked.
  std::map<syncer::SyncService*, SyncState> previous_states_;

  // Tracks if sync was enabled on all profiles after the last state change.
  bool all_profiles_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SyncDisableObserver);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_OBSERVERS_SYNC_DISABLE_OBSERVER_H_
