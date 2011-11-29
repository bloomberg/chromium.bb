// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/internal_api/debug_info_event_listener.h"

using browser_sync::sessions::SyncSessionSnapshot;
namespace sync_api {

DebugInfoEventListener::DebugInfoEventListener()
    : events_dropped_(false) {
}

DebugInfoEventListener::~DebugInfoEventListener() {
}

void DebugInfoEventListener::OnSyncCycleCompleted(
    const SyncSessionSnapshot* snapshot) {
  if (!snapshot)
    return;

  sync_pb::DebugEventInfo event_info;
  sync_pb::SyncCycleCompletedEventInfo* sync_completed_event_info =
      event_info.mutable_sync_cycle_completed_event_info();

  sync_completed_event_info->set_syncer_stuck(
      snapshot->syncer_status.syncer_stuck);
  sync_completed_event_info->set_num_blocking_conflicts(
      snapshot->num_conflicting_updates);
  sync_completed_event_info->set_num_non_blocking_conflicts(
      snapshot->num_blocking_conflicting_updates);

  AddEventToQueue(event_info);
}

void DebugInfoEventListener::OnInitializationComplete(
    const browser_sync::WeakHandle<browser_sync::JsBackend>& js_backend,
    bool success) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::INITIALIZATION_COMPLETE);
}

void DebugInfoEventListener::OnAuthError(
    const GoogleServiceAuthError& auth_error) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::AUTH_ERROR);
}

void DebugInfoEventListener::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::PASSPHRASE_REQUIRED);
}

void DebugInfoEventListener::OnPassphraseAccepted(
    const std::string& bootstrap_token) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::PASSPHRASE_ACCEPTED);
}

void DebugInfoEventListener::OnStopSyncingPermanently() {
  CreateAndAddEvent(sync_pb::DebugEventInfo::STOP_SYNCING_PERMANENTLY);
}

void DebugInfoEventListener::OnUpdatedToken(const std::string& token) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::UPDATED_TOKEN);
}

void DebugInfoEventListener::OnClearServerDataFailed() {
  // This command is not implemented on the client side.
  NOTREACHED();
}

void DebugInfoEventListener::OnClearServerDataSucceeded() {
  // This command is not implemented on the client side.
  NOTREACHED();
}

void DebugInfoEventListener::OnEncryptedTypesChanged(
    const syncable::ModelTypeSet& encrypted_types,
    bool encrypt_everything) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::ENCRYPTED_TYPES_CHANGED);
}

void DebugInfoEventListener::OnEncryptionComplete() {
  CreateAndAddEvent(sync_pb::DebugEventInfo::ENCRYPTION_COMPLETE);
}

void DebugInfoEventListener::OnActionableError(
    const browser_sync::SyncProtocolError& sync_error) {
  CreateAndAddEvent(sync_pb::DebugEventInfo::ACTIONABLE_ERROR);
}

void DebugInfoEventListener::GetAndClearDebugInfo(
    sync_pb::DebugInfo* debug_info) {
  DCHECK(events_.size() <= sync_api::kMaxEntries);
  while (!events_.empty()) {
    sync_pb::DebugEventInfo* event_info = debug_info->add_events();
    const sync_pb::DebugEventInfo& debug_event_info = events_.front();
    event_info->CopyFrom(debug_event_info);
    events_.pop();
  }

  debug_info->set_events_dropped(events_dropped_);

  events_dropped_ = false;
}

void DebugInfoEventListener::CreateAndAddEvent(
    sync_pb::DebugEventInfo::EventType type) {
  sync_pb::DebugEventInfo event_info;
  event_info.set_type(type);
  AddEventToQueue(event_info);
}

void DebugInfoEventListener::AddEventToQueue(
  const sync_pb::DebugEventInfo& event_info) {
  if (events_.size() >= sync_api::kMaxEntries) {
    DVLOG(1) << "DebugInfoEventListener::AddEventToQueue Dropping an old event "
             << "because of full queue";

    events_.pop();
    events_dropped_ = true;
  }
  events_.push(event_info);
}
}  // namespace sync_api
