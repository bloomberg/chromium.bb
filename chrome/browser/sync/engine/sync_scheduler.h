// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to schedule syncer tasks intelligently.
#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNC_SCHEDULER_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNC_SCHEDULER_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/nudge_source.h"
#include "chrome/browser/sync/engine/polling_constants.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/syncable/model_type_payload_map.h"

class MessageLoop;

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace browser_sync {

struct ServerConnectionEvent;

class SyncScheduler : public sessions::SyncSession::Delegate,
                      public ServerConnectionEventListener {
 public:
  enum Mode {
    // In this mode, the thread only performs configuration tasks.  This is
    // designed to make the case where we want to download updates for a
    // specific type only, and not continue syncing until we are moved into
    // normal mode.
    CONFIGURATION_MODE,
    // Resumes polling and allows nudges, drops configuration tasks.  Runs
    // through entire sync cycle.
    NORMAL_MODE,
  };

  // All methods of SyncScheduler must be called on the same thread
  // (except for RequestEarlyExit()).

  // |name| is a display string to identify the syncer thread.  Takes
  // |ownership of both |context| and |syncer|.
  SyncScheduler(const std::string& name,
                sessions::SyncSessionContext* context, Syncer* syncer);

  // Calls Stop().
  virtual ~SyncScheduler();

  // Start the scheduler with the given mode.  If the scheduler is
  // already started, switch to the given mode, although some
  // scheduled tasks from the old mode may still run.  If non-NULL,
  // |callback| will be invoked when the mode has been changed to
  // |mode|.  Takes ownership of |callback|.
  void Start(Mode mode, const base::Closure& callback);

  // Request that any running syncer task stop as soon as possible.
  // This function can be called from any thread.  Stop must still be
  // called to stop future schedule tasks.
  //
  // TODO(akalin): This function is awkward.  Find a better way to let
  // the UI thread stop the syncer thread.
  void RequestEarlyExit();

  // Cancel all scheduled tasks.  Can be called even if already stopped.
  void Stop();

  // The meat and potatoes.
  void ScheduleNudge(const base::TimeDelta& delay, NudgeSource source,
                     const syncable::ModelTypeBitSet& types,
                     const tracked_objects::Location& nudge_location);
  void ScheduleNudgeWithPayloads(
      const base::TimeDelta& delay, NudgeSource source,
      const syncable::ModelTypePayloadMap& types_with_payloads,
      const tracked_objects::Location& nudge_location);

  // Note: The source argument of this function must come from the subset of
  // GetUpdatesCallerInfo values related to configurations.
  void ScheduleConfig(
      const syncable::ModelTypeBitSet& types,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source);

  void ScheduleClearUserData();
  // If this is called before Start(), the cleanup is guaranteed to
  // happen before the Start finishes.
  //
  // TODO(akalin): Figure out how to test this.
  void ScheduleCleanupDisabledTypes();

  // Change status of notifications in the SyncSessionContext.
  void set_notifications_enabled(bool notifications_enabled);

  base::TimeDelta sessions_commit_delay() const;

  // DDOS avoidance function.  Calculates how long we should wait before trying
  // again after a failed sync attempt, where the last delay was |base_delay|.
  // TODO(tim): Look at URLRequestThrottlerEntryInterface.
  static base::TimeDelta GetRecommendedDelay(const base::TimeDelta& base_delay);

  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(
      const base::TimeTicks& silenced_until) OVERRIDE;
  virtual bool IsSyncingCurrentlySilenced() OVERRIDE;
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE;
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE;
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE;
  virtual void OnShouldStopSyncingPermanently() OVERRIDE;
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& snapshot) OVERRIDE;

  // ServerConnectionEventListener implementation.
  // TODO(tim): schedule a nudge when valid connection detected? in 1 minute?
  virtual void OnServerConnectionEvent(
      const ServerConnectionEvent& event) OVERRIDE;

 private:
  enum JobProcessDecision {
    // Indicates we should continue with the current job.
    CONTINUE,
    // Indicates that we should save it to be processed later.
    SAVE,
    // Indicates we should drop this job.
    DROP,
  };

  struct SyncSessionJob {
    // An enum used to describe jobs for scheduling purposes.
    enum SyncSessionJobPurpose {
      // Uninitialized state, should never be hit in practice.
      UNKNOWN = -1,
      // Our poll timer schedules POLL jobs periodically based on a server
      // assigned poll interval.
      POLL,
      // A nudge task can come from a variety of components needing to force
      // a sync.  The source is inferable from |session.source()|.
      NUDGE,
      // The user invoked a function in the UI to clear their entire account
      // and stop syncing (globally).
      CLEAR_USER_DATA,
      // Typically used for fetching updates for a subset of the enabled types
      // during initial sync or reconfiguration.  We don't run all steps of
      // the sync cycle for these (e.g. CleanupDisabledTypes is skipped).
      CONFIGURATION,
      // The user disabled some types and we have to clean up the data
      // for those.
      CLEANUP_DISABLED_TYPES,
    };
    SyncSessionJob();
    SyncSessionJob(SyncSessionJobPurpose purpose, base::TimeTicks start,
        linked_ptr<sessions::SyncSession> session, bool is_canary_job,
        const tracked_objects::Location& nudge_location);
    ~SyncSessionJob();
    static const char* GetPurposeString(SyncSessionJobPurpose purpose);

    SyncSessionJobPurpose purpose;
    base::TimeTicks scheduled_start;
    linked_ptr<sessions::SyncSession> session;
    bool is_canary_job;

    // This is the location the job came from.  Used for debugging.
    // In case of multiple nudges getting coalesced this stores the
    // first location that came in.
    tracked_objects::Location from_here;
  };
  friend class SyncSchedulerTest;
  friend class SyncSchedulerWhiteboxTest;

  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
      DropNudgeWhileExponentialBackOff);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, SaveNudge);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, ContinueNudge);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, DropPoll);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, ContinuePoll);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest, ContinueConfiguration);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           SaveConfigurationWhileThrottled);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           SaveNudgeWhileThrottled);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           ContinueClearUserDataUnderAllCircumstances);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
                           ContinueCanaryJobConfig);
  FRIEND_TEST_ALL_PREFIXES(SyncSchedulerWhiteboxTest,
      ContinueNudgeWhileExponentialBackOff);

  // A component used to get time delays associated with exponential backoff.
  // Encapsulated into a class to facilitate testing.
  class DelayProvider {
   public:
    DelayProvider();
    virtual base::TimeDelta GetDelay(const base::TimeDelta& last_delay);
    virtual ~DelayProvider();
   private:
    DISALLOW_COPY_AND_ASSIGN(DelayProvider);
  };

  struct WaitInterval {
    enum Mode {
      // Uninitialized state, should not be set in practice.
      UNKNOWN = -1,
      // A wait interval whose duration has been affected by exponential
      // backoff.
      // EXPONENTIAL_BACKOFF intervals are nudge-rate limited to 1 per interval.
      EXPONENTIAL_BACKOFF,
      // A server-initiated throttled interval.  We do not allow any syncing
      // during such an interval.
      THROTTLED,
    };
    WaitInterval();
    ~WaitInterval();
    WaitInterval(Mode mode, base::TimeDelta length);

    static const char* GetModeString(Mode mode);

    Mode mode;

    // This bool is set to true if we have observed a nudge during this
    // interval and mode == EXPONENTIAL_BACKOFF.
    bool had_nudge;
    base::TimeDelta length;
    base::OneShotTimer<SyncScheduler> timer;

    // Configure jobs are saved only when backing off or throttling. So we
    // expose the pointer here.
    scoped_ptr<SyncSessionJob> pending_configure_job;
  };

  static const char* GetModeString(Mode mode);

  static const char* GetDecisionString(JobProcessDecision decision);

  // Helpers that log before posting to |sync_loop_|.

  void PostTask(const tracked_objects::Location& from_here,
                const char* name,
                const base::Closure& task);
  void PostDelayedTask(const tracked_objects::Location& from_here,
                       const char* name,
                       const base::Closure& task,
                       int64 delay_ms);

  // Helper to assemble a job and post a delayed task to sync.
  void ScheduleSyncSessionJob(
      const base::TimeDelta& delay,
      SyncSessionJob::SyncSessionJobPurpose purpose,
      sessions::SyncSession* session,
      const tracked_objects::Location& from_here);

  // Invoke the Syncer to perform a sync.
  void DoSyncSessionJob(const SyncSessionJob& job);

  // Called after the Syncer has performed the sync represented by |job|, to
  // reset our state.
  void FinishSyncSessionJob(const SyncSessionJob& job);

  // Record important state that might be needed in future syncs, such as which
  // data types may require cleanup.
  void UpdateCarryoverSessionState(const SyncSessionJob& old_job);

  // Helper to FinishSyncSessionJob to schedule the next sync operation.
  void ScheduleNextSync(const SyncSessionJob& old_job);

  // Helper to configure polling intervals. Used by Start and ScheduleNextSync.
  void AdjustPolling(const SyncSessionJob* old_job);

  // Helper to restart waiting with |wait_interval_|'s timer.
  void RestartWaiting();

  // Helper to ScheduleNextSync in case of consecutive sync errors.
  void HandleConsecutiveContinuationError(const SyncSessionJob& old_job);

  // Determines if it is legal to run |job| by checking current
  // operational mode, backoff or throttling, freshness
  // (so we don't make redundant syncs), and connection.
  bool ShouldRunJob(const SyncSessionJob& job);

  // Decide whether we should CONTINUE, SAVE or DROP the job.
  JobProcessDecision DecideOnJob(const SyncSessionJob& job);

  // Decide on whether to CONTINUE, SAVE or DROP the job when we are in
  // backoff mode.
  JobProcessDecision DecideWhileInWaitInterval(const SyncSessionJob& job);

  // Saves the job for future execution. Note: It drops all the poll jobs.
  void SaveJob(const SyncSessionJob& job);

  // Coalesces the current job with the pending nudge.
  void InitOrCoalescePendingJob(const SyncSessionJob& job);

  // 'Impl' here refers to real implementation of public functions, running on
  // |thread_|.
  void StartImpl(Mode mode, const base::Closure& callback);
  void ScheduleNudgeImpl(
      const base::TimeDelta& delay,
      sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source,
      const syncable::ModelTypePayloadMap& types_with_payloads,
      bool is_canary_job, const tracked_objects::Location& nudge_location);
  void ScheduleConfigImpl(const ModelSafeRoutingInfo& routing_info,
      const std::vector<ModelSafeWorker*>& workers,
      const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source);
  void ScheduleClearUserDataImpl();

  // Returns true if the client is currently in exponential backoff.
  bool IsBackingOff() const;

  // Helper to signal all listeners registered with |session_context_|.
  void Notify(SyncEngineEvent::EventCause cause);

  // Callback to change backoff state.
  void DoCanaryJob();
  void Unthrottle();

  // Executes the pending job. Called whenever an event occurs that may
  // change conditions permitting a job to run. Like when network connection is
  // re-established, mode changes etc.
  void DoPendingJobIfPossible(bool is_canary_job);

  // The pointer is owned by the caller.
  browser_sync::sessions::SyncSession* CreateSyncSession(
      const browser_sync::sessions::SyncSourceInfo& info);

  // Creates a session for a poll and performs the sync.
  void PollTimerCallback();

  // Assign |start| and |end| to appropriate SyncerStep values for the
  // specified |purpose|.
  void SetSyncerStepsForPurpose(SyncSessionJob::SyncSessionJobPurpose purpose,
                                SyncerStep* start,
                                SyncerStep* end);

  // Initializes the hookup between the ServerConnectionManager and us.
  void WatchConnectionManager();

  // Used to update |server_connection_ok_|, see below.
  void CheckServerConnectionManagerStatus(
      HttpResponse::ServerConnectionCode code);

  // Called once the first time thread_ is started to broadcast an initial
  // session snapshot containing data like initial_sync_ended.  Important when
  // the client starts up and does not need to perform an initial sync.
  void SendInitialSnapshot();

  virtual void OnActionableError(const sessions::SyncSessionSnapshot& snapshot);

  base::WeakPtrFactory<SyncScheduler> weak_ptr_factory_;

  // Used for logging.
  const std::string name_;

  // The message loop this object is on.  Almost all methods have to
  // be called on this thread.
  MessageLoop* const sync_loop_;

  // Set in Start(), unset in Stop().
  bool started_;

  // Modifiable versions of kDefaultLongPollIntervalSeconds which can be
  // updated by the server.
  base::TimeDelta syncer_short_poll_interval_seconds_;
  base::TimeDelta syncer_long_poll_interval_seconds_;

  // Server-tweakable sessions commit delay.
  base::TimeDelta sessions_commit_delay_;

  // Periodic timer for polling.  See AdjustPolling.
  base::RepeatingTimer<SyncScheduler> poll_timer_;

  // The mode of operation.
  Mode mode_;

  // TODO(tim): Bug 26339. This needs to track more than just time I think,
  // since the nudges could be for different types. Current impl doesn't care.
  base::TimeTicks last_sync_session_end_time_;

  // Have we observed a valid server connection?
  bool server_connection_ok_;

  // Tracks in-flight nudges so we can coalesce.
  scoped_ptr<SyncSessionJob> pending_nudge_;

  // Current wait state.  Null if we're not in backoff and not throttled.
  scoped_ptr<WaitInterval> wait_interval_;

  scoped_ptr<DelayProvider> delay_provider_;

  // Invoked to run through the sync cycle.
  scoped_ptr<Syncer> syncer_;

  scoped_ptr<sessions::SyncSessionContext> session_context_;

  DISALLOW_COPY_AND_ASSIGN(SyncScheduler);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNC_SCHEDULER_H_
