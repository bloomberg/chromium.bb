// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/observers/sync_disable_observer.h"

#include "base/stl_util.h"

namespace ukm {

SyncDisableObserver::SyncDisableObserver()
    : sync_observer_(this),
      all_histories_enabled_(false),
      all_extensions_enabled_(false) {}

SyncDisableObserver::~SyncDisableObserver() {}

// static
SyncDisableObserver::SyncState SyncDisableObserver::GetSyncState(
    syncer::SyncService* sync_service) {
  SyncState state;
  state.history_enabled = sync_service->GetPreferredDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES);
  state.extensions_enabled =
      sync_service->GetPreferredDataTypes().Has(syncer::EXTENSIONS);
  state.initialized = sync_service->IsEngineInitialized();
  state.passphrase_protected =
      state.initialized && sync_service->IsUsingSecondaryPassphrase();
  return state;
}

void SyncDisableObserver::ObserveServiceForSyncDisables(
    syncer::SyncService* sync_service) {
  previous_states_[sync_service] = GetSyncState(sync_service);
  sync_observer_.Add(sync_service);
  UpdateAllProfileEnabled(false);
}

void SyncDisableObserver::UpdateAllProfileEnabled(bool must_purge) {
  bool all_enabled = CheckHistorySyncOnAllProfiles();
  bool all_extensions_enabled =
      all_enabled && CheckExtensionSyncOnAllProfiles();
  // Any change in sync settings needs to call OnSyncPrefsChanged so that the
  // new settings take effect.
  if (must_purge || (all_enabled != all_histories_enabled_) ||
      (all_extensions_enabled != all_extensions_enabled_)) {
    all_histories_enabled_ = all_enabled;
    all_extensions_enabled_ = all_extensions_enabled;
    OnSyncPrefsChanged(must_purge);
  }
}

bool SyncDisableObserver::CheckHistorySyncOnAllProfiles() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const SyncDisableObserver::SyncState& state = kv.second;
    if (!state.history_enabled || !state.initialized ||
        state.passphrase_protected)
      return false;
  }
  return true;
}

bool SyncDisableObserver::CheckExtensionSyncOnAllProfiles() {
  if (previous_states_.empty())
    return false;
  for (const auto& kv : previous_states_) {
    const SyncDisableObserver::SyncState& state = kv.second;
    if (!state.extensions_enabled)
      return false;
  }
  return true;
}

void SyncDisableObserver::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(base::ContainsKey(previous_states_, sync));
  SyncDisableObserver::SyncState state = GetSyncState(sync);
  const SyncDisableObserver::SyncState& previous_state = previous_states_[sync];
  bool must_purge =
      // Trigger a purge if history sync was disabled.
      (previous_state.history_enabled && !state.history_enabled) ||
      // Trigger a purge if engine has become disabled.
      (previous_state.initialized && !state.initialized) ||
      // Trigger a purge if the user added a passphrase.  Since we can't detect
      // the use of a passphrase while the engine is not initialized, we may
      // miss the transition if the user adds a passphrase in this state.
      (previous_state.initialized && state.initialized &&
       !previous_state.passphrase_protected && state.passphrase_protected);
  previous_states_[sync] = state;
  UpdateAllProfileEnabled(must_purge);
}

void SyncDisableObserver::OnSyncShutdown(syncer::SyncService* sync) {
  DCHECK(base::ContainsKey(previous_states_, sync));
  sync_observer_.Remove(sync);
  previous_states_.erase(sync);
  UpdateAllProfileEnabled(false);
}

bool SyncDisableObserver::IsHistorySyncEnabledOnAllProfiles() {
  return all_histories_enabled_;
}

bool SyncDisableObserver::IsExtensionSyncEnabledOnAllProfiles() {
  return all_extensions_enabled_;
}

}  // namespace ukm
