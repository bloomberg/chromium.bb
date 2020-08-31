// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_ALL_STATUS_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_ALL_STATUS_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/engine_impl/nudge_source.h"
#include "components/sync/engine_impl/sync_engine_event_listener.h"
#include "components/sync/engine_impl/syncer_types.h"

namespace syncer {

class SyncStatusObserver;
struct SyncCycleEvent;

// This class watches various sync engine components, updating its internal
// state upon change. It can send to observers a snapshot of this state as a
// SyncStatus object, aggregating all this data into one place.
//
// Most of this data ends up on the about:sync page.  But the page is only
// 'pinged' to update itself at the end of a sync cycle.  A user could refresh
// manually, but unless their timing is excellent it's unlikely that a user will
// see any state in mid-sync cycle.  We have no plans to change this.  However,
// we will continue to collect data and update state mid-sync-cycle in case we
// need to debug slow or stuck sync cycles.
class AllStatus : public SyncEngineEventListener {
 public:
  AllStatus();
  ~AllStatus() override;

  // Adds an observer which will receive a call whenever this object changes.
  // The |observer| gets notification after being added right away. |observer|
  // must not be null and must outlive this object.
  void AddObserver(SyncStatusObserver* observer);

  // SyncEngineEventListener implementation.
  void OnSyncCycleEvent(const SyncCycleEvent& event) override;
  void OnActionableError(const SyncProtocolError& error) override;
  void OnRetryTimeChanged(base::Time retry_time) override;
  void OnThrottledTypesChanged(ModelTypeSet throttled_types) override;
  void OnBackedOffTypesChanged(ModelTypeSet backed_off_types) override;
  void OnMigrationRequested(ModelTypeSet types) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;

  void SetNotificationsEnabled(bool notifications_enabled);

  void IncrementNotificationsReceived();

  void SetEncryptedTypes(ModelTypeSet types);
  void SetCryptographerCanEncrypt(bool can_encrypt);
  void SetCryptoHasPendingKeys(bool has_pending_keys);
  void SetPassphraseType(PassphraseType type);
  void SetHasKeystoreKey(bool has_keystore_key);
  void SetKeystoreMigrationTime(const base::Time& migration_time);

  void SetSyncId(const std::string& sync_id);
  void SetInvalidatorClientId(const std::string& invalidator_client_id);

  void SetLocalBackendFolder(const std::string& folder);

 protected:
  // Examines syncer to calculate syncing and the unsynced count,
  // and returns a Status with new values.
  SyncStatus CalcSyncing(const SyncCycleEvent& event) const;
  SyncStatus CreateBlankStatus() const;

  SyncStatus status_;

 private:
  friend class ScopedStatusLock;

  // Notifies all observers about changing of status.
  void NotifyStatusChanged();

  std::vector<SyncStatusObserver*> sync_status_observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AllStatus);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_ALL_STATUS_H_
