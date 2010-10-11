// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The AllStatus object watches various sync engine components and aggregates
// the status of all of them into one place.

#ifndef CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
#define CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
#pragma once

#include <map>

#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/syncer_types.h"

namespace browser_sync {

class ScopedStatusLock;
class ServerConnectionManager;
class Syncer;
class SyncerThread;
struct AuthWatcherEvent;
struct ServerConnectionEvent;

class AllStatus : public SyncEngineEventListener {
  friend class ScopedStatusLock;
 public:
  // Status of the entire sync process distilled into a single enum.
  enum SyncStatus {
    // Can't connect to server, but there are no pending changes in
    // our local dataase.
    OFFLINE,
    // Can't connect to server, and there are pending changes in our
    // local cache.
    OFFLINE_UNSYNCED,
    // Connected and syncing.
    SYNCING,
    // Connected, no pending changes.
    READY,
    // Internal sync error.
    CONFLICT,
    // Can't connect to server, and we haven't completed the initial
    // sync yet.  So there's nothing we can do but wait for the server.
    OFFLINE_UNUSABLE,
    // For array sizing, etc.
    ICON_STATUS_COUNT
  };

  struct Status {
    SyncStatus icon;
    int unsynced_count;
    int conflicting_count;
    bool syncing;
    bool authenticated;  // Successfully authenticated via gaia
    // True if we have received at least one good reply from the server.
    bool server_up;
    bool server_reachable;
    // True after a client has done a first sync.
    bool initial_sync_ended;
    // True if any syncer is stuck.
    bool syncer_stuck;
    // True if any syncer is stopped because of server issues.
    bool server_broken;
    // True only if the notification listener has subscribed.
    bool notifications_enabled;
    // Notifications counters updated by the actions in synapi.
    int notifications_received;
    int notifications_sent;
    // The max number of consecutive errors from any component.
    int max_consecutive_errors;
    bool disk_full;

    // Contains current transfer item meta handle
    int64 current_item_meta_handle;
    // The next two values will be equal if all updates have been received.
    // total updates available.
    int64 updates_available;
    // total updates received.
    int64 updates_received;
  };

  AllStatus();
  ~AllStatus();

  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  void HandleAuthWatcherEvent(const AuthWatcherEvent& event);

  virtual void OnSyncEngineEvent(const SyncEngineEvent& event);

  // Returns a string description of the SyncStatus (currently just the ascii
  // version of the enum). Will LOG(FATAL) if the status us out of range.
  static const char* GetSyncStatusString(SyncStatus status);

  Status status() const;

  void SetNotificationsEnabled(bool notifications_enabled);

  void IncrementNotificationsSent();

  void IncrementNotificationsReceived();

 protected:
  // Examines syncer to calculate syncing and the unsynced count,
  // and returns a Status with new values.
  Status CalcSyncing(const SyncEngineEvent& event) const;
  Status CreateBlankStatus() const;

  // Examines status to see what has changed, updates old_status in place.
  void CalcStatusChanges();

  Status status_;

  mutable Lock mutex_;  // Protects all data members.
  DISALLOW_COPY_AND_ASSIGN(AllStatus);
};

class ScopedStatusLock {
 public:
  explicit ScopedStatusLock(AllStatus* allstatus);
  ~ScopedStatusLock();
 protected:
  AllStatus* allstatus_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
