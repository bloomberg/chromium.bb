// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to run the syncer on a thread.
//

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_

#include <list>
#include <map>
#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/client_command_channel.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/browser/sync/util/pthread_helpers.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

class EventListenerHookup;

namespace syncable {
class DirectoryManager;
struct DirectoryManagerEvent;
}

namespace browser_sync {

class ModelSafeWorker;
class ServerConnectionManager;
class Syncer;
class TalkMediator;
class URLFactory;
struct ServerConnectionEvent;
struct SyncerEvent;
struct SyncerShutdownEvent;
struct TalkMediatorEvent;

class SyncerThread {
  FRIEND_TEST(SyncerThreadTest, CalculateSyncWaitTime);
  FRIEND_TEST(SyncerThreadTest, CalculatePollingWaitTime);

 public:
  friend class SyncerThreadTest;

  enum NudgeSource {
    kUnknown = 0,
    kNotification,
    kLocal,
    kContinuation
  };

  // Server can overwrite these values via client commands.
  // Standard short poll. This is used when XMPP is off.
  static const int kDefaultShortPollIntervalSeconds = 60;
  // Long poll is used when XMPP is on.
  static const int kDefaultLongPollIntervalSeconds = 3600;
  // 30 minutes by default. If exponential backoff kicks in, this is
  // the longest possible poll interval.
  static const int kDefaultMaxPollIntervalMs = 30 * 60 * 1000;

  SyncerThread(
      ClientCommandChannel* command_channel,
      syncable::DirectoryManager* mgr,
      ServerConnectionManager* connection_manager,
      AllStatus* all_status,
      ModelSafeWorker* model_safe_worker);
  ~SyncerThread();

  void WatchConnectionManager(ServerConnectionManager* conn_mgr);
  // Creates and starts a syncer thread.
  // Returns true if it creates a thread or if there's currently a thread
  // running and false otherwise.
  bool Start();

  // Stop processing. A max wait of at least 2*server RTT time is recommended.
  // returns true if we stopped, false otherwise.
  bool Stop(int max_wait);

  // Nudges the syncer to sync with a delay specified.  This API is for access
  // from the SyncerThread's controller and will cause a mutex lock.
  bool NudgeSyncer(int milliseconds_from_now, NudgeSource source);

  // Registers this thread to watch talk mediator events.
  void WatchTalkMediator(TalkMediator* talk_mediator);

  void WatchClientCommands(ClientCommandChannel* channel);

  SyncerEventChannel* channel();

 private:
  // A few members to gate the rate at which we nudge the syncer.
  enum {
    kNudgeRateLimitCount = 6,
    kNudgeRateLimitTime = 180,
  };

  // A queue of all scheduled nudges.  One insertion for every call to
  // NudgeQueue().
  typedef std::pair<timespec, NudgeSource> NudgeObject;

  struct IsTimeSpecGreater {
    inline bool operator() (const NudgeObject& lhs, const NudgeObject& rhs) {
      return lhs.first.tv_sec == rhs.first.tv_sec ?
          lhs.first.tv_nsec > rhs.first.tv_nsec :
          lhs.first.tv_sec > rhs.first.tv_sec;
    }
  };

  typedef std::priority_queue<NudgeObject,
                              std::vector<NudgeObject>, IsTimeSpecGreater>
    NudgeQueue;

  // Threshold multipler for how long before user should be considered idle.
  static const int kPollBackoffThresholdMultiplier = 10;

  friend void* RunSyncerThread(void* syncer_thread);
  void* Run();
  void HandleDirectoryManagerEvent(
      const syncable::DirectoryManagerEvent& event);
  void HandleSyncerEvent(const SyncerEvent& event);
  void HandleClientCommand(ClientCommandChannel::EventType event);

  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  void HandleTalkMediatorEvent(const TalkMediatorEvent& event);

  void* ThreadMain();
  void ThreadMainLoop();

  void SyncMain(Syncer* syncer);

  // Calculates the next sync wait time in seconds.  last_poll_wait is the time
  // duration of the previous polling timeout which was used.
  // user_idle_milliseconds is updated by this method, and is a report of the
  // full amount of time since the last period of activity for the user.  The
  // continue_sync_cycle parameter is used to determine whether or not we are
  // calculating a polling wait time that is a continuation of an sync cycle
  // which terminated while the syncer still had work to do.
  int CalculatePollingWaitTime(
      const AllStatus::Status& status,
      int last_poll_wait,  // in s
      int* user_idle_milliseconds,
      bool* continue_sync_cycle);
  // Helper to above function, considers effect of user idle time.
  int CalculateSyncWaitTime(int last_wait, int user_idle_ms);

  // Sets the source value of the controlled syncer's updates_source value.
  // The initial sync boolean is updated if read as a sentinel.  The following
  // two methods work in concert to achieve this goal.
  void UpdateNudgeSource(const timespec& now, bool* continue_sync_cycle,
                         bool* initial_sync);
  void SetUpdatesSource(bool nudged, NudgeSource nudge_source,
                        bool* initial_sync);

  // for unit tests only
  void DisableIdleDetection() { disable_idle_detection_ = true; }

  // false when we want to stop the thread.
  bool stop_syncer_thread_;

  // we use one mutex for all members except the channel.
  PThreadMutex mutex_;
  typedef PThreadScopedLock<PThreadMutex> MutexLock;

  // Handle of the running thread.
  pthread_t thread_;
  bool thread_running_;

  // Gets signaled whenever a thread outside of the syncer thread
  // changes a member variable.
  PThreadCondVar changed_;

  // State of the server connection
  bool connected_;

  // State of the notification framework is tracked by these values.
  bool p2p_authenticated_;
  bool p2p_subscribed_;

  scoped_ptr<EventListenerHookup> client_command_hookup_;
  scoped_ptr<EventListenerHookup> conn_mgr_hookup_;
  const AllStatus* allstatus_;

  Syncer* syncer_;

  syncable::DirectoryManager* dirman_;
  ServerConnectionManager* scm_;

  // Modifiable versions of kDefaultLongPollIntervalSeconds which can be
  // updated by the server.
  int syncer_short_poll_interval_seconds_;
  int syncer_long_poll_interval_seconds_;

  // The time we wait between polls in seconds. This is used as lower bound on
  // our wait time. Updated once per loop from the command line flag.
  int syncer_polling_interval_;

  // The upper bound on the nominal wait between polls in seconds. Note that
  // this bounds the "nominal" poll interval, while the the actual interval
  // also takes previous failures into account.
  int syncer_max_interval_;

  scoped_ptr<SyncerEventChannel> syncer_event_channel_;

  // This causes syncer to start syncing ASAP. If the rate of requests is
  // too high the request will be silently dropped.  mutex_ should be held when
  // this is called.
  void NudgeSyncImpl(int milliseconds_from_now, NudgeSource source);

  NudgeQueue nudge_queue_;

  scoped_ptr<EventListenerHookup> talk_mediator_hookup_;
  ClientCommandChannel* const command_channel_;
  scoped_ptr<EventListenerHookup> directory_manager_hookup_;
  scoped_ptr<EventListenerHookup> syncer_events_;

  // Handles any tasks that will result in model changes (modifications of
  // syncable::Entries). Pass this to the syncer created and managed by |this|.
  // Only non-null in syncapi case.
  scoped_ptr<ModelSafeWorker> model_safe_worker_;

  // Useful for unit tests
  bool disable_idle_detection_;

  DISALLOW_COPY_AND_ASSIGN(SyncerThread);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_
