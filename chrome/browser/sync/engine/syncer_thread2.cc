// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_thread2.h"

#include <algorithm>

#include "base/rand_util.h"
#include "chrome/browser/sync/engine/syncer.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {

using sessions::SyncSession;
using sessions::SyncSessionSnapshot;
using sessions::SyncSourceInfo;
using syncable::ModelTypeBitSet;
using sync_pb::GetUpdatesCallerInfo;

namespace s3 {

SyncerThread::DelayProvider::DelayProvider() {}
SyncerThread::DelayProvider::~DelayProvider() {}

TimeDelta SyncerThread::DelayProvider::GetDelay(
    const base::TimeDelta& last_delay) {
  return SyncerThread::GetRecommendedDelay(last_delay);
}

SyncerThread::WaitInterval::WaitInterval(Mode mode, TimeDelta length)
    : mode(mode), had_nudge(false), length(length) { }

SyncerThread::SyncerThread(sessions::SyncSessionContext* context,
                           Syncer* syncer)
    : thread_("SyncEngine_SyncerThread"),
      syncer_short_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultShortPollIntervalSeconds)),
      syncer_long_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultLongPollIntervalSeconds)),
      server_connection_ok_(false),
      delay_provider_(new DelayProvider()),
      syncer_(syncer),
      session_context_(context) {
}

SyncerThread::~SyncerThread() {
  DCHECK(!thread_.IsRunning());
}

void SyncerThread::Start(Mode mode) {
  if (!thread_.IsRunning() && !thread_.Start()) {
    NOTREACHED() << "Unable to start SyncerThread.";
    return;
  }

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::StartImpl, mode));
}

void SyncerThread::StartImpl(Mode mode) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  DCHECK(!session_context_->account_name().empty());
  DCHECK(syncer_.get());
  mode_ = mode;
  AdjustPolling(NULL);  // Will kick start poll timer if needed.
}

bool SyncerThread::ShouldRunJob(SyncSessionJob::Purpose purpose,
    const TimeTicks& scheduled_start) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  // Check wait interval.
  if (wait_interval_.get()) {
    if (wait_interval_->mode == WaitInterval::THROTTLED)
      return false;

    DCHECK_EQ(wait_interval_->mode, WaitInterval::EXPONENTIAL_BACKOFF);
    DCHECK(purpose == SyncSessionJob::POLL ||
           purpose == SyncSessionJob::NUDGE);
    if ((purpose != SyncSessionJob::NUDGE) || wait_interval_->had_nudge)
      return false;
  }

  // Mode / purpose contract (See 'Mode' enum in header). Don't run jobs that
  // were intended for a normal sync if we are in configuration mode, and vice
  // versa.
  switch (mode_) {
    case CONFIGURATION_MODE:
      if (purpose != SyncSessionJob::CONFIGURATION)
        return false;
      break;
    case NORMAL_MODE:
      if (purpose != SyncSessionJob::POLL && purpose != SyncSessionJob::NUDGE)
        return false;
      break;
    default:
      NOTREACHED() << "Unknown SyncerThread Mode: " << mode_;
      return false;
  }

  // Continuation NUDGE tasks have priority over POLLs because they are the
  // only tasks that trigger exponential backoff, so this prevents them from
  // being starved from running (e.g. due to a very, very low poll interval,
  // such as 0ms). It's rare that this would ever matter in practice.
  if (purpose == SyncSessionJob::POLL && (pending_nudge_.get() &&
      pending_nudge_->session->source().first ==
          GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION)) {
    return false;
  }

  // Freshness condition.
  if (purpose == SyncSessionJob::NUDGE &&
      (scheduled_start < last_sync_session_end_time_)) {
    return false;
  }

  return server_connection_ok_;
}

namespace {
GetUpdatesCallerInfo::GetUpdatesSource GetUpdatesFromNudgeSource(
    NudgeSource source) {
  switch (source) {
    case NUDGE_SOURCE_NOTIFICATION:
      return GetUpdatesCallerInfo::NOTIFICATION;
    case NUDGE_SOURCE_LOCAL:
      return GetUpdatesCallerInfo::LOCAL;
    case NUDGE_SOURCE_CONTINUATION:
      return GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION;
    case NUDGE_SOURCE_CLEAR_PRIVATE_DATA:
      return GetUpdatesCallerInfo::CLEAR_PRIVATE_DATA;
    case NUDGE_SOURCE_UNKNOWN:
    default:
      return GetUpdatesCallerInfo::UNKNOWN;
  }
}

// Functor for std::find_if to search by ModelSafeGroup.
struct WorkerGroupIs {
  explicit WorkerGroupIs(ModelSafeGroup group) : group(group) {}
  bool operator()(ModelSafeWorker* w) {
    return group == w->GetModelSafeGroup();
  }
  ModelSafeGroup group;
};
}  // namespace

void SyncerThread::ScheduleNudge(const TimeDelta& delay,
    NudgeSource source, const ModelTypeBitSet& types) {
  if (!thread_.IsRunning()) {
    NOTREACHED();
    return;
  }

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::ScheduleNudgeImpl, delay, source, types));
}

void SyncerThread::ScheduleNudgeImpl(const TimeDelta& delay,
    NudgeSource source, const ModelTypeBitSet& model_types) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  TimeTicks rough_start = TimeTicks::Now() + delay;
  if (!ShouldRunJob(SyncSessionJob::NUDGE, rough_start))
    return;

  // Note we currently nudge for all types regardless of the ones incurring
  // the nudge.  Doing different would throw off some syncer commands like
  // CleanupDisabledTypes.  We may want to change this in the future.
  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  session_context_->registrar()->GetModelSafeRoutingInfo(&routes);
  session_context_->registrar()->GetWorkers(&workers);
  SyncSourceInfo info(GetUpdatesFromNudgeSource(source), model_types);

  scoped_ptr<SyncSession> session(new SyncSession(
      session_context_.get(), this, info, routes, workers));

  if (pending_nudge_.get()) {
    if (IsBackingOff() && delay > TimeDelta::FromSeconds(1))
      return;

    pending_nudge_->session->Coalesce(*session.get());
    if (!IsBackingOff()) {
      return;
    } else {
      // Re-schedule the current pending nudge.
      SyncSession* s = pending_nudge_->session.get();
      session.reset(new SyncSession(s->context(), s->delegate(), s->source(),
          s->routing_info(), s->workers()));
      pending_nudge_.reset();
    }
  }
  ScheduleSyncSessionJob(delay, SyncSessionJob::NUDGE, session.release());
}

// Helper to extract the routing info and workers corresponding to types in
// |types| from |registrar|.
void GetModelSafeParamsForTypes(const ModelTypeBitSet& types,
    ModelSafeWorkerRegistrar* registrar, ModelSafeRoutingInfo* routes,
    std::vector<ModelSafeWorker*>* workers) {
  ModelSafeRoutingInfo r_tmp;
  std::vector<ModelSafeWorker*> w_tmp;
  registrar->GetModelSafeRoutingInfo(&r_tmp);
  registrar->GetWorkers(&w_tmp);

  typedef std::vector<ModelSafeWorker*>::const_iterator iter;
  for (size_t i = syncable::FIRST_REAL_MODEL_TYPE; i < types.size(); ++i) {
    if (!types.test(i))
      continue;
    syncable::ModelType t = syncable::ModelTypeFromInt(i);
    DCHECK_EQ(1U, r_tmp.count(t));
    (*routes)[t] = r_tmp[t];
    iter it = std::find_if(w_tmp.begin(), w_tmp.end(), WorkerGroupIs(r_tmp[t]));
    if (it != w_tmp.end())
      workers->push_back(*it);
    else
      NOTREACHED();
  }

  iter it = std::find_if(w_tmp.begin(), w_tmp.end(),
                         WorkerGroupIs(GROUP_PASSIVE));
  if (it != w_tmp.end())
    workers->push_back(*it);
  else
    NOTREACHED();
}

void SyncerThread::ScheduleConfig(const TimeDelta& delay,
    const ModelTypeBitSet& types) {
  if (!thread_.IsRunning()) {
    NOTREACHED();
    return;
  }

  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  GetModelSafeParamsForTypes(types, session_context_->registrar(),
                             &routes, &workers);

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::ScheduleConfigImpl, delay, routes, workers));
}

void SyncerThread::ScheduleConfigImpl(const TimeDelta& delay,
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  NOTIMPLEMENTED() << "TODO(tim)";
}

void SyncerThread::ScheduleSyncSessionJob(const base::TimeDelta& delay,
    SyncSessionJob::Purpose purpose, sessions::SyncSession* session) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  SyncSessionJob job = {purpose, TimeTicks::Now() + delay,
                        make_linked_ptr(session)};
  if (purpose == SyncSessionJob::NUDGE) {
    DCHECK(!pending_nudge_.get() || pending_nudge_->session.get() == session);
    pending_nudge_.reset(new SyncSessionJob(job));
  }
  MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(this,
      &SyncerThread::DoSyncSessionJob, job), delay.InMilliseconds());
}

void SyncerThread::DoSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  if (job.purpose == SyncSessionJob::NUDGE) {
    DCHECK(pending_nudge_.get());
    if (pending_nudge_->session != job.session)
      return;  // Another nudge must have been scheduled in in the meantime.
    pending_nudge_.reset();
  } else if (job.purpose == SyncSessionJob::CONFIGURATION) {
    NOTIMPLEMENTED() << "TODO(tim): SyncShare [DOWNLOAD_UPDATES,APPLY_UPDATES]";
  }

  bool has_more_to_sync = true;
  bool did_job = false;
  while (ShouldRunJob(job.purpose, job.scheduled_start) && has_more_to_sync) {
    VLOG(1) << "SyncerThread: Calling SyncShare.";
    did_job = true;
    // Synchronously perform the sync session from this thread.
    syncer_->SyncShare(job.session.get());
    has_more_to_sync = job.session->HasMoreToSync();
    if (has_more_to_sync)
      job.session->ResetTransientState();
  }
  VLOG(1) << "SyncerThread: Done SyncShare looping.";
  if (did_job)
    FinishSyncSessionJob(job);
}

void SyncerThread::FinishSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  // Update timing information for how often datatypes are triggering nudges.
  base::TimeTicks now = TimeTicks::Now();
  for (size_t i = syncable::FIRST_REAL_MODEL_TYPE;
       i < job.session->source().second.size() &&
           !last_sync_session_end_time_.is_null();
       ++i) {
    if (job.session->source().second[i]) {
      syncable::PostTimeToTypeHistogram(syncable::ModelTypeFromInt(i),
                                        now - last_sync_session_end_time_);
    }
  }
  last_sync_session_end_time_ = now;
  if (IsSyncingCurrentlySilenced())
    return;  // Nothing to do.

  VLOG(1) << "Updating the next polling time after SyncMain";
  ScheduleNextSync(job);
}

void SyncerThread::ScheduleNextSync(const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  DCHECK(!old_job.session->HasMoreToSync());
  // Note: |num_server_changes_remaining| > 0 here implies that we received a
  // broken response while trying to download all updates, because the Syncer
  // will loop until this value is exhausted. Also, if unsynced_handles exist
  // but HasMoreToSync is false, this implies that the Syncer determined no
  // forward progress was possible at this time (an error, such as an HTTP
  // 500, is likely to have occurred during commit).
  const bool work_to_do =
     old_job.session->status_controller()->num_server_changes_remaining() > 0
     || old_job.session->status_controller()->unsynced_handles().size() > 0;
  VLOG(1) << "syncer has work to do: " << work_to_do;

  AdjustPolling(&old_job);

  // TODO(tim): Old impl had special code if notifications disabled. Needed?
  if (!work_to_do) {
    wait_interval_.reset();  // Success implies backoff relief.
    return;
  }

  if (old_job.session->source().first ==
      GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION) {
    // We don't seem to have made forward progress. Start or extend backoff.
    HandleConsecutiveContinuationError(old_job);
  } else if (IsBackingOff()) {
    // We weren't continuing but we're in backoff; must have been a nudge.
    DCHECK_EQ(SyncSessionJob::NUDGE, old_job.purpose);
    DCHECK(!wait_interval_->had_nudge);
    wait_interval_->had_nudge = true;
    wait_interval_->timer.Reset();
  } else {
    // We weren't continuing and we aren't in backoff.  Schedule a normal
    // continuation.
    ScheduleNudgeImpl(TimeDelta::FromSeconds(0), NUDGE_SOURCE_CONTINUATION,
                      old_job.session->source().second);
  }
}

void SyncerThread::AdjustPolling(const SyncSessionJob* old_job) {
  DCHECK(thread_.IsRunning());
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  TimeDelta poll  = (!session_context_->notifications_enabled()) ?
      syncer_short_poll_interval_seconds_ :
      syncer_long_poll_interval_seconds_;
  bool rate_changed = !poll_timer_.IsRunning() ||
                       poll != poll_timer_.GetCurrentDelay();

  if (old_job && old_job->purpose != SyncSessionJob::POLL && !rate_changed)
    poll_timer_.Reset();

  if (!rate_changed)
    return;

  // Adjust poll rate.
  poll_timer_.Stop();
  poll_timer_.Start(poll, this, &SyncerThread::PollTimerCallback);
}

void SyncerThread::HandleConsecutiveContinuationError(
    const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  DCHECK(!IsBackingOff() || !wait_interval_->timer.IsRunning());
  SyncSession* old = old_job.session.get();
  SyncSession* s(new SyncSession(session_context_.get(), this,
      old->source(), old->routing_info(), old->workers()));
  TimeDelta length = delay_provider_->GetDelay(
      IsBackingOff() ? wait_interval_->length : TimeDelta::FromSeconds(1));
  wait_interval_.reset(new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF,
                                        length));
  SyncSessionJob job = {SyncSessionJob::NUDGE, TimeTicks::Now() + length,
                        make_linked_ptr(s)};
  pending_nudge_.reset(new SyncSessionJob(job));
  wait_interval_->timer.Start(length, this, &SyncerThread::DoCanaryJob);
}

// static
TimeDelta SyncerThread::GetRecommendedDelay(const TimeDelta& last_delay) {
  if (last_delay.InSeconds() >= kMaxBackoffSeconds)
    return TimeDelta::FromSeconds(kMaxBackoffSeconds);

  // This calculates approx. base_delay_seconds * 2 +/- base_delay_seconds / 2
  int64 backoff_s =
      std::max(static_cast<int64>(1),
               last_delay.InSeconds() * kBackoffRandomizationFactor);

  // Flip a coin to randomize backoff interval by +/- 50%.
  int rand_sign = base::RandInt(0, 1) * 2 - 1;

  // Truncation is adequate for rounding here.
  backoff_s = backoff_s +
      (rand_sign * (last_delay.InSeconds() / kBackoffRandomizationFactor));

  // Cap the backoff interval.
  backoff_s = std::max(static_cast<int64>(1),
                       std::min(backoff_s, kMaxBackoffSeconds));

  return TimeDelta::FromSeconds(backoff_s);
}

void SyncerThread::Stop() {
  syncer_->RequestEarlyExit();  // Safe to call from any thread.
  thread_.Stop();
  Notify(SyncEngineEvent::SYNCER_THREAD_EXITING);
}

void SyncerThread::DoCanaryJob() {
  DCHECK(pending_nudge_.get());
  wait_interval_->had_nudge = false;
  SyncSessionJob copy = {pending_nudge_->purpose,
                         pending_nudge_->scheduled_start,
                         pending_nudge_->session};
  DoSyncSessionJob(copy);
}

void SyncerThread::PollTimerCallback() {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  ModelSafeRoutingInfo r;
  std::vector<ModelSafeWorker*> w;
  session_context_->registrar()->GetModelSafeRoutingInfo(&r);
  session_context_->registrar()->GetWorkers(&w);
  SyncSourceInfo info(GetUpdatesCallerInfo::PERIODIC, ModelTypeBitSet());
  SyncSession* s = new SyncSession(session_context_.get(), this, info, r, w);
  ScheduleSyncSessionJob(TimeDelta::FromSeconds(0), SyncSessionJob::POLL, s);
}

void SyncerThread::Unthrottle() {
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);
  wait_interval_.reset();
}

void SyncerThread::Notify(SyncEngineEvent::EventCause cause) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  session_context_->NotifyListeners(SyncEngineEvent(cause));
}

bool SyncerThread::IsBackingOff() const {
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::EXPONENTIAL_BACKOFF;
}

void SyncerThread::OnSilencedUntil(const base::TimeTicks& silenced_until) {
  wait_interval_.reset(new WaitInterval(WaitInterval::THROTTLED,
                                        silenced_until - TimeTicks::Now()));
  wait_interval_->timer.Start(wait_interval_->length, this,
      &SyncerThread::Unthrottle);
}

bool SyncerThread::IsSyncingCurrentlySilenced() {
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::THROTTLED;
}

void SyncerThread::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  syncer_short_poll_interval_seconds_ = new_interval;
}

void SyncerThread::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  syncer_long_poll_interval_seconds_ = new_interval;
}

void SyncerThread::OnShouldStopSyncingPermanently() {
  syncer_->RequestEarlyExit();  // Thread-safe.
  Notify(SyncEngineEvent::STOP_SYNCING_PERMANENTLY);
}

void SyncerThread::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  NOTIMPLEMENTED();
}

void SyncerThread::set_notifications_enabled(bool notifications_enabled) {
  session_context_->set_notifications_enabled(notifications_enabled);
}

}  // s3
}  // browser_sync
