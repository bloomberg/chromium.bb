// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/all_status.h"

#include <algorithm>

#include "base/logging.h"
#include "base/port.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

AllStatus::AllStatus() {
  status_.summary = sync_api::SyncManager::Status::OFFLINE;
  status_.initial_sync_ended = true;
  status_.notifications_enabled = false;
  status_.cryptographer_ready = false;
  status_.crypto_has_pending_keys = false;
}

AllStatus::~AllStatus() {
}

sync_api::SyncManager::Status AllStatus::CreateBlankStatus() const {
  // Status is initialized with the previous status value.  Variables
  // whose values accumulate (e.g. lifetime counters like updates_received)
  // are not to be cleared here.
  sync_api::SyncManager::Status status = status_;
  status.syncing = true;
  status.unsynced_count = 0;
  status.conflicting_count = 0;
  status.initial_sync_ended = false;
  status.syncer_stuck = false;
  status.max_consecutive_errors = 0;
  status.server_broken = false;
  status.updates_available = 0;
  return status;
}

sync_api::SyncManager::Status AllStatus::CalcSyncing(
    const SyncEngineEvent &event) const {
  sync_api::SyncManager::Status status = CreateBlankStatus();
  const sessions::SyncSessionSnapshot* snapshot = event.snapshot;
  status.unsynced_count += static_cast<int>(snapshot->unsynced_count);
  status.conflicting_count += snapshot->errors.num_conflicting_commits;
  // The syncer may not be done yet, which could cause conflicting updates.
  // But this is only used for status, so it is better to have visibility.
  status.conflicting_count += snapshot->num_conflicting_updates;

  status.syncing |= snapshot->syncer_status.sync_in_progress;
  status.syncing =
      snapshot->has_more_to_sync && snapshot->is_silenced;
  status.initial_sync_ended |= snapshot->is_share_usable;
  status.syncer_stuck |= snapshot->syncer_status.syncer_stuck;

  const sessions::ErrorCounters& errors(snapshot->errors);
  if (errors.consecutive_errors > status.max_consecutive_errors)
    status.max_consecutive_errors = errors.consecutive_errors;

  // 100 is an arbitrary limit.
  if (errors.consecutive_transient_error_commits > 100)
    status.server_broken = true;

  status.updates_available += snapshot->num_server_changes_remaining;

  status.sync_protocol_error = snapshot->errors.sync_protocol_error;

  // Accumulate update count only once per session to avoid double-counting.
  // TODO(ncarter): Make this realtime by having the syncer_status
  // counter preserve its value across sessions.  http://crbug.com/26339
  if (event.what_happened == SyncEngineEvent::SYNC_CYCLE_ENDED) {
    status.updates_received +=
        snapshot->syncer_status.num_updates_downloaded_total;
    status.tombstone_updates_received +=
        snapshot->syncer_status.num_tombstone_updates_downloaded_total;
    status.num_local_overwrites_total +=
        snapshot->syncer_status.num_local_overwrites;
    status.num_server_overwrites_total +=
        snapshot->syncer_status.num_server_overwrites;
    if (snapshot->syncer_status.num_updates_downloaded_total == 0) {
      ++status.empty_get_updates;
    } else {
      ++status.nonempty_get_updates;
    }
    if (snapshot->syncer_status.num_successful_commits == 0 &&
        snapshot->syncer_status.num_updates_downloaded_total == 0) {
      ++status.useless_sync_cycles;
    } else {
      ++status.useful_sync_cycles;
    }
  }
  return status;
}

void AllStatus::CalcStatusChanges() {
  const bool unsynced_changes = status_.unsynced_count > 0;
  const bool online = status_.authenticated &&
    status_.server_reachable && status_.server_up && !status_.server_broken;
  if (online) {
    if (status_.syncer_stuck)
      status_.summary = sync_api::SyncManager::Status::CONFLICT;
    else if (unsynced_changes || status_.syncing)
      status_.summary = sync_api::SyncManager::Status::SYNCING;
    else
      status_.summary = sync_api::SyncManager::Status::READY;
  } else if (!status_.initial_sync_ended) {
    status_.summary = sync_api::SyncManager::Status::OFFLINE_UNUSABLE;
  } else if (unsynced_changes) {
    status_.summary = sync_api::SyncManager::Status::OFFLINE_UNSYNCED;
  } else {
    status_.summary = sync_api::SyncManager::Status::OFFLINE;
  }
}

void AllStatus::OnSyncEngineEvent(const SyncEngineEvent& event) {
  ScopedStatusLock lock(this);
  switch (event.what_happened) {
    case SyncEngineEvent::SYNC_CYCLE_ENDED:
    case SyncEngineEvent::STATUS_CHANGED:
      status_ = CalcSyncing(event);
      break;
    case SyncEngineEvent::STOP_SYNCING_PERMANENTLY:
    case SyncEngineEvent::UPDATED_TOKEN:
    case SyncEngineEvent::CLEAR_SERVER_DATA_FAILED:
    case SyncEngineEvent::CLEAR_SERVER_DATA_SUCCEEDED:
       break;
    case SyncEngineEvent::ACTIONABLE_ERROR:
      status_ = CreateBlankStatus();
      status_.sync_protocol_error = event.snapshot->errors.sync_protocol_error;
      break;
    default:
      LOG(ERROR) << "Unrecognized Syncer Event: " << event.what_happened;
      break;
  }
}

void AllStatus::HandleServerConnectionEvent(
    const ServerConnectionEvent& event) {
  ScopedStatusLock lock(this);
  status_.server_up = IsGoodReplyFromServer(event.connection_code);
  status_.server_reachable = event.server_reachable;

  if (event.connection_code == HttpResponse::SERVER_CONNECTION_OK) {
    status_.authenticated = true;
  } else {
    status_.authenticated = false;
  }
}

sync_api::SyncManager::Status AllStatus::status() const {
  base::AutoLock lock(mutex_);
  return status_;
}

void AllStatus::SetNotificationsEnabled(bool notifications_enabled) {
  ScopedStatusLock lock(this);
  status_.notifications_enabled = notifications_enabled;
}

void AllStatus::IncrementNotifiableCommits() {
  ScopedStatusLock lock(this);
  ++status_.notifiable_commits;
}

void AllStatus::IncrementNotificationsReceived() {
  ScopedStatusLock lock(this);
  ++status_.notifications_received;
}

void AllStatus::SetEncryptedTypes(const syncable::ModelTypeSet& types) {
  ScopedStatusLock lock(this);
  status_.encrypted_types = types;
}

void AllStatus::SetCryptographerReady(bool ready) {
  ScopedStatusLock lock(this);
  status_.cryptographer_ready = ready;
}

void AllStatus::SetCryptoHasPendingKeys(bool has_pending_keys) {
  ScopedStatusLock lock(this);
  status_.crypto_has_pending_keys = has_pending_keys;
}

ScopedStatusLock::ScopedStatusLock(AllStatus* allstatus)
    : allstatus_(allstatus) {
  allstatus->mutex_.Acquire();
}

ScopedStatusLock::~ScopedStatusLock() {
  allstatus_->CalcStatusChanges();
  allstatus_->mutex_.Release();
}

}  // namespace browser_sync
