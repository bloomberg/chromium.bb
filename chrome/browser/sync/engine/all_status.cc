// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/all_status.h"

#include <algorithm>

#include "base/logging.h"
#include "base/port.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/common/deprecated/event_sys-inl.h"
#include "jingle/notifier/listener/talk_mediator.h"

namespace browser_sync {

static const time_t kMinSyncObserveInterval = 10;  // seconds

const char* AllStatus::GetSyncStatusString(SyncStatus icon) {
  const char* strings[] = {"OFFLINE", "OFFLINE_UNSYNCED", "SYNCING", "READY",
      "CONFLICT", "OFFLINE_UNUSABLE"};
  COMPILE_ASSERT(arraysize(strings) == ICON_STATUS_COUNT, enum_indexed_array);
  if (icon < 0 || icon >= static_cast<SyncStatus>(arraysize(strings)))
    LOG(FATAL) << "Illegal Icon State:" << icon;
  return strings[icon];
}

static const AllStatus::Status init_status =
  { AllStatus::OFFLINE };

static const AllStatusEvent shutdown_event =
  { AllStatusEvent::SHUTDOWN, init_status };

AllStatus::AllStatus() : status_(init_status),
                         channel_(new Channel(shutdown_event)) {
  status_.initial_sync_ended = true;
  status_.notifications_enabled = false;
}

AllStatus::~AllStatus() {
  syncer_thread_hookup_.reset();
  delete channel_;
}

void AllStatus::WatchConnectionManager(ServerConnectionManager* conn_mgr) {
  conn_mgr_hookup_.reset(NewEventListenerHookup(conn_mgr->channel(), this,
                         &AllStatus::HandleServerConnectionEvent));
}

void AllStatus::WatchSyncerThread(SyncerThread* syncer_thread) {
  syncer_thread_hookup_.reset(syncer_thread == NULL ? NULL :
      syncer_thread->relay_channel()->AddObserver(this));
}

AllStatus::Status AllStatus::CreateBlankStatus() const {
  Status status = status_;
  status.syncing = true;
  status.unsynced_count = 0;
  status.conflicting_count = 0;
  status.initial_sync_ended = false;
  status.syncer_stuck = false;
  status.max_consecutive_errors = 0;
  status.server_broken = false;
  status.updates_available = 0;
  status.updates_received = 0;
  return status;
}

AllStatus::Status AllStatus::CalcSyncing(const SyncerEvent &event) const {
  Status status = CreateBlankStatus();
  const sessions::SyncSessionSnapshot* snapshot = event.snapshot;
  status.unsynced_count += static_cast<int>(snapshot->unsynced_count);
  status.conflicting_count += snapshot->errors.num_conflicting_commits;
  // The syncer may not be done yet, which could cause conflicting updates.
  // But this is only used for status, so it is better to have visibility.
  status.conflicting_count += snapshot->num_conflicting_updates;

  status.syncing |= snapshot->syncer_status.syncing;
  status.syncing = snapshot->has_more_to_sync && snapshot->is_silenced;
  status.initial_sync_ended |= snapshot->is_share_usable;
  status.syncer_stuck |= snapshot->syncer_status.syncer_stuck;

  const sessions::ErrorCounters& errors(snapshot->errors);
  if (errors.consecutive_errors > status.max_consecutive_errors)
    status.max_consecutive_errors = errors.consecutive_errors;

  // 100 is an arbitrary limit.
  if (errors.consecutive_transient_error_commits > 100)
    status.server_broken = true;

  status.updates_available += snapshot->num_server_changes_remaining;
  status.updates_received += snapshot->max_local_timestamp;
  return status;
}

AllStatus::Status AllStatus::CalcSyncing() const {
  return CreateBlankStatus();
}

int AllStatus::CalcStatusChanges(Status* old_status) {
  int what_changed = 0;

  // Calculate what changed and what the new icon should be.
  if (status_.syncing != old_status->syncing)
    what_changed |= AllStatusEvent::SYNCING;
  if (status_.unsynced_count != old_status->unsynced_count)
    what_changed |= AllStatusEvent::UNSYNCED_COUNT;
  if (status_.server_up != old_status->server_up)
    what_changed |= AllStatusEvent::SERVER_UP;
  if (status_.server_reachable != old_status->server_reachable)
    what_changed |= AllStatusEvent::SERVER_REACHABLE;
  if (status_.notifications_enabled != old_status->notifications_enabled)
    what_changed |= AllStatusEvent::NOTIFICATIONS_ENABLED;
  if (status_.notifications_received != old_status->notifications_received)
    what_changed |= AllStatusEvent::NOTIFICATIONS_RECEIVED;
  if (status_.notifications_sent != old_status->notifications_sent)
    what_changed |= AllStatusEvent::NOTIFICATIONS_SENT;
  if (status_.initial_sync_ended != old_status->initial_sync_ended)
    what_changed |= AllStatusEvent::INITIAL_SYNC_ENDED;
  if (status_.authenticated != old_status->authenticated)
    what_changed |= AllStatusEvent::AUTHENTICATED;

  const bool unsynced_changes = status_.unsynced_count > 0;
  const bool online = status_.authenticated &&
    status_.server_reachable && status_.server_up && !status_.server_broken;
  if (online) {
    if (status_.syncer_stuck)
      status_.icon = CONFLICT;
    else if (unsynced_changes || status_.syncing)
      status_.icon = SYNCING;
    else
      status_.icon = READY;
  } else if (!status_.initial_sync_ended) {
    status_.icon = OFFLINE_UNUSABLE;
  } else if (unsynced_changes) {
    status_.icon = OFFLINE_UNSYNCED;
  } else {
    status_.icon = OFFLINE;
  }

  if (status_.icon != old_status->icon)
    what_changed |= AllStatusEvent::ICON;

  if (0 == what_changed)
    return 0;
  *old_status = status_;
  return what_changed;
}

void AllStatus::HandleAuthWatcherEvent(const AuthWatcherEvent& auth_event) {
  ScopedStatusLockWithNotify lock(this);
  switch (auth_event.what_happened) {
    case AuthWatcherEvent::GAIA_AUTH_FAILED:
    case AuthWatcherEvent::SERVICE_AUTH_FAILED:
    case AuthWatcherEvent::SERVICE_CONNECTION_FAILED:
    case AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START:
      status_.authenticated = false;
      break;
    case AuthWatcherEvent::AUTH_SUCCEEDED:
      // If we've already calculated that the server is reachable, since we've
      // successfully authenticated, we can be confident that the server is up.
      if (status_.server_reachable)
        status_.server_up = true;

      if (!status_.authenticated) {
        status_.authenticated = true;
        status_ = CalcSyncing();
      } else {
        lock.set_notify_plan(DONT_NOTIFY);
      }
      break;
    default:
      lock.set_notify_plan(DONT_NOTIFY);
      break;
  }
}

void AllStatus::HandleChannelEvent(const SyncerEvent& event) {
  ScopedStatusLockWithNotify lock(this);
  switch (event.what_happened) {
    case SyncerEvent::COMMITS_SUCCEEDED:
      break;
    case SyncerEvent::SYNC_CYCLE_ENDED:
    case SyncerEvent::STATUS_CHANGED:
      status_ = CalcSyncing(event);
      break;
    case SyncerEvent::SHUTDOWN_USE_WITH_CARE:
      // We're safe to use this value here because we don't call into the syncer
      // or block on any processes.
      lock.set_notify_plan(DONT_NOTIFY);
      syncer_thread_hookup_.reset();
      break;
    case SyncerEvent::OVER_QUOTA:
      LOG(WARNING) << "User has gone over quota.";
      lock.NotifyOverQuota();
      break;
    case SyncerEvent::REQUEST_SYNC_NUDGE:
    case SyncerEvent::PAUSED:
    case SyncerEvent::RESUMED:
    case SyncerEvent::WAITING_FOR_CONNECTION:
    case SyncerEvent::CONNECTED:
    case SyncerEvent::STOP_SYNCING_PERMANENTLY:
    case SyncerEvent::SYNCER_THREAD_EXITING:
       lock.set_notify_plan(DONT_NOTIFY);
       break;
    default:
      LOG(ERROR) << "Unrecognized Syncer Event: " << event.what_happened;
      lock.set_notify_plan(DONT_NOTIFY);
      break;
  }
}

void AllStatus::HandleServerConnectionEvent(
    const ServerConnectionEvent& event) {
  if (ServerConnectionEvent::STATUS_CHANGED == event.what_happened) {
    ScopedStatusLockWithNotify lock(this);
    status_.server_up = IsGoodReplyFromServer(event.connection_code);
    status_.server_reachable = event.server_reachable;
  }
}

AllStatus::Status AllStatus::status() const {
  AutoLock lock(mutex_);
  return status_;
}

void AllStatus::SetNotificationsEnabled(bool notifications_enabled) {
  ScopedStatusLockWithNotify lock(this);
  status_.notifications_enabled = notifications_enabled;
}

void AllStatus::IncrementNotificationsSent() {
  ScopedStatusLockWithNotify lock(this);
  ++status_.notifications_sent;
}

void AllStatus::IncrementNotificationsReceived() {
  ScopedStatusLockWithNotify lock(this);
  ++status_.notifications_received;
}

ScopedStatusLockWithNotify::ScopedStatusLockWithNotify(AllStatus* allstatus)
  : allstatus_(allstatus), plan_(NOTIFY_IF_STATUS_CHANGED) {
  event_.what_changed = 0;
  allstatus->mutex_.Acquire();
  event_.status = allstatus->status_;
}

ScopedStatusLockWithNotify::~ScopedStatusLockWithNotify() {
  if (DONT_NOTIFY == plan_) {
    allstatus_->mutex_.Release();
    return;
  }
  event_.what_changed |= allstatus_->CalcStatusChanges(&event_.status);
  allstatus_->mutex_.Release();
  if (event_.what_changed)
    allstatus_->channel()->NotifyListeners(event_);
}

void ScopedStatusLockWithNotify::NotifyOverQuota() {
  event_.what_changed |= AllStatusEvent::OVER_QUOTA;
}

}  // namespace browser_sync
