// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class to run the syncer on a thread.
#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD2_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD2_H_
#pragma once

#include "base/linked_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/sync/engine/nudge_source.h"
#include "chrome/browser/sync/engine/polling_constants.h"
#include "chrome/browser/sync/sessions/sync_session.h"
#include "chrome/browser/sync/sessions/sync_session_context.h"

namespace browser_sync {

struct ServerConnectionEvent;
class Syncer;

namespace s3 {

class SyncerThread : public sessions::SyncSession::Delegate {
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

  // Takes ownership of both |context| and |syncer|.
  SyncerThread(sessions::SyncSessionContext* context, Syncer* syncer);
  virtual ~SyncerThread();

  // Change the mode of operation.
  // We don't use a lock when changing modes, so we won't cause currently
  // scheduled jobs to adhere to the new mode.  We could protect it, but it
  // doesn't buy very much as a) a session could already be in progress and it
  // will continue no matter what, b) the scheduled sessions already contain
  // all their required state and won't be affected by potential change at
  // higher levels (i.e. the registrar), and c) we service tasks FIFO, so once
  // the mode changes all future jobs will be run against the updated mode.
  void Start(Mode mode);

  // Joins on the thread as soon as possible (currently running session
  // completes).
  void Stop();

  // The meat and potatoes.
  void ScheduleNudge(const base::TimeDelta& delay, NudgeSource source,
                     const syncable::ModelTypeBitSet& types);
  void ScheduleConfig(const base::TimeDelta& delay,
                      const syncable::ModelTypeBitSet& types);

  // Change status of notifications in the SyncSessionContext.
  void set_notifications_enabled(bool notifications_enabled);

  // DDOS avoidance function.  Calculates how long we should wait before trying
  // again after a failed sync attempt, where the last delay was |base_delay|.
  // TODO(tim): Look at URLRequestThrottlerEntryInterface.
  static base::TimeDelta GetRecommendedDelay(const base::TimeDelta& base_delay);

  // SyncSession::Delegate implementation.
  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until);
  virtual bool IsSyncingCurrentlySilenced();
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval);
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval);
  virtual void OnShouldStopSyncingPermanently();

 private:
  friend class SyncerThread2Test;

  // State pertaining to exponential backoff or throttling periods.
  struct WaitInterval;

  // Internal state for every sync task that is scheduled.
  struct SyncSessionJob {
    // An enum used to describe jobs for scheduling purposes.
    enum Purpose {
      // Our poll timer schedules POLL jobs periodically based on a server
      // assigned poll interval.
      POLL,
      // A nudge task can come from a variety of components needing to force
      // a sync.  The source is inferable from |session.source()|.
      NUDGE,
      // Typically used for fetching updates for a subset of the enabled types
      // during initial sync or reconfiguration.  We don't run all steps of
      // the sync cycle for these (e.g. CleanupDisabledTypes is skipped).
      CONFIGURATION,
    };

    Purpose purpose;
    base::TimeTicks scheduled_start;
    linked_ptr<sessions::SyncSession> session;
  };

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

  // Helper to assemble a job and post a delayed task to sync.
  void ScheduleSyncSessionJob(const base::TimeDelta& delay,
                              SyncSessionJob::Purpose purpose,
                              sessions::SyncSession* session);

  // Invoke the Syncer to perform a sync.
  void DoSyncSessionJob(const SyncSessionJob& job);

  // Called after the Syncer has performed the sync represented by |job|, to
  // reset our state.
  void FinishSyncSessionJob(const SyncSessionJob& job);

  // Helper to FinishSyncSessionJob to schedule the next sync operation.
  void ScheduleNextSync(const SyncSessionJob& old_job);

  // Helper to configure polling intervals. Used by Start and ScheduleNextSync.
  void AdjustPolling(const SyncSessionJob* old_job);

  // Helper to ScheduleNextSync in case of consecutive sync errors.
  void HandleConsecutiveContinuationError(const SyncSessionJob& old_job);

  // Determines if it is legal to run a sync job for |purpose| at
  // |scheduled_start|.  This checks current operational mode, backoff or
  // throttling, freshness (so we don't make redundant syncs), and connection.
  bool ShouldRunJob(SyncSessionJob::Purpose purpose,
                    const base::TimeTicks& scheduled_start);

  // 'Impl' here refers to real implementation of public functions, running on
  // |thread_|.
  void StartImpl(Mode mode);
  void ScheduleNudgeImpl(const base::TimeDelta& delay,
                         NudgeSource source,
                         const syncable::ModelTypeBitSet& model_types);
  void ScheduleConfigImpl(const base::TimeDelta& delay,
                          const ModelSafeRoutingInfo& routing_info,
                          const std::vector<ModelSafeWorker*>& workers);

  // Returns true if the client is currently in exponential backoff.
  bool IsBackingOff() const;

  // Helper to signal all listeners registered with |session_context_|.
  void Notify(SyncEngineEvent::EventCause cause);

  // ServerConnectionEventListener implementation.
  // TODO(tim): schedule a nudge when valid connection detected? in 1 minute?
  virtual void OnServerConnectionEvent(const ServerConnectionEvent& event);

  // Callback to change backoff state.
  void DoCanaryJob();
  void Unthrottle();

  // Creates a session for a poll and performs the sync.
  void PollTimerCallback();

  base::Thread thread_;

  // Modifiable versions of kDefaultLongPollIntervalSeconds which can be
  // updated by the server.
  base::TimeDelta syncer_short_poll_interval_seconds_;
  base::TimeDelta syncer_long_poll_interval_seconds_;

  // Periodic timer for polling.  See AdjustPolling.
  base::RepeatingTimer<SyncerThread> poll_timer_;

  // The mode of operation. We don't use a lock, see Start(...) comment.
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

  DISALLOW_COPY_AND_ASSIGN(SyncerThread);
};

}  // namespace s3

}  // namespace browser_sync

// The SyncerThread manages its own internal thread and thus outlives it. We
// don't need refcounting for posting tasks to this internal thread.
DISABLE_RUNNABLE_METHOD_REFCOUNT(browser_sync::s3::SyncerThread);

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCER_THREAD2_H_
