// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_thread.h"

#include <algorithm>
#include <queue>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "build/build_config.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/sessions/sync_session.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#endif

using std::priority_queue;
using std::min;
using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {

using sessions::SyncSession;
using sessions::SyncSessionSnapshot;
using sessions::SyncSourceInfo;
using sessions::TypePayloadMap;

// We use high values here to ensure that failure to receive poll updates from
// the server doesn't result in rapid-fire polling from the client due to low
// local limits.
const int SyncerThread::kDefaultShortPollIntervalSeconds = 3600 * 8;
const int SyncerThread::kDefaultLongPollIntervalSeconds = 3600 * 12;

// TODO(tim): This is used to regulate the short poll (when notifications are
// disabled) based on user idle time.  If it is set to a smaller value than
// the short poll interval, it basically does nothing; for now, this is what
// we want and allows stronger control over the poll rate from the server. We
// should probably re-visit this code later and figure out if user idle time
// is really something we want and make sure it works, if it is.
const int SyncerThread::kDefaultMaxPollIntervalMs = 30 * 60 * 1000;

// Backoff interval randomization factor.
static const int kBackoffRandomizationFactor = 2;

const int SyncerThread::kMaxBackoffSeconds = 60 * 60 * 4;  // 4 hours.

SyncerThread::ProtectedFields::ProtectedFields()
        : stop_syncer_thread_(false),
          pause_requested_(false),
          paused_(false),
          syncer_(NULL),
          connected_(false),
          pending_nudge_source_(kUnknown) {}

SyncerThread::ProtectedFields::~ProtectedFields() {}

void SyncerThread::NudgeSyncerWithPayloads(
    int milliseconds_from_now,
    NudgeSource source,
    const TypePayloadMap& model_types_with_payloads) {
  base::AutoLock lock(lock_);
  if (vault_.syncer_ == NULL) {
    return;
  }

  NudgeSyncImpl(milliseconds_from_now, source, model_types_with_payloads);
}

void SyncerThread::NudgeSyncerWithDataTypes(
    int milliseconds_from_now,
    NudgeSource source,
    const syncable::ModelTypeBitSet& model_types) {
  base::AutoLock lock(lock_);
  if (vault_.syncer_ == NULL) {
    return;
  }

  TypePayloadMap model_types_with_payloads =
      sessions::MakeTypePayloadMapFromBitSet(model_types, std::string());
  NudgeSyncImpl(milliseconds_from_now, source, model_types_with_payloads);
}

void SyncerThread::NudgeSyncer(
    int milliseconds_from_now,
    NudgeSource source) {
  base::AutoLock lock(lock_);
  if (vault_.syncer_ == NULL) {
    return;
  }

  // Set all enabled datatypes.
  ModelSafeRoutingInfo routes;
  session_context_->registrar()->GetModelSafeRoutingInfo(&routes);
  TypePayloadMap model_types_with_payloads =
      sessions::MakeTypePayloadMapFromRoutingInfo(routes, std::string());
  NudgeSyncImpl(milliseconds_from_now, source, model_types_with_payloads);
}

SyncerThread::SyncerThread(sessions::SyncSessionContext* context)
    : thread_main_started_(false, false),
      thread_("SyncEngine_SyncerThread"),
      vault_field_changed_(&lock_),
      conn_mgr_hookup_(NULL),
      syncer_short_poll_interval_seconds_(kDefaultShortPollIntervalSeconds),
      syncer_long_poll_interval_seconds_(kDefaultLongPollIntervalSeconds),
      syncer_polling_interval_(kDefaultShortPollIntervalSeconds),
      syncer_max_interval_(kDefaultMaxPollIntervalMs),
      session_context_(context),
      disable_idle_detection_(false) {
  DCHECK(context);

  if (context->connection_manager())
    WatchConnectionManager(context->connection_manager());
}

SyncerThread::~SyncerThread() {
  conn_mgr_hookup_.reset();
  delete vault_.syncer_;
  CHECK(!thread_.IsRunning());
}

// Creates and starts a syncer thread.
// Returns true if it creates a thread or if there's currently a thread running
// and false otherwise.
bool SyncerThread::Start() {
  {
    base::AutoLock lock(lock_);
    if (thread_.IsRunning()) {
      return true;
    }

    if (!thread_.Start()) {
      return false;
    }
  }

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &SyncerThread::ThreadMain));

  // Wait for notification that our task makes it safely onto the message
  // loop before returning, so the caller can't call Stop before we're
  // actually up and running.  This is for consistency with the old pthread
  // impl because pthread_create would do this in one step.
  thread_main_started_.Wait();
  VLOG(1) << "SyncerThread started.";
  return true;
}

// Stop processing. A max wait of at least 2*server RTT time is recommended.
// Returns true if we stopped, false otherwise.
bool SyncerThread::Stop(int max_wait) {
  RequestSyncerExitAndSetThreadStopConditions();

  // This will join, and finish when ThreadMain terminates.
  thread_.Stop();
  return true;
}

void SyncerThread::RequestSyncerExitAndSetThreadStopConditions() {
  {
    base::AutoLock lock(lock_);
    // If the thread has been started, then we either already have or are about
    // to enter ThreadMainLoop so we have to proceed with shutdown and wait for
    // it to finish.  If the thread has not been started --and we now own the
    // lock-- then we can early out because the caller has not called Start().
    if (!thread_.IsRunning())
      return;

    VLOG(1) << "SyncerThread::Stop - setting ThreadMain exit condition to true "
               "(vault_.stop_syncer_thread_)";
    // Exit the ThreadMainLoop once the syncer finishes (we tell it to exit
    // below).
    vault_.stop_syncer_thread_ = true;
    if (NULL != vault_.syncer_) {
      // Try to early exit the syncer itself, which could be looping inside
      // SyncShare.
      vault_.syncer_->RequestEarlyExit();
    }

    // stop_syncer_thread_ is now true and the Syncer has been told to exit.
    // We want to wake up all waiters so they can re-examine state. We signal,
    // causing all waiters to try to re-acquire the lock, and then we release
    // the lock, and join on our internal thread which should soon run off the
    // end of ThreadMain.
    vault_field_changed_.Broadcast();
  }
}

bool SyncerThread::RequestPause() {
  base::AutoLock lock(lock_);
  if (vault_.pause_requested_ || vault_.paused_)
    return false;

  if (thread_.IsRunning()) {
    // Set the pause request.  The syncer thread will read this
    // request, enter the paused state, and send the PAUSED
    // notification.
    vault_.pause_requested_ = true;
    vault_field_changed_.Broadcast();
    VLOG(1) << "Pause requested.";
  } else {
    // If the thread is not running, go directly into the paused state
    // and notify.
    EnterPausedState();
    VLOG(1) << "Paused while not running.";
  }
  return true;
}

void SyncerThread::Notify(SyncEngineEvent::EventCause cause) {
  session_context_->NotifyListeners(SyncEngineEvent(cause));
}

bool SyncerThread::RequestResume() {
  base::AutoLock lock(lock_);
  // Only valid to request a resume when we are already paused or we
  // have a pause pending.
  if (!(vault_.paused_ || vault_.pause_requested_))
    return false;

  if (thread_.IsRunning()) {
    if (vault_.pause_requested_) {
      // If pause was requested we have not yet paused.  In this case,
      // the resume cancels the pause request.
      vault_.pause_requested_ = false;
      vault_field_changed_.Broadcast();
      Notify(SyncEngineEvent::SYNCER_THREAD_RESUMED);
      VLOG(1) << "Pending pause canceled by resume.";
    } else {
      // Unpause and notify.
      vault_.paused_ = false;
      vault_field_changed_.Broadcast();
    }
  } else {
    ExitPausedState();
    VLOG(1) << "Resumed while not running.";
  }
  return true;
}

void SyncerThread::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  syncer_long_poll_interval_seconds_ = static_cast<int>(
      new_interval.InSeconds());
}

void SyncerThread::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  syncer_short_poll_interval_seconds_ = static_cast<int>(
      new_interval.InSeconds());
}

void SyncerThread::OnSilencedUntil(const base::TimeTicks& silenced_until) {
  silenced_until_ = silenced_until;
}

bool SyncerThread::IsSyncingCurrentlySilenced() {
  // We should ignore reads from silenced_until_ under ThreadSanitizer
  // since this is a benign race.
  ANNOTATE_IGNORE_READS_BEGIN();
  bool ret = (silenced_until_ - TimeTicks::Now()) >= TimeDelta::FromSeconds(0);
  ANNOTATE_IGNORE_READS_END();
  return ret;
}

void SyncerThread::OnShouldStopSyncingPermanently() {
  RequestSyncerExitAndSetThreadStopConditions();
  Notify(SyncEngineEvent::STOP_SYNCING_PERMANENTLY);
}

void SyncerThread::ThreadMainLoop() {
  // This is called with lock_ acquired.
  lock_.AssertAcquired();
  VLOG(1) << "In thread main loop.";

  // Use the short poll value by default.
  vault_.current_wait_interval_.poll_delta =
      TimeDelta::FromSeconds(syncer_short_poll_interval_seconds_);
  int user_idle_milliseconds = 0;
  TimeTicks last_sync_time;
  bool initial_sync_for_thread = true;
  bool continue_sync_cycle = false;

#if defined(OS_LINUX)
  idle_query_.reset(new IdleQueryLinux());
#endif

  if (vault_.syncer_ == NULL) {
    VLOG(1) << "Syncer thread waiting for database initialization.";
    while (vault_.syncer_ == NULL && !vault_.stop_syncer_thread_)
      vault_field_changed_.Wait();
    VLOG_IF(1, !(vault_.syncer_ == NULL)) << "Syncer was found after DB "
                                             "started.";
  }

  while (!vault_.stop_syncer_thread_) {
    // The Wait()s in these conditionals using |vault_| are not TimedWait()s (as
    // below) because we cannot poll until these conditions are met, so we wait
    // indefinitely.

    // If we are not connected, enter WaitUntilConnectedOrQuit() which
    // will return only when the network is connected or a quit is
    // requested.  Note that it is possible to exit
    // WaitUntilConnectedOrQuit() in the paused state which will be
    // handled by the next statement.
    if (!vault_.connected_ && !initial_sync_for_thread) {
      WaitUntilConnectedOrQuit();
      continue;
    }

    // Check if we should be paused or if a pause was requested.  Note
    // that we don't check initial_sync_for_thread here since we want
    // the pause to happen regardless if it is the initial sync or not.
    if (vault_.pause_requested_ || vault_.paused_) {
      PauseUntilResumedOrQuit();
      continue;
    }

    const TimeTicks next_poll = last_sync_time +
        vault_.current_wait_interval_.poll_delta;
    bool throttled = vault_.current_wait_interval_.mode ==
        WaitInterval::THROTTLED;
    // If we are throttled, we must wait.  Otherwise, wait until either the next
    // nudge (if one exists) or the poll interval.
    TimeTicks end_wait = next_poll;
    if (!throttled && !vault_.pending_nudge_time_.is_null()) {
      end_wait = std::min(end_wait, vault_.pending_nudge_time_);
    }
    VLOG(1) << "end_wait is " << end_wait.ToInternalValue()
            << "\nnext_poll is " << next_poll.ToInternalValue();

    // We block until the CV is signaled (e.g a control field changed, loss of
    // network connection, nudge, spurious, etc), or the poll interval elapses.
    TimeDelta sleep_time = end_wait - TimeTicks::Now();
    if (!initial_sync_for_thread && sleep_time > TimeDelta::FromSeconds(0)) {
      vault_field_changed_.TimedWait(sleep_time);

      if (TimeTicks::Now() < end_wait) {
        // Didn't timeout. Could be a spurious signal, or a signal corresponding
        // to an actual change in one of our control fields.  By continuing here
        // we perform the typical "always recheck conditions when signaled",
        // (typically handled by a while(condition_not_met) cv.wait() construct)
        // because we jump to the top of the loop.  The main difference is we
        // recalculate the wait interval, but last_sync_time won't have changed.
        // So if we were signaled by a nudge (for ex.) we'll grab the new nudge
        // off the queue and wait for that delta.  If it was a spurious signal,
        // we'll keep waiting for the same moment in time as we just were.
        continue;
       }
    }

    // Handle a nudge, caused by either a notification or a local bookmark
    // event.  This will also update the source of the following SyncMain call.
    VLOG(1) << "Calling Sync Main at time " << Time::Now().ToInternalValue();
    bool nudged = false;
    scoped_ptr<SyncSession> session;
    session.reset(SyncMain(vault_.syncer_,
        throttled, continue_sync_cycle, &initial_sync_for_thread, &nudged));

    // Update timing information for how often these datatypes are triggering
    // nudges.
    base::TimeTicks now = TimeTicks::Now();
    if (!last_sync_time.is_null()) {
      TypePayloadMap::const_iterator iter;
      for (iter = session->source().types.begin();
           iter != session->source().types.end();
           ++iter) {
        syncable::PostTimeToTypeHistogram(iter->first,
                                          now - last_sync_time);
      }
    }

    last_sync_time = now;

    VLOG(1) << "Updating the next polling time after SyncMain";
    vault_.current_wait_interval_ = CalculatePollingWaitTime(
        static_cast<int>(vault_.current_wait_interval_.poll_delta.InSeconds()),
        &user_idle_milliseconds, &continue_sync_cycle, nudged);
  }
#if defined(OS_LINUX)
  idle_query_.reset();
#endif
}

void SyncerThread::SetConnected(bool connected) {
  DCHECK(!thread_.IsRunning());
  vault_.connected_ = connected;
}

void SyncerThread::SetSyncerPollingInterval(base::TimeDelta interval) {
  // TODO(timsteele): Use TimeDelta internally.
  syncer_polling_interval_ = static_cast<int>(interval.InSeconds());
}

void SyncerThread::SetSyncerShortPollInterval(base::TimeDelta interval) {
  // TODO(timsteele): Use TimeDelta internally.
  syncer_short_poll_interval_seconds_ =
      static_cast<int>(interval.InSeconds());
}

void SyncerThread::WaitUntilConnectedOrQuit() {
  VLOG(1) << "Syncer thread waiting for connection.";
  Notify(SyncEngineEvent::SYNCER_THREAD_WAITING_FOR_CONNECTION);

  bool is_paused = vault_.paused_;

  while (!vault_.connected_ && !vault_.stop_syncer_thread_) {
    if (!is_paused && vault_.pause_requested_) {
      // If we get a pause request while waiting for a connection,
      // enter the paused state.
      EnterPausedState();
      is_paused = true;
      VLOG(1) << "Syncer thread entering disconnected pause.";
    }

    if (is_paused && !vault_.paused_) {
      ExitPausedState();
      is_paused = false;
      VLOG(1) << "Syncer thread exiting disconnected pause.";
    }

    vault_field_changed_.Wait();
  }

  if (!vault_.stop_syncer_thread_) {
    Notify(SyncEngineEvent::SYNCER_THREAD_CONNECTED);
    VLOG(1) << "Syncer thread found connection.";
  }
}

void SyncerThread::PauseUntilResumedOrQuit() {
  VLOG(1) << "Syncer thread entering pause.";
  // If pause was requested (rather than already being paused), send
  // the PAUSED notification.
  if (vault_.pause_requested_)
    EnterPausedState();

  // Thread will get stuck here until either a resume is requested
  // or shutdown is started.
  while (vault_.paused_ && !vault_.stop_syncer_thread_)
    vault_field_changed_.Wait();

  // Notify that we have resumed if we are not shutting down.
  if (!vault_.stop_syncer_thread_)
    ExitPausedState();

  VLOG(1) << "Syncer thread exiting pause.";
}

void SyncerThread::EnterPausedState() {
  lock_.AssertAcquired();
  vault_.pause_requested_ = false;
  vault_.paused_ = true;
  vault_field_changed_.Broadcast();
  Notify(SyncEngineEvent::SYNCER_THREAD_PAUSED);
}

void SyncerThread::ExitPausedState() {
  lock_.AssertAcquired();
  vault_.paused_ = false;
  vault_field_changed_.Broadcast();
  Notify(SyncEngineEvent::SYNCER_THREAD_RESUMED);
}

void SyncerThread::DisableIdleDetection() {
  disable_idle_detection_ = true;
}

// We check how long the user's been idle and sync less often if the machine is
// not in use. The aim is to reduce server load.
SyncerThread::WaitInterval SyncerThread::CalculatePollingWaitTime(
    int last_poll_wait,  // Time in seconds.
    int* user_idle_milliseconds,
    bool* continue_sync_cycle,
    bool was_nudged) {
  lock_.AssertAcquired();  // We access 'vault' in here, so we need the lock.
  WaitInterval return_interval;

  // Server initiated throttling trumps everything.
  if (!silenced_until_.is_null()) {
    // We don't need to reset other state, it can continue where it left off.
    return_interval.mode = WaitInterval::THROTTLED;
    return_interval.poll_delta = silenced_until_ - TimeTicks::Now();
    return return_interval;
  }

  bool is_continuing_sync_cyle = *continue_sync_cycle;
  *continue_sync_cycle = false;

  // Determine if the syncer has unfinished work to do.
  SyncSessionSnapshot* snapshot = session_context_->previous_session_snapshot();
  const bool syncer_has_work_to_do = snapshot &&
      (snapshot->num_server_changes_remaining > 0 ||
       snapshot->unsynced_count > 0);
  VLOG(1) << "syncer_has_work_to_do is " << syncer_has_work_to_do;

  // First calculate the expected wait time, figuring in any backoff because of
  // user idle time.  next_wait is in seconds
  syncer_polling_interval_ = (!session_context_->notifications_enabled()) ?
      syncer_short_poll_interval_seconds_ :
      syncer_long_poll_interval_seconds_;
  int default_next_wait = syncer_polling_interval_;
  return_interval.poll_delta = TimeDelta::FromSeconds(default_next_wait);

  if (syncer_has_work_to_do) {
    // Provide exponential backoff due to consecutive errors, else attempt to
    // complete the work as soon as possible.
    if (is_continuing_sync_cyle) {
      return_interval.mode = WaitInterval::EXPONENTIAL_BACKOFF;
      if (was_nudged && vault_.current_wait_interval_.mode ==
          WaitInterval::EXPONENTIAL_BACKOFF) {
          // We were nudged, it failed, and we were already in backoff.
          return_interval.had_nudge_during_backoff = true;
          // Keep exponent for exponential backoff the same in this case.
          return_interval.poll_delta = vault_.current_wait_interval_.poll_delta;
      } else {
        // We weren't nudged, or we were in a NORMAL wait interval until now.
        return_interval.poll_delta = TimeDelta::FromSeconds(
            GetRecommendedDelaySeconds(last_poll_wait));
      }
    } else {
      // No consecutive error.
      return_interval.poll_delta = TimeDelta::FromSeconds(
           GetRecommendedDelaySeconds(0));
    }
    *continue_sync_cycle = true;
  } else if (!session_context_->notifications_enabled()) {
    // Ensure that we start exponential backoff from our base polling
    // interval when we are not continuing a sync cycle.
    last_poll_wait = std::max(last_poll_wait, syncer_polling_interval_);

    // Did the user start interacting with the computer again?
    // If so, revise our idle time (and probably next_sync_time) downwards
    int new_idle_time = disable_idle_detection_ ? 0 : UserIdleTime();
    if (new_idle_time < *user_idle_milliseconds) {
      *user_idle_milliseconds = new_idle_time;
    }
    return_interval.poll_delta = TimeDelta::FromMilliseconds(
        CalculateSyncWaitTime(last_poll_wait * 1000,
                              *user_idle_milliseconds));
    DCHECK_GE(return_interval.poll_delta.InSeconds(), default_next_wait);
  }

  VLOG(1) << "Sync wait: idle " << default_next_wait
          << " non-idle or backoff " << return_interval.poll_delta.InSeconds();

  return return_interval;
}

void SyncerThread::ThreadMain() {
  base::AutoLock lock(lock_);
  // Signal Start() to let it know we've made it safely onto the message loop,
  // and unblock it's caller.
  thread_main_started_.Signal();
  ThreadMainLoop();
  VLOG(1) << "Syncer thread ThreadMain is done.";
  Notify(SyncEngineEvent::SYNCER_THREAD_EXITING);
}

SyncSession* SyncerThread::SyncMain(Syncer* syncer, bool was_throttled,
    bool continue_sync_cycle, bool* initial_sync_for_thread,
    bool* was_nudged) {
  CHECK(syncer);

  // Since we are initiating a new session for which we are the delegate, we
  // are not currently silenced so reset this state for the next session which
  // may need to use it.
  silenced_until_ = base::TimeTicks();

  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  session_context_->registrar()->GetModelSafeRoutingInfo(&routes);
  session_context_->registrar()->GetWorkers(&workers);
  SyncSourceInfo info(GetAndResetNudgeSource(was_throttled,
      continue_sync_cycle, initial_sync_for_thread, was_nudged));
  scoped_ptr<SyncSession> session;

  base::AutoUnlock unlock(lock_);
  do {
    session.reset(new SyncSession(session_context_.get(), this,
                                  info, routes, workers));
    VLOG(1) << "Calling SyncShare.";
    syncer->SyncShare(session.get());
  } while (session->HasMoreToSync() && silenced_until_.is_null());

  VLOG(1) << "Done calling SyncShare.";
  return session.release();
}

SyncSourceInfo SyncerThread::GetAndResetNudgeSource(bool was_throttled,
                                                    bool continue_sync_cycle,
                                                    bool* initial_sync,
                                                    bool* was_nudged) {
  bool nudged = false;
  NudgeSource nudge_source = kUnknown;
  TypePayloadMap model_types_with_payloads;
  // Has the previous sync cycle completed?
  if (continue_sync_cycle)
    nudge_source = kContinuation;
  // Update the nudge source if a new nudge has come through during the
  // previous sync cycle.
  if (!vault_.pending_nudge_time_.is_null()) {
    if (!was_throttled) {
      nudge_source = vault_.pending_nudge_source_;
      model_types_with_payloads = vault_.pending_nudge_types_;
      nudged = true;
    }
    VLOG(1) << "Clearing pending nudge from " << vault_.pending_nudge_source_
            << " at tick " << vault_.pending_nudge_time_.ToInternalValue();
    vault_.pending_nudge_source_ = kUnknown;
    vault_.pending_nudge_types_.clear();
    vault_.pending_nudge_time_ = base::TimeTicks();
  }

  *was_nudged = nudged;

  // TODO(tim): Hack for bug 64136 to correctly tag continuations that result
  // from syncer having more work to do.  This will be handled properly with
  // the message loop based syncer thread, bug 26339.
  return MakeSyncSourceInfo(nudged || nudge_source == kContinuation,
      nudge_source, model_types_with_payloads, initial_sync);
}

SyncSourceInfo SyncerThread::MakeSyncSourceInfo(bool nudged,
    NudgeSource nudge_source,
    const TypePayloadMap& model_types_with_payloads,
    bool* initial_sync) {
  sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source =
      sync_pb::GetUpdatesCallerInfo::UNKNOWN;
  if (*initial_sync) {
    updates_source = sync_pb::GetUpdatesCallerInfo::FIRST_UPDATE;
    *initial_sync = false;
  } else if (!nudged) {
    updates_source = sync_pb::GetUpdatesCallerInfo::PERIODIC;
  } else {
    switch (nudge_source) {
      case kNotification:
        updates_source = sync_pb::GetUpdatesCallerInfo::NOTIFICATION;
        break;
      case kLocal:
        updates_source = sync_pb::GetUpdatesCallerInfo::LOCAL;
        break;
      case kContinuation:
        updates_source = sync_pb::GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION;
        break;
      case kClearPrivateData:
        updates_source = sync_pb::GetUpdatesCallerInfo::CLEAR_PRIVATE_DATA;
        break;
      case kUnknown:
      default:
        updates_source = sync_pb::GetUpdatesCallerInfo::UNKNOWN;
        break;
    }
  }

  TypePayloadMap sync_source_types;
  if (model_types_with_payloads.empty()) {
    // No datatypes requested. This must be a poll so set all enabled datatypes.
    ModelSafeRoutingInfo routes;
    session_context_->registrar()->GetModelSafeRoutingInfo(&routes);
    sync_source_types = sessions::MakeTypePayloadMapFromRoutingInfo(routes,
        std::string());
  } else {
    sync_source_types = model_types_with_payloads;
  }

  return SyncSourceInfo(updates_source, sync_source_types);
}

void SyncerThread::CreateSyncer(const std::string& dirname) {
  base::AutoLock lock(lock_);
  VLOG(1) << "Creating syncer up for: " << dirname;
  // The underlying database structure is ready, and we should create
  // the syncer.
  CHECK(vault_.syncer_ == NULL);
  vault_.syncer_ = new Syncer();
  vault_field_changed_.Broadcast();
}

// Sets |*connected| to false if it is currently true but |code| suggests that
// the current network configuration and/or auth state cannot be used to make
// forward progress, and user intervention (e.g changing server URL or auth
// credentials) is likely necessary.  If |*connected| is false, set it to true
// if |code| suggests that we just recently made healthy contact with the
// server.
static inline void CheckConnected(bool* connected,
                                  HttpResponse::ServerConnectionCode code,
                                  base::ConditionVariable* condvar) {
  if (*connected) {
    // Note, be careful when adding cases here because if the SyncerThread
    // thinks there is no valid connection as determined by this method, it
    // will drop out of *all* forward progress sync loops (it won't poll and it
    // will queue up Talk notifications but not actually call SyncShare) until
    // some external action causes a ServerConnectionManager to broadcast that
    // a valid connection has been re-established.
    if (HttpResponse::CONNECTION_UNAVAILABLE == code ||
        HttpResponse::SYNC_AUTH_ERROR == code) {
      *connected = false;
      condvar->Broadcast();
    }
  } else {
    if (HttpResponse::SERVER_CONNECTION_OK == code) {
      *connected = true;
      condvar->Broadcast();
    }
  }
}

void SyncerThread::WatchConnectionManager(ServerConnectionManager* conn_mgr) {
  conn_mgr_hookup_.reset(NewEventListenerHookup(conn_mgr->channel(), this,
                         &SyncerThread::HandleServerConnectionEvent));
  CheckConnected(&vault_.connected_, conn_mgr->server_status(),
                 &vault_field_changed_);
}

void SyncerThread::HandleServerConnectionEvent(
    const ServerConnectionEvent& event) {
  if (ServerConnectionEvent::STATUS_CHANGED == event.what_happened) {
    base::AutoLock lock(lock_);
    CheckConnected(&vault_.connected_, event.connection_code,
                   &vault_field_changed_);
  }
}

int SyncerThread::GetRecommendedDelaySeconds(int base_delay_seconds) {
  if (base_delay_seconds >= kMaxBackoffSeconds)
    return kMaxBackoffSeconds;

  // This calculates approx. base_delay_seconds * 2 +/- base_delay_seconds / 2
  int backoff_s =
      std::max(1, base_delay_seconds * kBackoffRandomizationFactor);

  // Flip a coin to randomize backoff interval by +/- 50%.
  int rand_sign = base::RandInt(0, 1) * 2 - 1;

  // Truncation is adequate for rounding here.
  backoff_s = backoff_s +
      (rand_sign * (base_delay_seconds / kBackoffRandomizationFactor));

  // Cap the backoff interval.
  backoff_s = std::max(1, std::min(backoff_s, kMaxBackoffSeconds));

  return backoff_s;
}

// Inputs and return value in milliseconds.
int SyncerThread::CalculateSyncWaitTime(int last_interval, int user_idle_ms) {
  // syncer_polling_interval_ is in seconds
  int syncer_polling_interval_ms = syncer_polling_interval_ * 1000;

  // This is our default and lower bound.
  int next_wait = syncer_polling_interval_ms;

  // Get idle time, bounded by max wait.
  int idle = min(user_idle_ms, syncer_max_interval_);

  // If the user has been idle for a while, we'll start decreasing the poll
  // rate.
  if (idle >= kPollBackoffThresholdMultiplier * syncer_polling_interval_ms) {
    next_wait = std::min(GetRecommendedDelaySeconds(
        last_interval / 1000), syncer_max_interval_ / 1000) * 1000;
  }

  return next_wait;
}

// Called with mutex_ already locked.
void SyncerThread::NudgeSyncImpl(
    int milliseconds_from_now,
    NudgeSource source,
    const TypePayloadMap& model_types_with_payloads) {
  // TODO(sync): Add the option to reset the backoff state machine.
  // This is needed so nudges that are a result of the user's desire
  // to download updates for a new data type can be satisfied quickly.
  if (vault_.current_wait_interval_.mode == WaitInterval::THROTTLED ||
      vault_.current_wait_interval_.had_nudge_during_backoff) {
    // Drop nudges on the floor if we've already had one since starting this
    // stage of exponential backoff or we are throttled.
    return;
  }

  // Union the current TypePayloadMap with any from nudges that may have already
  // posted (coalesce the nudge datatype information).
  // TODO(tim): It seems weird to do this if the sources don't match up (e.g.
  // if pending_source is kLocal and |source| is kClearPrivateData).
  sessions::CoalescePayloads(&vault_.pending_nudge_types_,
                             model_types_with_payloads);

  const TimeTicks nudge_time = TimeTicks::Now() +
      TimeDelta::FromMilliseconds(milliseconds_from_now);
  if (nudge_time <= vault_.pending_nudge_time_) {
    VLOG(1) << "Nudge for source " << source
            << " dropped due to existing later pending nudge";
    return;
  }

  VLOG(1) << "Replacing pending nudge for source " << source
          << " at " << nudge_time.ToInternalValue();

  vault_.pending_nudge_source_ = source;
  vault_.pending_nudge_time_ = nudge_time;
  vault_field_changed_.Broadcast();
}

void SyncerThread::SetNotificationsEnabled(bool notifications_enabled) {
  base::AutoLock lock(lock_);
  session_context_->set_notifications_enabled(notifications_enabled);
}

// Returns the amount of time since the user last interacted with the computer,
// in milliseconds
int SyncerThread::UserIdleTime() {
#if defined(OS_WIN)
  LASTINPUTINFO last_input_info;
  last_input_info.cbSize = sizeof(LASTINPUTINFO);

  // Get time in windows ticks since system start of last activity.
  BOOL b = ::GetLastInputInfo(&last_input_info);
  if (b == TRUE)
    return ::GetTickCount() - last_input_info.dwTime;
#elif defined(OS_MACOSX)
  // It would be great to do something like:
  //
  // return 1000 *
  //     CGEventSourceSecondsSinceLastEventType(
  //         kCGEventSourceStateCombinedSessionState,
  //         kCGAnyInputEventType);
  //
  // Unfortunately, CGEvent* lives in ApplicationServices, and we're a daemon
  // and can't link that high up the food chain. Thus this mucking in IOKit.

  io_service_t hid_service =
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOHIDSystem"));
  if (!hid_service) {
    LOG(WARNING) << "Could not obtain IOHIDSystem";
    return 0;
  }

  CFTypeRef object = IORegistryEntryCreateCFProperty(hid_service,
                                                     CFSTR("HIDIdleTime"),
                                                     kCFAllocatorDefault,
                                                     0);
  if (!object) {
    LOG(WARNING) << "Could not get IOHIDSystem's HIDIdleTime property";
    IOObjectRelease(hid_service);
    return 0;
  }

  int64 idle_time;  // in nanoseconds
  Boolean success = false;
  if (CFGetTypeID(object) == CFNumberGetTypeID()) {
    success = CFNumberGetValue((CFNumberRef)object,
                               kCFNumberSInt64Type,
                               &idle_time);
  } else {
    LOG(WARNING) << "IOHIDSystem's HIDIdleTime property isn't a number!";
  }

  CFRelease(object);
  IOObjectRelease(hid_service);

  if (!success) {
    LOG(WARNING) << "Could not get IOHIDSystem's HIDIdleTime property's value";
    return 0;
  }
  return idle_time / 1000000;  // nano to milli
#elif defined(OS_LINUX)
  if (idle_query_.get())
    return idle_query_->IdleTime();
  return 0;
#else
  static bool was_logged = false;
  if (!was_logged) {
    was_logged = true;
    VLOG(1) << "UserIdleTime unimplemented on this platform, synchronization "
               "will not throttle when user idle";
  }
#endif

  return 0;
}

}  // namespace browser_sync
