// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to run the syncer on a thread.
// This is the default implementation of SyncerThread whose Stop implementation
// does not support a timeout, but is greatly simplified.
#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_
#pragma once

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/engine/syncer_types.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/deprecated/event_sys-inl.h"

#if defined(OS_LINUX)
#include "chrome/browser/sync/engine/idle_query_linux.h"
#endif

class EventListenerHookup;

namespace browser_sync {

class ModelSafeWorker;
class ServerConnectionManager;
class Syncer;
class URLFactory;
struct ServerConnectionEvent;

class SyncerThread : public base::RefCountedThreadSafe<SyncerThread>,
                     public sessions::SyncSession::Delegate {
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadTest, CalculateSyncWaitTime);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadTest, CalculatePollingWaitTime);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, Polling);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, Nudge);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, NudgeWithDataTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest,
                           NudgeWithDataTypesCoalesced);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, NudgeWithPayloads);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest,
                           NudgeWithPayloadsCoalesced);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, Throttling);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, AuthInvalid);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, Pause);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, StartWhenNotConnected);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, PauseWhenNotConnected);
  FRIEND_TEST_ALL_PREFIXES(SyncerThreadWithSyncerTest, StopSyncPermanently);
  friend class SyncerThreadWithSyncerTest;
  friend class SyncerThreadFactory;
 public:
  // Encapsulates the parameters that make up an interval on which the
  // syncer thread is sleeping.
  struct WaitInterval {
    enum Mode {
      // A wait interval whose duration has not been affected by exponential
      // backoff.  The base case for exponential backoff falls in to this case
      // (e.g when the exponent is 1).  So far, we don't need a separate case.
      // NORMAL intervals are not nudge-rate limited.
      NORMAL,
      // A wait interval whose duration has been affected by exponential
      // backoff.
      // EXPONENTIAL_BACKOFF intervals are nudge-rate limited to 1 per interval.
      EXPONENTIAL_BACKOFF,
      // A server-initiated throttled interval.  We do not allow any syncing
      // during such an interval.
      THROTTLED,
    };

    Mode mode;
    // This bool is set to true if we have observed a nudge during during this
    // interval and mode == EXPONENTIAL_BACKOFF.
    bool had_nudge_during_backoff;
    base::TimeDelta poll_delta;  // The wait duration until the next poll.

    WaitInterval() : mode(NORMAL), had_nudge_during_backoff(false) { }
  };

  enum NudgeSource {
    kUnknown = 0,
    kNotification,
    kLocal,
    kContinuation,
    kClearPrivateData
  };
  // Server can overwrite these values via client commands.
  // Standard short poll. This is used when XMPP is off.
  static const int kDefaultShortPollIntervalSeconds;
  // Long poll is used when XMPP is on.
  static const int kDefaultLongPollIntervalSeconds;
  // 30 minutes by default. If exponential backoff kicks in, this is the
  // longest possible poll interval.
  static const int kDefaultMaxPollIntervalMs;
  // Maximum interval for exponential backoff.
  static const int kMaxBackoffSeconds;

  explicit SyncerThread(sessions::SyncSessionContext* context);
  virtual ~SyncerThread();

  virtual void WatchConnectionManager(ServerConnectionManager* conn_mgr);

  // Starts a syncer thread.
  // Returns true if it creates a thread or if there's currently a thread
  // running and false otherwise.
  virtual bool Start();

  // Stop processing. |max_wait| doesn't do anything in this version.
  virtual bool Stop(int max_wait);

  // Request that the thread pauses.  Returns false if the request can
  // not be completed (e.g. the thread is not running).  When the
  // thread actually pauses, a SyncEngineEvent::PAUSED event notification
  // will be sent to the relay channel.
  virtual bool RequestPause();

  // Request that the thread resumes from pause.  Returns false if the
  // request can not be completed (e.g. the thread is not running or
  // is not currently paused).  When the thread actually resumes, a
  // SyncEngineEvent::RESUMED event notification will be sent to the relay
  // channel.
  virtual bool RequestResume();

  // Nudges the syncer to sync with a delay specified. This API is for access
  // from the SyncerThread's controller and will cause a mutex lock.
  virtual void NudgeSyncer(int milliseconds_from_now, NudgeSource source);

  // Same as |NudgeSyncer|, but supports tracking the datatypes that caused
  // the nudge to occur.
  virtual void NudgeSyncerWithDataTypes(
      int milliseconds_from_now,
      NudgeSource source,
      const syncable::ModelTypeBitSet& model_types);

  // Same as |NudgeSyncer|, but supports including a payload for passing on to
  // the download updates command. Datatypes with payloads are also considered
  // to have caused a nudged to occur and treated accordingly.
  virtual void NudgeSyncerWithPayloads(
      int milliseconds_from_now,
      NudgeSource source,
      const sessions::TypePayloadMap& model_types_with_payloads);

  void SetNotificationsEnabled(bool notifications_enabled);

  // Call this when a directory is opened
  void CreateSyncer(const std::string& dirname);

  // DDOS avoidance function.  The argument and return value is in seconds
  static int GetRecommendedDelaySeconds(int base_delay_seconds);

 protected:
  virtual void ThreadMain();
  void ThreadMainLoop();

  virtual void SetConnected(bool connected);

  virtual void SetSyncerPollingInterval(base::TimeDelta interval);
  virtual void SetSyncerShortPollInterval(base::TimeDelta interval);

  // Needed to emulate the behavior of pthread_create, which synchronously
  // started the thread and set the value of thread_running_ to true.
  // We can't quite match that because we asynchronously post the task,
  // which opens a window for Stop to get called before the task actually
  // makes it.  To prevent this, we block Start() until we're sure it's ok.
  base::WaitableEvent thread_main_started_;

  // Handle of the running thread.
  base::Thread thread_;

  // Fields that are modified / accessed by multiple threads go in this struct
  // for clarity and explicitness.
  struct ProtectedFields {
    ProtectedFields();
    ~ProtectedFields();

    // False when we want to stop the thread.
    bool stop_syncer_thread_;

    // True when a pause was requested.
    bool pause_requested_;

    // True when the thread is paused.
    bool paused_;

    Syncer* syncer_;

    // State of the server connection.
    bool connected_;

    // kUnknown if there is no pending nudge.  (Theoretically, there
    // could be a pending nudge of type kUnknown, so it's better to
    // check pending_nudge_time_.)
    NudgeSource pending_nudge_source_;

    // Map of all datatypes that are requesting a nudge. Can be union
    // from multiple nudges that are coalesced. In addition, we
    // optionally track a payload associated with each datatype (most recent
    // payload overwrites old ones). These payloads are used by the download
    // updates command and can contain datatype specific information the server
    // might use.
    sessions::TypePayloadMap pending_nudge_types_;

    // null iff there is no pending nudge.
    base::TimeTicks pending_nudge_time_;

    // The wait interval for to the current iteration of our main loop.  This is
    // only written to by the syncer thread, and since the only reader from a
    // different thread (NudgeSync) is called at totally random times, we don't
    // really need to access mutually exclusively as the data races that exist
    // are intrinsic, but do so anyway and avoid using 'volatile'.
    WaitInterval current_wait_interval_;
  } vault_;

  // Gets signaled whenever a thread outside of the syncer thread changes a
  // protected field in the vault_.
  base::ConditionVariable vault_field_changed_;

  // Used to lock everything in |vault_|.
  base::Lock lock_;

 private:
  // Threshold multipler for how long before user should be considered idle.
  static const int kPollBackoffThresholdMultiplier = 10;

  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until);
  virtual bool IsSyncingCurrentlySilenced();
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval);
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval);
  virtual void OnShouldStopSyncingPermanently();

  void HandleServerConnectionEvent(const ServerConnectionEvent& event);

  // Collect all local state required for a sync and build a SyncSession out of
  // it, reset state for the next time, and performs the sync cycle.
  // See |GetAndResetNudgeSource| for details on what 'reset' means.
  // |was_nudged| is set to true if the session returned is fulfilling a nudge.
  // Returns once the session is finished (HasMoreToSync returns false).  The
  // caller owns the returned SyncSession.
  sessions::SyncSession* SyncMain(Syncer* syncer,
                                  bool was_throttled,
                                  bool continue_sync_cycle,
                                  bool* initial_sync_for_thread,
                                  bool* was_nudged);

  // Calculates the next sync wait time and exponential backoff state.
  // last_poll_wait is the time duration of the previous polling timeout which
  // was used. user_idle_milliseconds is updated by this method, and is a report
  // of the full amount of time since the last period of activity for the user.
  // The continue_sync_cycle parameter is used to determine whether or not we
  // are calculating a polling wait time that is a continuation of an sync cycle
  // which terminated while the syncer still had work to do. was_nudged is used
  // in case of exponential backoff so we only allow one nudge per backoff
  // interval.
  WaitInterval CalculatePollingWaitTime(
      int last_poll_wait,  // in s
      int* user_idle_milliseconds,
      bool* continue_sync_cycle,
      bool was_nudged);

  // Helper to above function, considers effect of user idle time.
  virtual int CalculateSyncWaitTime(int last_wait, int user_idle_ms);

  // Resets the source tracking state to a clean slate and returns the current
  // state in a SyncSourceInfo.
  // The initial sync boolean is updated if read as a sentinel.  The following
  // two methods work in concert to achieve this goal.
  // If |was_throttled| was true, this still discards elapsed nudges, but we
  // treat the request as a periodic poll rather than a nudge from a source.
  // Builds a SyncSourceInfo and returns whether a nudge occurred in the
  // |was_nudged| parameter.
  sessions::SyncSourceInfo GetAndResetNudgeSource(bool was_throttled,
                                                  bool continue_sync_cycle,
                                                  bool* initial_sync,
                                                  bool* was_nudged);

  sessions::SyncSourceInfo MakeSyncSourceInfo(
      bool nudged,
      NudgeSource nudge_source,
      const sessions::TypePayloadMap& model_types_with_payloads,
      bool* initial_sync);

  int UserIdleTime();

  void WaitUntilConnectedOrQuit();

  // The thread will remain in this method until a resume is requested
  // or shutdown is started.
  void PauseUntilResumedOrQuit();

  void EnterPausedState();

  void ExitPausedState();

  // For unit tests only.
  virtual void DisableIdleDetection();

  // This sets all conditions for syncer thread termination but does not
  // actually join threads.  It is expected that Stop will be called at some
  // time after to fully stop and clean up.
  void RequestSyncerExitAndSetThreadStopConditions();

  void Notify(SyncEngineEvent::EventCause cause);

  scoped_ptr<EventListenerHookup> conn_mgr_hookup_;

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

  // This causes syncer to start syncing ASAP. If the rate of requests is too
  // high the request will be silently dropped.  mutex_ should be held when
  // this is called.
  void NudgeSyncImpl(
      int milliseconds_from_now,
      NudgeSource source,
      const sessions::TypePayloadMap& model_types_with_payloads);

#if defined(OS_LINUX)
  // On Linux, we need this information in order to query idle time.
  scoped_ptr<IdleQueryLinux> idle_query_;
#endif

  scoped_ptr<sessions::SyncSessionContext> session_context_;

  // Set whenever the server instructs us to stop sending it requests until
  // a specified time, and reset for each call to SyncShare. (Note that the
  // WaitInterval::THROTTLED contract is such that we don't call SyncShare at
  // all until the "silenced until" embargo expires.)
  base::TimeTicks silenced_until_;

  // Useful for unit tests
  bool disable_idle_detection_;

  DISALLOW_COPY_AND_ASSIGN(SyncerThread);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD_H_
