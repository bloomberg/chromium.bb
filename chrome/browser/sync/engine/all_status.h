// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The AllStatus object watches various sync engine components and aggregates
// the status of all of them into one place.

#ifndef CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
#define CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_

#include <map>

#include "base/atomicops.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/util/event_sys.h"

namespace browser_sync {

class AuthWatcher;
class GaiaAuthenticator;
class ScopedStatusLockWithNotify;
class ServerConnectionManager;
class Syncer;
class SyncerThread;
class TalkMediator;
struct AllStatusEvent;
struct AuthWatcherEvent;
struct GaiaAuthEvent;
struct ServerConnectionEvent;
struct SyncerEvent;
struct TalkMediatorEvent;

class AllStatus {
  friend class ScopedStatusLockWithNotify;
 public:
  typedef EventChannel<AllStatusEvent, Lock> Channel;

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

  // Maximum interval for exponential backoff.
  static const int kMaxBackoffSeconds;

  AllStatus();
  ~AllStatus();

  void WatchConnectionManager(ServerConnectionManager* conn_mgr);
  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  // Both WatchAuthenticator/HandleGaiaAuthEvent and WatchAuthWatcher/
  // HandleAuthWatcherEventachieve have the same goal; use only one of the
  // following two. (The AuthWatcher is watched under Windows; the
  // GaiaAuthenticator is watched under Mac/Linux.)
  void WatchAuthenticator(GaiaAuthenticator* gaia);
  void HandleGaiaAuthEvent(const GaiaAuthEvent& event);

  void WatchAuthWatcher(AuthWatcher* auth_watcher);
  void HandleAuthWatcherEvent(const AuthWatcherEvent& event);

  void WatchSyncerThread(SyncerThread* syncer_thread);
  void HandleSyncerEvent(const SyncerEvent& event);

  void WatchTalkMediator(
      const browser_sync::TalkMediator* talk_mediator);
  void HandleTalkMediatorEvent(
      const browser_sync::TalkMediatorEvent& event);

  // Returns a string description of the SyncStatus (currently just the ascii
  // version of the enum). Will LOG(FATAL) if the status us out of range.
  static const char* GetSyncStatusString(SyncStatus status);

  Channel* channel() const { return channel_; }

  Status status() const;

  // DDOS avoidance function.  The argument and return value is in seconds
  static int GetRecommendedDelaySeconds(int base_delay_seconds);

  // This uses AllStatus' max_consecutive_errors as the error count
  int GetRecommendedDelay(int base_delay) const;

 protected:
  typedef std::map<Syncer*, EventListenerHookup*> Syncers;

  // Examines syncer to calculate syncing and the unsynced count,
  // and returns a Status with new values.
  Status CalcSyncing() const;
  Status CalcSyncing(const SyncerEvent& event) const;
  Status CreateBlankStatus() const;

  // Examines status to see what has changed, updates old_status in place.
  int CalcStatusChanges(Status* old_status);

  Status status_;
  Channel* const channel_;
  scoped_ptr<EventListenerHookup> conn_mgr_hookup_;
  scoped_ptr<EventListenerHookup> gaia_hookup_;
  scoped_ptr<EventListenerHookup> authwatcher_hookup_;
  scoped_ptr<EventListenerHookup> syncer_thread_hookup_;
  scoped_ptr<EventListenerHookup> diskfull_hookup_;
  scoped_ptr<EventListenerHookup> talk_mediator_hookup_;

  mutable Lock mutex_;  // Protects all data members.
  DISALLOW_COPY_AND_ASSIGN(AllStatus);
};

struct AllStatusEvent {
  enum {  // A bit mask of which members have changed.
    SHUTDOWN =               0x0000,
    ICON =                   0x0001,
    UNSYNCED_COUNT =         0x0002,
    AUTHENTICATED =          0x0004,
    SYNCING =                0x0008,
    SERVER_UP =              0x0010,
    NOTIFICATIONS_ENABLED =  0x0020,
    INITIAL_SYNC_ENDED =     0x0080,
    SERVER_REACHABLE =       0x0100,
    DISK_FULL =              0x0200,
    OVER_QUOTA =             0x0400,
    NOTIFICATIONS_RECEIVED = 0x0800,
    NOTIFICATIONS_SENT =     0x1000,
    TRASH_WARNING =          0x40000,
  };
  int what_changed;
  AllStatus::Status status;

  typedef AllStatusEvent EventType;
  static inline bool IsChannelShutdownEvent(const AllStatusEvent& e) {
    return SHUTDOWN == e.what_changed;
  }
};

enum StatusNotifyPlan {
  NOTIFY_IF_STATUS_CHANGED,
  // A small optimization, don't do the big compare when we know
  // nothing has changed.
  DONT_NOTIFY,
};

class ScopedStatusLockWithNotify {
 public:
  explicit ScopedStatusLockWithNotify(AllStatus* allstatus);
  ~ScopedStatusLockWithNotify();
  // Defaults to true, but can be explicitly reset so we don't have to
  // do the big compare in the destructor.  Small optimization.

  inline void set_notify_plan(StatusNotifyPlan plan) { plan_ = plan; }
  void NotifyOverQuota();
 protected:
  AllStatusEvent event_;
  AllStatus* const allstatus_;
  StatusNotifyPlan plan_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_ALL_STATUS_H_
