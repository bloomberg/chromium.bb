// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncer_thread.h"

#include <algorithm>

#include "base/rand_util.h"
#include "chrome/browser/sync/engine/syncer.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {

using sessions::SyncSession;
using sessions::SyncSessionSnapshot;
using sessions::SyncSourceInfo;
using syncable::ModelTypePayloadMap;
using syncable::ModelTypeBitSet;
using sync_pb::GetUpdatesCallerInfo;

SyncerThread::DelayProvider::DelayProvider() {}
SyncerThread::DelayProvider::~DelayProvider() {}

SyncerThread::WaitInterval::WaitInterval() {}
SyncerThread::WaitInterval::~WaitInterval() {}

SyncerThread::SyncSessionJob::SyncSessionJob() {}
SyncerThread::SyncSessionJob::~SyncSessionJob() {}

SyncerThread::SyncSessionJob::SyncSessionJob(SyncSessionJobPurpose purpose,
    base::TimeTicks start,
    linked_ptr<sessions::SyncSession> session, bool is_canary_job,
    const tracked_objects::Location& nudge_location) : purpose(purpose),
        scheduled_start(start),
        session(session),
        is_canary_job(is_canary_job),
        nudge_location(nudge_location) {
}

TimeDelta SyncerThread::DelayProvider::GetDelay(
    const base::TimeDelta& last_delay) {
  return SyncerThread::GetRecommendedDelay(last_delay);
}

GetUpdatesCallerInfo::GetUpdatesSource GetUpdatesFromNudgeSource(
    NudgeSource source) {
  switch (source) {
    case NUDGE_SOURCE_NOTIFICATION:
      return GetUpdatesCallerInfo::NOTIFICATION;
    case NUDGE_SOURCE_LOCAL:
      return GetUpdatesCallerInfo::LOCAL;
    case NUDGE_SOURCE_CONTINUATION:
      return GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION;
    case NUDGE_SOURCE_UNKNOWN:
      return GetUpdatesCallerInfo::UNKNOWN;
    default:
      NOTREACHED();
      return GetUpdatesCallerInfo::UNKNOWN;
  }
}

GetUpdatesCallerInfo::GetUpdatesSource GetSourceFromReason(
    sync_api::ConfigureReason reason) {
  switch (reason) {
    case sync_api::CONFIGURE_REASON_RECONFIGURATION:
      return GetUpdatesCallerInfo::RECONFIGURATION;
    case sync_api::CONFIGURE_REASON_MIGRATION:
      return GetUpdatesCallerInfo::MIGRATION;
    default:
      NOTREACHED();
  }

  return GetUpdatesCallerInfo::UNKNOWN;
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
      mode_(NORMAL_MODE),
      server_connection_ok_(false),
      delay_provider_(new DelayProvider()),
      syncer_(syncer),
      session_context_(context) {
}

SyncerThread::~SyncerThread() {
  DCHECK(!thread_.IsRunning());
}

void SyncerThread::CheckServerConnectionManagerStatus(
    HttpResponse::ServerConnectionCode code) {

  VLOG(1) << "SyncerThread(" << this << ")" << " Server connection changed."
          << "Old mode: " << server_connection_ok_ << " Code: " << code;
  // Note, be careful when adding cases here because if the SyncerThread
  // thinks there is no valid connection as determined by this method, it
  // will drop out of *all* forward progress sync loops (it won't poll and it
  // will queue up Talk notifications but not actually call SyncShare) until
  // some external action causes a ServerConnectionManager to broadcast that
  // a valid connection has been re-established.
  if (HttpResponse::CONNECTION_UNAVAILABLE == code ||
      HttpResponse::SYNC_AUTH_ERROR == code) {
    server_connection_ok_ = false;
    VLOG(1) << "SyncerThread(" << this << ")" << " Server connection changed."
            << " new mode:" << server_connection_ok_;
  } else if (HttpResponse::SERVER_CONNECTION_OK == code) {
    server_connection_ok_ = true;
    VLOG(1) << "SyncerThread(" << this << ")" << " Server connection changed."
            << " new mode:" << server_connection_ok_;
    DoCanaryJob();
  }
}

void SyncerThread::Start(Mode mode, ModeChangeCallback* callback) {
  VLOG(1) << "SyncerThread(" << this << ")" << "  Start called from thread "
          << MessageLoop::current()->thread_name();
  if (!thread_.IsRunning()) {
    VLOG(1) << "SyncerThread(" << this << ")" << " Starting thread with mode "
            << mode;
    if (!thread_.Start()) {
      NOTREACHED() << "Unable to start SyncerThread.";
      return;
    }
    WatchConnectionManager();
    thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &SyncerThread::SendInitialSnapshot));
  }

  VLOG(1) << "SyncerThread(" << this << ")" << "  Entering start with mode = "
          << mode;

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::StartImpl, mode, callback));
}

void SyncerThread::SendInitialSnapshot() {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  scoped_ptr<SyncSession> dummy(new SyncSession(session_context_.get(), this,
      SyncSourceInfo(), ModelSafeRoutingInfo(),
      std::vector<ModelSafeWorker*>()));
  SyncEngineEvent event(SyncEngineEvent::STATUS_CHANGED);
  sessions::SyncSessionSnapshot snapshot(dummy->TakeSnapshot());
  event.snapshot = &snapshot;
  session_context_->NotifyListeners(event);
}

void SyncerThread::WatchConnectionManager() {
  ServerConnectionManager* scm = session_context_->connection_manager();
  CheckServerConnectionManagerStatus(scm->server_status());
  scm->AddListener(this);
}

void SyncerThread::StartImpl(Mode mode, ModeChangeCallback* callback) {
  VLOG(1) << "SyncerThread(" << this << ")" << " Doing StartImpl with mode "
          << mode;

  // TODO(lipalani): This will leak if startimpl is never run. Fix it using a
  // ThreadSafeRefcounted object.
  scoped_ptr<ModeChangeCallback> scoped_callback(callback);
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  DCHECK(!session_context_->account_name().empty());
  DCHECK(syncer_.get());
  mode_ = mode;
  AdjustPolling(NULL);  // Will kick start poll timer if needed.
  if (scoped_callback.get())
    scoped_callback->Run();

  // We just changed our mode. See if there are any pending jobs that we could
  // execute in the new mode.
  DoPendingJobIfPossible(false);
}

SyncerThread::JobProcessDecision SyncerThread::DecideWhileInWaitInterval(
    const SyncSessionJob& job) {

  DCHECK(wait_interval_.get());
  DCHECK_NE(job.purpose, SyncSessionJob::CLEAR_USER_DATA);

  VLOG(1) << "SyncerThread(" << this << ")" << " Wait interval mode : "
          << wait_interval_->mode << "Wait interval had nudge : "
          << wait_interval_->had_nudge << "is canary job : "
          << job.is_canary_job;

  if (job.purpose == SyncSessionJob::POLL)
    return DROP;

  DCHECK(job.purpose == SyncSessionJob::NUDGE ||
      job.purpose == SyncSessionJob::CONFIGURATION);
  if (wait_interval_->mode == WaitInterval::THROTTLED)
    return SAVE;

  DCHECK_EQ(wait_interval_->mode, WaitInterval::EXPONENTIAL_BACKOFF);
  if (job.purpose == SyncSessionJob::NUDGE) {
    if (mode_ == CONFIGURATION_MODE)
      return SAVE;

    // If we already had one nudge then just drop this nudge. We will retry
    // later when the timer runs out.
    return wait_interval_->had_nudge ? DROP : CONTINUE;
  }
  // This is a config job.
  return job.is_canary_job ? CONTINUE : SAVE;
}

SyncerThread::JobProcessDecision SyncerThread::DecideOnJob(
    const SyncSessionJob& job) {
  if (job.purpose == SyncSessionJob::CLEAR_USER_DATA)
    return CONTINUE;

  if (wait_interval_.get())
    return DecideWhileInWaitInterval(job);

  if (mode_ == CONFIGURATION_MODE) {
    if (job.purpose == SyncSessionJob::NUDGE)
      return SAVE;
    else if (job.purpose == SyncSessionJob::CONFIGURATION)
      return CONTINUE;
    else
      return DROP;
  }

  // We are in normal mode.
  DCHECK_EQ(mode_, NORMAL_MODE);
  DCHECK_NE(job.purpose, SyncSessionJob::CONFIGURATION);

  // Freshness condition
  if (job.scheduled_start < last_sync_session_end_time_) {
    VLOG(1) << "SyncerThread(" << this << ")"
            << " Dropping job because of freshness";
    return DROP;
  }

  if (server_connection_ok_)
    return CONTINUE;

  VLOG(1) << "SyncerThread(" << this << ")"
          << " Bad server connection. Using that to decide on job.";
  return job.purpose == SyncSessionJob::NUDGE ? SAVE : DROP;
}

void SyncerThread::InitOrCoalescePendingJob(const SyncSessionJob& job) {
  DCHECK(job.purpose != SyncSessionJob::CONFIGURATION);
  if (pending_nudge_.get() == NULL) {
    VLOG(1) << "SyncerThread(" << this << ")"
            << " Creating a pending nudge job";
    SyncSession* s = job.session.get();
    scoped_ptr<SyncSession> session(new SyncSession(s->context(),
        s->delegate(), s->source(), s->routing_info(), s->workers()));

    SyncSessionJob new_job(SyncSessionJob::NUDGE, job.scheduled_start,
        make_linked_ptr(session.release()), false, job.nudge_location);
    pending_nudge_.reset(new SyncSessionJob(new_job));

    return;
  }

  VLOG(1) << "SyncerThread(" << this << ")" << " Coalescing a pending nudge";
  pending_nudge_->session->Coalesce(*(job.session.get()));
  pending_nudge_->scheduled_start = job.scheduled_start;

  // Unfortunately the nudge location cannot be modified. So it stores the
  // location of the first caller.
}

bool SyncerThread::ShouldRunJob(const SyncSessionJob& job) {
  JobProcessDecision decision = DecideOnJob(job);
  VLOG(1) << "SyncerThread(" << this << ")" << " Should run job, decision: "
          << decision << " Job purpose " << job.purpose << "mode " << mode_;
  if (decision != SAVE)
    return decision == CONTINUE;

  DCHECK(job.purpose == SyncSessionJob::NUDGE || job.purpose ==
      SyncSessionJob::CONFIGURATION);

  SaveJob(job);
  return false;
}

void SyncerThread::SaveJob(const SyncSessionJob& job) {
  DCHECK(job.purpose != SyncSessionJob::CLEAR_USER_DATA);
  if (job.purpose == SyncSessionJob::NUDGE) {
    VLOG(1) << "SyncerThread(" << this << ")" << " Saving a nudge job";
    InitOrCoalescePendingJob(job);
  } else if (job.purpose == SyncSessionJob::CONFIGURATION){
    VLOG(1) << "SyncerThread(" << this << ")" << " Saving a configuration job";
    DCHECK(wait_interval_.get());
    DCHECK(mode_ == CONFIGURATION_MODE);

    SyncSession* old = job.session.get();
    SyncSession* s(new SyncSession(session_context_.get(), this,
        old->source(), old->routing_info(), old->workers()));
    SyncSessionJob new_job(job.purpose, TimeTicks::Now(),
                          make_linked_ptr(s), false, job.nudge_location);
    wait_interval_->pending_configure_job.reset(new SyncSessionJob(new_job));
  } // drop the rest.
}

// Functor for std::find_if to search by ModelSafeGroup.
struct ModelSafeWorkerGroupIs {
  explicit ModelSafeWorkerGroupIs(ModelSafeGroup group) : group(group) {}
  bool operator()(ModelSafeWorker* w) {
    return group == w->GetModelSafeGroup();
  }
  ModelSafeGroup group;
};

void SyncerThread::ScheduleClearUserData() {
  if (!thread_.IsRunning()) {
    NOTREACHED();
    return;
  }
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::ScheduleClearUserDataImpl));
}

void SyncerThread::ScheduleNudge(const TimeDelta& delay,
    NudgeSource source, const ModelTypeBitSet& types,
    const tracked_objects::Location& nudge_location) {
  if (!thread_.IsRunning()) {
    VLOG(0) << "Dropping nudge because thread is not running.";
    NOTREACHED();
    return;
  }

  VLOG(1) << "SyncerThread(" << this << ")" << " Nudge scheduled";

  ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromBitSet(types, std::string());
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::ScheduleNudgeImpl, delay,
      GetUpdatesFromNudgeSource(source), types_with_payloads, false,
      nudge_location));
}

void SyncerThread::ScheduleNudgeWithPayloads(const TimeDelta& delay,
    NudgeSource source, const ModelTypePayloadMap& types_with_payloads,
    const tracked_objects::Location& nudge_location) {
  if (!thread_.IsRunning()) {
    NOTREACHED();
    return;
  }

  VLOG(1) << "SyncerThread(" << this << ")" << " Nudge scheduled with payloads";

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::ScheduleNudgeImpl, delay,
      GetUpdatesFromNudgeSource(source), types_with_payloads, false,
      nudge_location));
}

void SyncerThread::ScheduleClearUserDataImpl() {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  SyncSession* session = new SyncSession(session_context_.get(), this,
      SyncSourceInfo(), ModelSafeRoutingInfo(),
      std::vector<ModelSafeWorker*>());
  ScheduleSyncSessionJob(TimeDelta::FromSeconds(0),
      SyncSessionJob::CLEAR_USER_DATA, session, FROM_HERE);
}

void SyncerThread::ScheduleNudgeImpl(const TimeDelta& delay,
    GetUpdatesCallerInfo::GetUpdatesSource source,
    const ModelTypePayloadMap& types_with_payloads,
    bool is_canary_job, const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  VLOG(1) << "SyncerThread(" << this << ")" << " Running Schedule nudge impl";
  // Note we currently nudge for all types regardless of the ones incurring
  // the nudge.  Doing different would throw off some syncer commands like
  // CleanupDisabledTypes.  We may want to change this in the future.
  SyncSourceInfo info(source, types_with_payloads);

  SyncSession* session(CreateSyncSession(info));
  SyncSessionJob job(SyncSessionJob::NUDGE, TimeTicks::Now() + delay,
                     make_linked_ptr(session), is_canary_job,
                     nudge_location);

  session = NULL;
  if (!ShouldRunJob(job))
    return;

  if (pending_nudge_.get()) {
    if (IsBackingOff() && delay > TimeDelta::FromSeconds(1)) {
      VLOG(1) << "SyncerThread(" << this << ")" << " Dropping the nudge because"
              << "we are in backoff";
      return;
    }

    VLOG(1) << "SyncerThread(" << this << ")" << " Coalescing pending nudge";
    pending_nudge_->session->Coalesce(*(job.session.get()));

    if (!IsBackingOff()) {
      VLOG(1) << "SyncerThread(" << this << ")" << " Dropping a nudge because"
              << " we are not in backoff and the job was coalesced";
      return;
    } else {
      VLOG(1) << "SyncerThread(" << this << ")"
              << " Rescheduling pending nudge";
      SyncSession* s = pending_nudge_->session.get();
      job.session.reset(new SyncSession(s->context(), s->delegate(),
          s->source(), s->routing_info(), s->workers()));
      pending_nudge_.reset();
    }
  }

  // TODO(lipalani) - pass the job itself to ScheduleSyncSessionJob.
  ScheduleSyncSessionJob(delay, SyncSessionJob::NUDGE, job.session.release(),
      nudge_location);
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

  bool passive_group_added = false;

  typedef std::vector<ModelSafeWorker*>::const_iterator iter;
  for (size_t i = syncable::FIRST_REAL_MODEL_TYPE; i < types.size(); ++i) {
    if (!types.test(i))
      continue;
    syncable::ModelType t = syncable::ModelTypeFromInt(i);
    DCHECK_EQ(1U, r_tmp.count(t));
    (*routes)[t] = r_tmp[t];
    iter it = std::find_if(w_tmp.begin(), w_tmp.end(),
                           ModelSafeWorkerGroupIs(r_tmp[t]));
    if (it != w_tmp.end()) {
      iter it2 = std::find_if(workers->begin(), workers->end(),
                              ModelSafeWorkerGroupIs(r_tmp[t]));
      if (it2 == workers->end())
        workers->push_back(*it);

      if (r_tmp[t] == GROUP_PASSIVE)
        passive_group_added = true;
    } else {
        NOTREACHED();
    }
  }

  // Always add group passive.
  if (passive_group_added == false) {
    iter it = std::find_if(w_tmp.begin(), w_tmp.end(),
                           ModelSafeWorkerGroupIs(GROUP_PASSIVE));
    if (it != w_tmp.end())
      workers->push_back(*it);
    else
      NOTREACHED();
  }
}

void SyncerThread::ScheduleConfig(const ModelTypeBitSet& types,
                                  sync_api::ConfigureReason reason) {
  if (!thread_.IsRunning()) {
    VLOG(0) << "ScheduleConfig failed because thread is not running.";
    NOTREACHED();
    return;
  }

  VLOG(1) << "SyncerThread(" << this << ")" << " Scheduling a config";
  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  GetModelSafeParamsForTypes(types, session_context_->registrar(),
                             &routes, &workers);

  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncerThread::ScheduleConfigImpl, routes, workers,
      GetSourceFromReason(reason)));
}

void SyncerThread::ScheduleConfigImpl(const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers,
    const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  VLOG(1) << "SyncerThread(" << this << ")" << " ScheduleConfigImpl...";
  // TODO(tim): config-specific GetUpdatesCallerInfo value?
  SyncSession* session = new SyncSession(session_context_.get(), this,
      SyncSourceInfo(source,
          syncable::ModelTypePayloadMapFromRoutingInfo(
              routing_info, std::string())),
      routing_info, workers);
  ScheduleSyncSessionJob(TimeDelta::FromSeconds(0),
    SyncSessionJob::CONFIGURATION, session, FROM_HERE);
}

void SyncerThread::ScheduleSyncSessionJob(const base::TimeDelta& delay,
    SyncSessionJob::SyncSessionJobPurpose purpose,
    sessions::SyncSession* session,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());

  SyncSessionJob job(purpose, TimeTicks::Now() + delay,
                        make_linked_ptr(session), false, nudge_location);
  if (purpose == SyncSessionJob::NUDGE) {
    VLOG(1) << "SyncerThread(" << this << ")" << " Resetting pending_nudge in"
            << " ScheduleSyncSessionJob";
    DCHECK(!pending_nudge_.get() || pending_nudge_->session.get() == session);
    pending_nudge_.reset(new SyncSessionJob(job));
  }
  VLOG(1) << "SyncerThread(" << this << ")"
          << " Posting job to execute in DoSyncSessionJob. Job purpose "
          << job.purpose;
  MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(this,
      &SyncerThread::DoSyncSessionJob, job),
      delay.InMilliseconds());
}

void SyncerThread::SetSyncerStepsForPurpose(
    SyncSessionJob::SyncSessionJobPurpose purpose,
    SyncerStep* start, SyncerStep* end) {
  *end = SYNCER_END;
  switch (purpose) {
    case SyncSessionJob::CONFIGURATION:
      *start = DOWNLOAD_UPDATES;
      *end = APPLY_UPDATES;
      return;
    case SyncSessionJob::CLEAR_USER_DATA:
      *start = CLEAR_PRIVATE_DATA;
       return;
    case SyncSessionJob::NUDGE:
    case SyncSessionJob::POLL:
      *start = SYNCER_BEGIN;
      return;
    default:
      NOTREACHED();
  }
}

void SyncerThread::DoSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  if (!ShouldRunJob(job)) {
    LOG(WARNING) << "Not executing job at DoSyncSessionJob, purpose = "
        << job.purpose << " source = "
        << job.session->source().updates_source;
    return;
  }

  if (job.purpose == SyncSessionJob::NUDGE) {
    if (pending_nudge_.get() == NULL ||
        pending_nudge_->session != job.session) {
      VLOG(1) << "SyncerThread(" << this << ")" << "Dropping a nudge in "
              << "DoSyncSessionJob because another nudge was scheduled";
      return;  // Another nudge must have been scheduled in in the meantime.
    }
    pending_nudge_.reset();

    // Create the session with the latest model safe table and use it to purge
    // and update any disabled or modified entries in the job.
    scoped_ptr<SyncSession> session(CreateSyncSession(job.session->source()));

    job.session->RebaseRoutingInfoWithLatest(session.get());
  }
  VLOG(1) << "SyncerThread(" << this << ")" << " DoSyncSessionJob. job purpose "
          << job.purpose;

  SyncerStep begin(SYNCER_BEGIN);
  SyncerStep end(SYNCER_END);
  SetSyncerStepsForPurpose(job.purpose, &begin, &end);

  bool has_more_to_sync = true;
  while (ShouldRunJob(job) && has_more_to_sync) {
    VLOG(1) << "SyncerThread(" << this << ")"
            << " SyncerThread: Calling SyncShare.";
    // Synchronously perform the sync session from this thread.
    syncer_->SyncShare(job.session.get(), begin, end);
    has_more_to_sync = job.session->HasMoreToSync();
    if (has_more_to_sync)
      job.session->ResetTransientState();
  }
  VLOG(1) << "SyncerThread(" << this << ")"
          << " SyncerThread: Done SyncShare looping.";
  FinishSyncSessionJob(job);
}

void SyncerThread::UpdateCarryoverSessionState(const SyncSessionJob& old_job) {
  if (old_job.purpose == SyncSessionJob::CONFIGURATION) {
    // Whatever types were part of a configuration task will have had updates
    // downloaded.  For that reason, we make sure they get recorded in the
    // event that they get disabled at a later time.
    ModelSafeRoutingInfo r(session_context_->previous_session_routing_info());
    if (!r.empty()) {
      ModelSafeRoutingInfo temp_r;
      ModelSafeRoutingInfo old_info(old_job.session->routing_info());
      std::set_union(r.begin(), r.end(), old_info.begin(), old_info.end(),
          std::insert_iterator<ModelSafeRoutingInfo>(temp_r, temp_r.begin()));
      session_context_->set_previous_session_routing_info(temp_r);
    }
  } else {
    session_context_->set_previous_session_routing_info(
        old_job.session->routing_info());
  }
}

void SyncerThread::FinishSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  // Update timing information for how often datatypes are triggering nudges.
  base::TimeTicks now = TimeTicks::Now();
  if (!last_sync_session_end_time_.is_null()) {
    ModelTypePayloadMap::const_iterator iter;
    for (iter = job.session->source().types.begin();
         iter != job.session->source().types.end();
         ++iter) {
      syncable::PostTimeToTypeHistogram(iter->first,
                                        now - last_sync_session_end_time_);
    }
  }
  last_sync_session_end_time_ = now;
  UpdateCarryoverSessionState(job);
  if (IsSyncingCurrentlySilenced()) {
    VLOG(1) << "SyncerThread(" << this << ")"
            << " We are currently throttled. So not scheduling the next sync.";
    SaveJob(job);
    return;  // Nothing to do.
  }

  VLOG(1) << "SyncerThread(" << this << ")"
          << " Updating the next polling time after SyncMain";
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
  VLOG(1) << "SyncerThread(" << this << ")" << " syncer has work to do: "
          << work_to_do;

  AdjustPolling(&old_job);

  // TODO(tim): Old impl had special code if notifications disabled. Needed?
  if (!work_to_do) {
    // Success implies backoff relief.  Note that if this was a "one-off" job
    // (i.e. purpose == SyncSessionJob::CLEAR_USER_DATA), if there was
    // work_to_do before it ran this wont have changed, as jobs like this don't
    // run a full sync cycle.  So we don't need special code here.
    wait_interval_.reset();
    VLOG(1) << "SyncerThread(" << this << ")"
            << " Job suceeded so not scheduling more jobs";
    return;
  }

  if (old_job.session->source().updates_source ==
      GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION) {
    VLOG(1) << "SyncerThread(" << this << ")"
            << " Job failed with source continuation";
    // We don't seem to have made forward progress. Start or extend backoff.
    HandleConsecutiveContinuationError(old_job);
  } else if (IsBackingOff()) {
    VLOG(1) << "SyncerThread(" << this << ")"
            << " A nudge during backoff failed";
    // We weren't continuing but we're in backoff; must have been a nudge.
    DCHECK_EQ(SyncSessionJob::NUDGE, old_job.purpose);
    DCHECK(!wait_interval_->had_nudge);
    wait_interval_->had_nudge = true;
    wait_interval_->timer.Reset();
  } else {
    VLOG(1) << "SyncerThread(" << this << ")"
            << " Failed. Schedule a job with continuation as source";
    // We weren't continuing and we aren't in backoff.  Schedule a normal
    // continuation.
    if (old_job.purpose == SyncSessionJob::CONFIGURATION) {
      ScheduleConfigImpl(old_job.session->routing_info(),
          old_job.session->workers(),
          GetUpdatesFromNudgeSource(NUDGE_SOURCE_CONTINUATION));
    } else  {
      // For all other purposes(nudge and poll) we schedule a retry nudge.
      ScheduleNudgeImpl(TimeDelta::FromSeconds(0),
                        GetUpdatesFromNudgeSource(NUDGE_SOURCE_CONTINUATION),
                        old_job.session->source().types, false, FROM_HERE);
    }
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
  // This if conditions should be compiled out in retail builds.
  if (IsBackingOff()) {
    DCHECK(wait_interval_->timer.IsRunning() || old_job.is_canary_job);
  }

  TimeDelta length = delay_provider_->GetDelay(
      IsBackingOff() ? wait_interval_->length : TimeDelta::FromSeconds(1));

  VLOG(1) << "SyncerThread(" << this << ")"
          << " In handle continuation error. Old job purpose is "
          << old_job.purpose << " . The time delta(ms) is "
          << length.InMilliseconds();

  // This will reset the had_nudge variable as well.
  wait_interval_.reset(new WaitInterval(WaitInterval::EXPONENTIAL_BACKOFF,
                                        length));
  if (old_job.purpose == SyncSessionJob::CONFIGURATION) {
    SyncSession* old = old_job.session.get();
    SyncSession* s(new SyncSession(session_context_.get(), this,
        old->source(), old->routing_info(), old->workers()));
    SyncSessionJob job(old_job.purpose, TimeTicks::Now() + length,
                        make_linked_ptr(s), false, FROM_HERE);
    wait_interval_->pending_configure_job.reset(new SyncSessionJob(job));
  } else {
    // We are not in configuration mode. So wait_interval's pending job
    // should be null.
    DCHECK(wait_interval_->pending_configure_job.get() == NULL);

    // TODO(lipalani) - handle clear user data.
    InitOrCoalescePendingJob(old_job);
  }
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
  VLOG(1) << "SyncerThread(" << this << ")" << " stop called";
  syncer_->RequestEarlyExit();  // Safe to call from any thread.
  session_context_->connection_manager()->RemoveListener(this);
  thread_.Stop();
}

void SyncerThread::DoCanaryJob() {
  VLOG(1) << "SyncerThread(" << this << ")" << " Do canary job";
  DoPendingJobIfPossible(true);
}

void SyncerThread::DoPendingJobIfPossible(bool is_canary_job) {
  SyncSessionJob* job_to_execute = NULL;
  if (mode_ == CONFIGURATION_MODE && wait_interval_.get()
      && wait_interval_->pending_configure_job.get()) {
    VLOG(1) << "SyncerThread(" << this << ")" << " Found pending configure job";
    job_to_execute = wait_interval_->pending_configure_job.get();
  } else if (mode_ == NORMAL_MODE && pending_nudge_.get()) {
    VLOG(1) << "SyncerThread(" << this << ")" << " Found pending nudge job";
    // Pending jobs mostly have time from the past. Reset it so this job
    // will get executed.
    if (pending_nudge_->scheduled_start < TimeTicks::Now())
      pending_nudge_->scheduled_start = TimeTicks::Now();

    scoped_ptr<SyncSession> session(CreateSyncSession(
        pending_nudge_->session->source()));

    // Also the routing info might have been changed since we cached the
    // pending nudge. Update it by coalescing to the latest.
    pending_nudge_->session->Coalesce(*(session.get()));
    // The pending nudge would be cleared in the DoSyncSessionJob function.
    job_to_execute = pending_nudge_.get();
  }

  if (job_to_execute != NULL) {
    VLOG(1) << "SyncerThread(" << this << ")" << " Executing pending job";
    SyncSessionJob copy = *job_to_execute;
    copy.is_canary_job = is_canary_job;
    DoSyncSessionJob(copy);
  }
}

SyncSession* SyncerThread::CreateSyncSession(const SyncSourceInfo& source) {
  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  session_context_->registrar()->GetModelSafeRoutingInfo(&routes);
  session_context_->registrar()->GetWorkers(&workers);
  SyncSourceInfo info(source);

  SyncSession* session(new SyncSession(session_context_.get(), this, info,
      routes, workers));

  return session;
}

void SyncerThread::PollTimerCallback() {
  DCHECK_EQ(MessageLoop::current(), thread_.message_loop());
  ModelSafeRoutingInfo r;
  ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromRoutingInfo(r, std::string());
  SyncSourceInfo info(GetUpdatesCallerInfo::PERIODIC, types_with_payloads);
  SyncSession* s = CreateSyncSession(info);
  ScheduleSyncSessionJob(TimeDelta::FromSeconds(0), SyncSessionJob::POLL, s,
      FROM_HERE);
}

void SyncerThread::Unthrottle() {
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);
  VLOG(1) << "SyncerThread(" << this << ")" << " Unthrottled..";
  DoCanaryJob();
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
  VLOG(1) << "SyncerThread(" << this << ")"
          << " OnShouldStopSyncingPermanently";
  syncer_->RequestEarlyExit();  // Thread-safe.
  Notify(SyncEngineEvent::STOP_SYNCING_PERMANENTLY);
}

void SyncerThread::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  thread_.message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &SyncerThread::CheckServerConnectionManagerStatus,
      event.connection_code));
}

void SyncerThread::set_notifications_enabled(bool notifications_enabled) {
  session_context_->set_notifications_enabled(notifications_enabled);
}

}  // browser_sync
