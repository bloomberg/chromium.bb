// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/sync_scheduler.h"

#include <algorithm>
#include <cstring>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "chrome/browser/sync/engine/syncer.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/protocol/proto_enum_conversions.h"
#include "chrome/browser/sync/util/logging.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {

using sessions::SyncSession;
using sessions::SyncSessionSnapshot;
using sessions::SyncSourceInfo;
using syncable::ModelTypePayloadMap;
using syncable::ModelTypeBitSet;
using sync_pb::GetUpdatesCallerInfo;

namespace {
bool ShouldRequestEarlyExit(
    const browser_sync::SyncProtocolError& error) {
  switch (error.error_type) {
    case browser_sync::SYNC_SUCCESS:
    case browser_sync::MIGRATION_DONE:
    case browser_sync::THROTTLED:
    case browser_sync::TRANSIENT_ERROR:
      return false;
    case browser_sync::NOT_MY_BIRTHDAY:
    case browser_sync::CLEAR_PENDING:
      // If we send terminate sync early then |sync_cycle_ended| notification
      // would not be sent. If there were no actions then |ACTIONABLE_ERROR|
      // notification wouldnt be sent either. Then the UI layer would be left
      // waiting forever. So assert we would send something.
      DCHECK(error.action != browser_sync::UNKNOWN_ACTION);
      return true;
    case browser_sync::INVALID_CREDENTIAL:
      // The notification for this is handled by PostAndProcessHeaders|.
      // Server does no have to send any action for this.
      return true;
    // Make the default a NOTREACHED. So if a new error is introduced we
    // think about its expected functionality.
    default:
      NOTREACHED();
      return false;
  }
}

bool IsActionableError(
    const browser_sync::SyncProtocolError& error) {
  return (error.action != browser_sync::UNKNOWN_ACTION);
}
}  // namespace

SyncScheduler::DelayProvider::DelayProvider() {}
SyncScheduler::DelayProvider::~DelayProvider() {}

SyncScheduler::WaitInterval::WaitInterval()
    : mode(UNKNOWN),
      had_nudge(false) {
}

SyncScheduler::WaitInterval::~WaitInterval() {}

#define ENUM_CASE(x) case x: return #x; break;

const char* SyncScheduler::WaitInterval::GetModeString(Mode mode) {
  switch (mode) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(EXPONENTIAL_BACKOFF);
    ENUM_CASE(THROTTLED);
  }
  NOTREACHED();
  return "";
}

SyncScheduler::SyncSessionJob::SyncSessionJob()
    : purpose(UNKNOWN),
      is_canary_job(false) {
}

SyncScheduler::SyncSessionJob::~SyncSessionJob() {}

SyncScheduler::SyncSessionJob::SyncSessionJob(SyncSessionJobPurpose purpose,
    base::TimeTicks start,
    linked_ptr<sessions::SyncSession> session, bool is_canary_job,
    const tracked_objects::Location& from_here) : purpose(purpose),
        scheduled_start(start),
        session(session),
        is_canary_job(is_canary_job),
        from_here(from_here) {
}

const char* SyncScheduler::SyncSessionJob::GetPurposeString(
    SyncScheduler::SyncSessionJob::SyncSessionJobPurpose purpose) {
  switch (purpose) {
    ENUM_CASE(UNKNOWN);
    ENUM_CASE(POLL);
    ENUM_CASE(NUDGE);
    ENUM_CASE(CLEAR_USER_DATA);
    ENUM_CASE(CONFIGURATION);
    ENUM_CASE(CLEANUP_DISABLED_TYPES);
  }
  NOTREACHED();
  return "";
}

TimeDelta SyncScheduler::DelayProvider::GetDelay(
    const base::TimeDelta& last_delay) {
  return SyncScheduler::GetRecommendedDelay(last_delay);
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

SyncScheduler::WaitInterval::WaitInterval(Mode mode, TimeDelta length)
    : mode(mode), had_nudge(false), length(length) { }

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SVLOG(verbose_level) VLOG(verbose_level) << name_ << ": "

#define SVLOG_LOC(from_here, verbose_level)             \
  VLOG_LOC(from_here, verbose_level) << name_ << ": "

namespace {

const int kDefaultSessionsCommitDelaySeconds = 10;

bool IsConfigRelatedUpdateSourceValue(
    GetUpdatesCallerInfo::GetUpdatesSource source) {
  switch (source) {
    case GetUpdatesCallerInfo::RECONFIGURATION:
    case GetUpdatesCallerInfo::MIGRATION:
    case GetUpdatesCallerInfo::NEW_CLIENT:
    case GetUpdatesCallerInfo::NEWLY_SUPPORTED_DATATYPE:
      return true;
    default:
      return false;
  }
}

}  // namespace

SyncScheduler::SyncScheduler(const std::string& name,
                             sessions::SyncSessionContext* context,
                             Syncer* syncer)
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_ptr_factory_for_weak_handle_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      weak_handle_this_(MakeWeakHandle(
          weak_ptr_factory_for_weak_handle_.GetWeakPtr())),
      name_(name),
      sync_loop_(MessageLoop::current()),
      started_(false),
      syncer_short_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultShortPollIntervalSeconds)),
      syncer_long_poll_interval_seconds_(
          TimeDelta::FromSeconds(kDefaultLongPollIntervalSeconds)),
      sessions_commit_delay_(
          TimeDelta::FromSeconds(kDefaultSessionsCommitDelaySeconds)),
      mode_(NORMAL_MODE),
      server_connection_ok_(false),
      delay_provider_(new DelayProvider()),
      syncer_(syncer),
      session_context_(context) {
  DCHECK(sync_loop_);
}

SyncScheduler::~SyncScheduler() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  StopImpl(base::Closure());
}

void SyncScheduler::CheckServerConnectionManagerStatus(
    HttpResponse::ServerConnectionCode code) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG(2) << "New server connection code: "
           << HttpResponse::GetServerConnectionCodeString(code);
  bool old_server_connection_ok = server_connection_ok_;

  // Note, be careful when adding cases here because if the SyncScheduler
  // thinks there is no valid connection as determined by this method, it
  // will drop out of *all* forward progress sync loops (it won't poll and it
  // will queue up Talk notifications but not actually call SyncShare) until
  // some external action causes a ServerConnectionManager to broadcast that
  // a valid connection has been re-established.
  if (HttpResponse::CONNECTION_UNAVAILABLE == code ||
      HttpResponse::SYNC_AUTH_ERROR == code) {
    server_connection_ok_ = false;
    SVLOG(2) << "Sync auth error or unavailable connection: "
             << "server connection is down";
  } else if (HttpResponse::SERVER_CONNECTION_OK == code) {
    server_connection_ok_ = true;
    SVLOG(2) << "Sync server connection is ok: "
             << "server connection is up, doing canary job";
    DoCanaryJob();
  }

  if (old_server_connection_ok != server_connection_ok_) {
    const char* transition =
        server_connection_ok_ ? "down -> up" : "up -> down";
    SVLOG(2) << "Server connection changed: " << transition;
  }
}

void SyncScheduler::Start(Mode mode, const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  std::string thread_name = MessageLoop::current()->thread_name();
  if (thread_name.empty())
    thread_name = "<Main thread>";
  SVLOG(2) << "Start called from thread "
           << thread_name << " with mode " << GetModeString(mode);
  if (!started_) {
    started_ = true;
    WatchConnectionManager();
    PostTask(FROM_HERE, "SendInitialSnapshot",
             base::Bind(&SyncScheduler::SendInitialSnapshot,
                        weak_ptr_factory_.GetWeakPtr()));
  }
  PostTask(FROM_HERE, "StartImpl",
           base::Bind(&SyncScheduler::StartImpl,
                      weak_ptr_factory_.GetWeakPtr(), mode, callback));
}

void SyncScheduler::SendInitialSnapshot() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  scoped_ptr<SyncSession> dummy(new SyncSession(session_context_.get(), this,
      SyncSourceInfo(), ModelSafeRoutingInfo(),
      std::vector<ModelSafeWorker*>()));
  SyncEngineEvent event(SyncEngineEvent::STATUS_CHANGED);
  sessions::SyncSessionSnapshot snapshot(dummy->TakeSnapshot());
  event.snapshot = &snapshot;
  session_context_->NotifyListeners(event);
}

void SyncScheduler::WatchConnectionManager() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ServerConnectionManager* scm = session_context_->connection_manager();
  PostTask(FROM_HERE, "CheckServerConnectionManagerStatus",
           base::Bind(&SyncScheduler::CheckServerConnectionManagerStatus,
                      weak_ptr_factory_.GetWeakPtr(),
                      scm->server_status()));
  scm->AddListener(this);
}

void SyncScheduler::StartImpl(Mode mode, const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG(2) << "In StartImpl with mode " << GetModeString(mode);

  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(!session_context_->account_name().empty());
  DCHECK(syncer_.get());
  Mode old_mode = mode_;
  mode_ = mode;
  AdjustPolling(NULL);  // Will kick start poll timer if needed.
  if (!callback.is_null())
    callback.Run();

  if (old_mode != mode_) {
    // We just changed our mode. See if there are any pending jobs that we could
    // execute in the new mode.
    DoPendingJobIfPossible(false);
  }
}

SyncScheduler::JobProcessDecision SyncScheduler::DecideWhileInWaitInterval(
    const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(wait_interval_.get());
  DCHECK_NE(job.purpose, SyncSessionJob::CLEAR_USER_DATA);
  DCHECK_NE(job.purpose, SyncSessionJob::CLEANUP_DISABLED_TYPES);

  SVLOG(2) << "DecideWhileInWaitInterval with WaitInterval mode "
           << WaitInterval::GetModeString(wait_interval_->mode)
           << (wait_interval_->had_nudge ? " (had nudge)" : "")
           << (job.is_canary_job ? " (canary)" : "");

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
    if (!job.is_canary_job)
      return wait_interval_->had_nudge ? DROP : CONTINUE;
    else // We are here because timer ran out. So retry.
      return CONTINUE;
  }
  return job.is_canary_job ? CONTINUE : SAVE;
}

SyncScheduler::JobProcessDecision SyncScheduler::DecideOnJob(
    const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (job.purpose == SyncSessionJob::CLEAR_USER_DATA ||
      job.purpose == SyncSessionJob::CLEANUP_DISABLED_TYPES)
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
    SVLOG(2) << "Dropping job because of freshness";
    return DROP;
  }

  if (server_connection_ok_)
    return CONTINUE;

  SVLOG(2) << "Bad server connection. Using that to decide on job.";
  return job.purpose == SyncSessionJob::NUDGE ? SAVE : DROP;
}

void SyncScheduler::InitOrCoalescePendingJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(job.purpose != SyncSessionJob::CONFIGURATION);
  if (pending_nudge_.get() == NULL) {
    SVLOG(2) << "Creating a pending nudge job";
    SyncSession* s = job.session.get();
    scoped_ptr<SyncSession> session(new SyncSession(s->context(),
        s->delegate(), s->source(), s->routing_info(), s->workers()));

    SyncSessionJob new_job(SyncSessionJob::NUDGE, job.scheduled_start,
        make_linked_ptr(session.release()), false, job.from_here);
    pending_nudge_.reset(new SyncSessionJob(new_job));

    return;
  }

  SVLOG(2) << "Coalescing a pending nudge";
  pending_nudge_->session->Coalesce(*(job.session.get()));
  pending_nudge_->scheduled_start = job.scheduled_start;

  // Unfortunately the nudge location cannot be modified. So it stores the
  // location of the first caller.
}

bool SyncScheduler::ShouldRunJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(started_);

  JobProcessDecision decision = DecideOnJob(job);
  SVLOG(2) << "Should run "
           << SyncSessionJob::GetPurposeString(job.purpose)
           << " job in mode " << GetModeString(mode_)
           << ": " << GetDecisionString(decision);
  if (decision != SAVE)
    return decision == CONTINUE;

  DCHECK(job.purpose == SyncSessionJob::NUDGE || job.purpose ==
      SyncSessionJob::CONFIGURATION);

  SaveJob(job);
  return false;
}

void SyncScheduler::SaveJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK_NE(job.purpose, SyncSessionJob::CLEAR_USER_DATA);
  // TODO(sync): Should we also check that job.purpose !=
  // CLEANUP_DISABLED_TYPES?  (See http://crbug.com/90868.)
  if (job.purpose == SyncSessionJob::NUDGE) {
    SVLOG(2) << "Saving a nudge job";
    InitOrCoalescePendingJob(job);
  } else if (job.purpose == SyncSessionJob::CONFIGURATION){
    SVLOG(2) << "Saving a configuration job";
    DCHECK(wait_interval_.get());
    DCHECK(mode_ == CONFIGURATION_MODE);

    SyncSession* old = job.session.get();
    SyncSession* s(new SyncSession(session_context_.get(), this,
        old->source(), old->routing_info(), old->workers()));
    SyncSessionJob new_job(job.purpose, TimeTicks::Now(),
                           make_linked_ptr(s), false, job.from_here);
    wait_interval_->pending_configure_job.reset(new SyncSessionJob(new_job));
  } // drop the rest.
  // TODO(sync): Is it okay to drop the rest?  It's weird that
  // SaveJob() only does what it says sometimes.  (See
  // http://crbug.com/90868.)
}

// Functor for std::find_if to search by ModelSafeGroup.
struct ModelSafeWorkerGroupIs {
  explicit ModelSafeWorkerGroupIs(ModelSafeGroup group) : group(group) {}
  bool operator()(ModelSafeWorker* w) {
    return group == w->GetModelSafeGroup();
  }
  ModelSafeGroup group;
};

void SyncScheduler::ScheduleClearUserData() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  PostTask(FROM_HERE, "ScheduleClearUserDataImpl",
           base::Bind(&SyncScheduler::ScheduleClearUserDataImpl,
                      weak_ptr_factory_.GetWeakPtr()));
}

// TODO(sync): Remove the *Impl methods for the other Schedule*
// functions, too.
void SyncScheduler::ScheduleCleanupDisabledTypes() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ScheduleSyncSessionJob(
      TimeDelta::FromSeconds(0), SyncSessionJob::CLEANUP_DISABLED_TYPES,
      CreateSyncSession(SyncSourceInfo()), FROM_HERE);
}

void SyncScheduler::ScheduleNudge(
    const TimeDelta& delay,
    NudgeSource source, const ModelTypeBitSet& types,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay " << delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "types " << syncable::ModelTypeBitSetToString(types);

  ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromBitSet(types, std::string());
  PostTask(nudge_location, "ScheduleNudgeImpl",
           base::Bind(&SyncScheduler::ScheduleNudgeImpl,
                      weak_ptr_factory_.GetWeakPtr(),
                      delay,
                      GetUpdatesFromNudgeSource(source),
                      types_with_payloads,
                      false,
                      nudge_location));
}

void SyncScheduler::ScheduleNudgeWithPayloads(
    const TimeDelta& delay,
    NudgeSource source, const ModelTypePayloadMap& types_with_payloads,
    const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG_LOC(nudge_location, 2)
      << "Nudge scheduled with delay " << delay.InMilliseconds() << " ms, "
      << "source " << GetNudgeSourceString(source) << ", "
      << "payloads "
      << syncable::ModelTypePayloadMapToString(types_with_payloads);

  PostTask(nudge_location, "ScheduleNudgeImpl",
           base::Bind(&SyncScheduler::ScheduleNudgeImpl,
                      weak_ptr_factory_.GetWeakPtr(),
                      delay,
                      GetUpdatesFromNudgeSource(source),
                      types_with_payloads,
                      false,
                      nudge_location));
}

void SyncScheduler::ScheduleClearUserDataImpl() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ScheduleSyncSessionJob(
      TimeDelta::FromSeconds(0), SyncSessionJob::CLEAR_USER_DATA,
      CreateSyncSession(SyncSourceInfo()), FROM_HERE);
}

void SyncScheduler::ScheduleNudgeImpl(
    const TimeDelta& delay,
    GetUpdatesCallerInfo::GetUpdatesSource source,
    const ModelTypePayloadMap& types_with_payloads,
    bool is_canary_job, const tracked_objects::Location& nudge_location) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  SVLOG_LOC(nudge_location, 2)
      << "In ScheduleNudgeImpl with delay "
      << delay.InMilliseconds() << " ms, "
      << "source " << GetUpdatesSourceString(source) << ", "
      << "payloads "
      << syncable::ModelTypePayloadMapToString(types_with_payloads)
      << (is_canary_job ? " (canary)" : "");

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
      SVLOG(2) << "Dropping the nudge because we are in backoff";
      return;
    }

    SVLOG(2) << "Coalescing pending nudge";
    pending_nudge_->session->Coalesce(*(job.session.get()));

    if (!IsBackingOff()) {
      SVLOG(2) << "Dropping a nudge because"
               << " we are not in backoff and the job was coalesced";
      return;
    } else {
      SVLOG(2) << "Rescheduling pending nudge";
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

void SyncScheduler::ScheduleConfig(
    const ModelTypeBitSet& types,
    GetUpdatesCallerInfo::GetUpdatesSource source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(IsConfigRelatedUpdateSourceValue(source));
  SVLOG(2) << "Scheduling a config";
  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  GetModelSafeParamsForTypes(types, session_context_->registrar(),
                             &routes, &workers);

  PostTask(FROM_HERE, "ScheduleConfigImpl",
           base::Bind(&SyncScheduler::ScheduleConfigImpl,
                      weak_ptr_factory_.GetWeakPtr(),
                      routes,
                      workers,
                      source));
}

void SyncScheduler::ScheduleConfigImpl(
    const ModelSafeRoutingInfo& routing_info,
    const std::vector<ModelSafeWorker*>& workers,
    const sync_pb::GetUpdatesCallerInfo::GetUpdatesSource source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

  SVLOG(2) << "In ScheduleConfigImpl";
  // TODO(tim): config-specific GetUpdatesCallerInfo value?
  SyncSession* session = new SyncSession(session_context_.get(), this,
      SyncSourceInfo(source,
          syncable::ModelTypePayloadMapFromRoutingInfo(
              routing_info, std::string())),
      routing_info, workers);
  ScheduleSyncSessionJob(TimeDelta::FromSeconds(0),
      SyncSessionJob::CONFIGURATION, session, FROM_HERE);
}

const char* SyncScheduler::GetModeString(SyncScheduler::Mode mode) {
  switch (mode) {
    ENUM_CASE(CONFIGURATION_MODE);
    ENUM_CASE(NORMAL_MODE);
  }
  return "";
}

const char* SyncScheduler::GetDecisionString(
    SyncScheduler::JobProcessDecision mode) {
  switch (mode) {
    ENUM_CASE(CONTINUE);
    ENUM_CASE(SAVE);
    ENUM_CASE(DROP);
  }
  return "";
}

void SyncScheduler::PostTask(
    const tracked_objects::Location& from_here,
    const char* name, const base::Closure& task) {
  SVLOG_LOC(from_here, 3) << "Posting " << name << " task";
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (!started_) {
    SVLOG(1) << "Not posting task as scheduler is stopped.";
    return;
  }
  sync_loop_->PostTask(from_here, task);
}

void SyncScheduler::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const char* name, const base::Closure& task, int64 delay_ms) {
  SVLOG_LOC(from_here, 3) << "Posting " << name << " task with "
                          << delay_ms << " ms delay";
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (!started_) {
    SVLOG(1) << "Not posting task as scheduler is stopped.";
    return;
  }
  sync_loop_->PostDelayedTask(from_here, task, delay_ms);
}

void SyncScheduler::ScheduleSyncSessionJob(
    const base::TimeDelta& delay,
    SyncSessionJob::SyncSessionJobPurpose purpose,
    sessions::SyncSession* session,
    const tracked_objects::Location& from_here) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG_LOC(from_here, 2)
      << "In ScheduleSyncSessionJob with "
      << SyncSessionJob::GetPurposeString(purpose)
      << " job and " << delay.InMilliseconds() << " ms delay";

  SyncSessionJob job(purpose, TimeTicks::Now() + delay,
                     make_linked_ptr(session), false, from_here);
  if (purpose == SyncSessionJob::NUDGE) {
    SVLOG_LOC(from_here, 2) << "Resetting pending_nudge";
    DCHECK(!pending_nudge_.get() || pending_nudge_->session.get() == session);
    pending_nudge_.reset(new SyncSessionJob(job));
  }
  PostDelayedTask(from_here, "DoSyncSessionJob",
                  base::Bind(&SyncScheduler::DoSyncSessionJob,
                             weak_ptr_factory_.GetWeakPtr(),
                             job),
                  delay.InMilliseconds());
}

void SyncScheduler::SetSyncerStepsForPurpose(
    SyncSessionJob::SyncSessionJobPurpose purpose,
    SyncerStep* start, SyncerStep* end) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  switch (purpose) {
    case SyncSessionJob::CONFIGURATION:
      *start = DOWNLOAD_UPDATES;
      *end = APPLY_UPDATES;
      return;
    case SyncSessionJob::CLEAR_USER_DATA:
      *start = CLEAR_PRIVATE_DATA;
      *end = CLEAR_PRIVATE_DATA;
       return;
    case SyncSessionJob::NUDGE:
    case SyncSessionJob::POLL:
      *start = SYNCER_BEGIN;
      *end = SYNCER_END;
      return;
    case SyncSessionJob::CLEANUP_DISABLED_TYPES:
      *start = CLEANUP_DISABLED_TYPES;
      *end = CLEANUP_DISABLED_TYPES;
      return;
    default:
      NOTREACHED();
      *start = SYNCER_END;
      *end = SYNCER_END;
      return;
  }
}

void SyncScheduler::DoSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (!ShouldRunJob(job)) {
    SLOG(WARNING)
        << "Not executing "
        << SyncSessionJob::GetPurposeString(job.purpose) << " job from "
        << GetUpdatesSourceString(job.session->source().updates_source);
    return;
  }

  if (job.purpose == SyncSessionJob::NUDGE) {
    if (pending_nudge_.get() == NULL ||
        pending_nudge_->session != job.session) {
      SVLOG(2) << "Dropping a nudge in "
               << "DoSyncSessionJob because another nudge was scheduled";
      return;  // Another nudge must have been scheduled in in the meantime.
    }
    pending_nudge_.reset();

    // Create the session with the latest model safe table and use it to purge
    // and update any disabled or modified entries in the job.
    scoped_ptr<SyncSession> session(CreateSyncSession(job.session->source()));

    job.session->RebaseRoutingInfoWithLatest(session.get());
  }
  SVLOG(2) << "DoSyncSessionJob with "
           << SyncSessionJob::GetPurposeString(job.purpose) << " job";

  SyncerStep begin(SYNCER_END);
  SyncerStep end(SYNCER_END);
  SetSyncerStepsForPurpose(job.purpose, &begin, &end);

  bool has_more_to_sync = true;
  while (ShouldRunJob(job) && has_more_to_sync) {
    SVLOG(2) << "Calling SyncShare.";
    // Synchronously perform the sync session from this thread.
    syncer_->SyncShare(job.session.get(), begin, end);
    has_more_to_sync = job.session->HasMoreToSync();
    if (has_more_to_sync)
      job.session->ResetTransientState();
  }
  SVLOG(2) << "Done SyncShare looping.";

  FinishSyncSessionJob(job);
}

void SyncScheduler::UpdateCarryoverSessionState(
    const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
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

void SyncScheduler::FinishSyncSessionJob(const SyncSessionJob& job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
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
    SVLOG(2) << "We are currently throttled; not scheduling the next sync.";
    // TODO(sync): Investigate whether we need to check job.purpose
    // here; see DCHECKs in SaveJob().  (See http://crbug.com/90868.)
    SaveJob(job);
    return;  // Nothing to do.
  }

  SVLOG(2) << "Updating the next polling time after SyncMain";
  ScheduleNextSync(job);
}

void SyncScheduler::ScheduleNextSync(const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK(!old_job.session->HasMoreToSync());
  // Note: |num_server_changes_remaining| > 0 here implies that we received a
  // broken response while trying to download all updates, because the Syncer
  // will loop until this value is exhausted. Also, if unsynced_handles exist
  // but HasMoreToSync is false, this implies that the Syncer determined no
  // forward progress was possible at this time (an error, such as an HTTP
  // 500, is likely to have occurred during commit).
  int num_server_changes_remaining =
      old_job.session->status_controller().num_server_changes_remaining();
  size_t num_unsynced_handles =
      old_job.session->status_controller().unsynced_handles().size();
  const bool work_to_do =
      num_server_changes_remaining > 0 || num_unsynced_handles > 0;
  SVLOG(2) << "num server changes remaining: " << num_server_changes_remaining
           << ", num unsynced handles: " << num_unsynced_handles
           << ", syncer has work to do: " << work_to_do;

  AdjustPolling(&old_job);

  // TODO(tim): Old impl had special code if notifications disabled. Needed?
  if (!work_to_do) {
    // Success implies backoff relief.  Note that if this was a
    // "one-off" job (i.e. purpose ==
    // SyncSessionJob::{CLEAR_USER_DATA,CLEANUP_DISABLED_TYPES}), if
    // there was work_to_do before it ran this wont have changed, as
    // jobs like this don't run a full sync cycle.  So we don't need
    // special code here.
    wait_interval_.reset();
    SVLOG(2) << "Job succeeded so not scheduling more jobs";
    return;
  }

  // We are in backoff mode and our time did not run out. That means we had
  // a local change, notification from server or a network connection change
  // notification. In any case set had_nudge = true so we dont retry next
  // nudge. Note: we will keep retrying network connection changes though as
  // they are treated as canary jobs. Also we check the mode here because
  // we want to do this only in normal mode. For config mode jobs we dont
  // have anything similar to had_nudge.
  if (IsBackingOff() && wait_interval_->timer.IsRunning() &&
      mode_ == NORMAL_MODE) {
    SVLOG(2) << "A nudge during backoff failed";
    // We weren't continuing but we're in backoff; must have been a nudge.
    DCHECK_EQ(SyncSessionJob::NUDGE, old_job.purpose);
    DCHECK(!wait_interval_->had_nudge);
    wait_interval_->had_nudge = true;
    // Old job did not finish. So make it the pending job.
    InitOrCoalescePendingJob(old_job);
    // Resume waiting.
    RestartWaiting();
  } else if (old_job.session->source().updates_source ==
             GetUpdatesCallerInfo::SYNC_CYCLE_CONTINUATION) {
    SVLOG(2) << "Job failed with source continuation";
    // We don't seem to have made forward progress. Start or extend backoff.
    HandleConsecutiveContinuationError(old_job);
  } else {
    SVLOG(2) << "Failed. Schedule a job with continuation as source";
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

void SyncScheduler::AdjustPolling(const SyncSessionJob* old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);

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
  poll_timer_.Start(FROM_HERE, poll, this, &SyncScheduler::PollTimerCallback);
}

void SyncScheduler::RestartWaiting() {
  CHECK(wait_interval_.get());
  wait_interval_->timer.Stop();
  wait_interval_->timer.Start(FROM_HERE, wait_interval_->length,
                              this, &SyncScheduler::DoCanaryJob);
}

void SyncScheduler::HandleConsecutiveContinuationError(
    const SyncSessionJob& old_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (DCHECK_IS_ON()) {
    if (IsBackingOff()) {
      DCHECK(wait_interval_->timer.IsRunning() || old_job.is_canary_job);
    }
  }

  TimeDelta length = delay_provider_->GetDelay(
      IsBackingOff() ? wait_interval_->length : TimeDelta::FromSeconds(1));

  SVLOG(2) << "In handle continuation error with "
           << SyncSessionJob::GetPurposeString(old_job.purpose)
           << " job. The time delta(ms) is "
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
  RestartWaiting();
}

// static
TimeDelta SyncScheduler::GetRecommendedDelay(const TimeDelta& last_delay) {
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

void SyncScheduler::RequestStop(const base::Closure& callback) {
  syncer_->RequestEarlyExit();  // Safe to call from any thread.
  DCHECK(weak_handle_this_.IsInitialized());
  SVLOG(3) << "Posting StopImpl";
  weak_handle_this_.Call(FROM_HERE,
                         &SyncScheduler::StopImpl,
                         callback);
}

void SyncScheduler::StopImpl(const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG(2) << "StopImpl called";

  // Kill any in-flight method calls.
  weak_ptr_factory_.InvalidateWeakPtrs();
  wait_interval_.reset();
  poll_timer_.Stop();
  if (started_) {
    session_context_->connection_manager()->RemoveListener(this);
    started_ = false;
  }
  if (!callback.is_null())
    callback.Run();
}

void SyncScheduler::DoCanaryJob() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG(2) << "Do canary job";
  DoPendingJobIfPossible(true);
}

void SyncScheduler::DoPendingJobIfPossible(bool is_canary_job) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SyncSessionJob* job_to_execute = NULL;
  if (mode_ == CONFIGURATION_MODE && wait_interval_.get()
      && wait_interval_->pending_configure_job.get()) {
    SVLOG(2) << "Found pending configure job";
    job_to_execute = wait_interval_->pending_configure_job.get();
  } else if (mode_ == NORMAL_MODE && pending_nudge_.get()) {
    SVLOG(2) << "Found pending nudge job";
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
    SVLOG(2) << "Executing pending job";
    SyncSessionJob copy = *job_to_execute;
    copy.is_canary_job = is_canary_job;
    DoSyncSessionJob(copy);
  }
}

SyncSession* SyncScheduler::CreateSyncSession(const SyncSourceInfo& source) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ModelSafeRoutingInfo routes;
  std::vector<ModelSafeWorker*> workers;
  session_context_->registrar()->GetModelSafeRoutingInfo(&routes);
  VLOG(2) << "Creating sync session with routes "
          << ModelSafeRoutingInfoToString(routes);
  session_context_->registrar()->GetWorkers(&workers);
  SyncSourceInfo info(source);

  SyncSession* session(new SyncSession(session_context_.get(), this, info,
      routes, workers));

  return session;
}

void SyncScheduler::PollTimerCallback() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ModelSafeRoutingInfo r;
  ModelTypePayloadMap types_with_payloads =
      syncable::ModelTypePayloadMapFromRoutingInfo(r, std::string());
  SyncSourceInfo info(GetUpdatesCallerInfo::PERIODIC, types_with_payloads);
  SyncSession* s = CreateSyncSession(info);
  ScheduleSyncSessionJob(TimeDelta::FromSeconds(0), SyncSessionJob::POLL, s,
      FROM_HERE);
}

void SyncScheduler::Unthrottle() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  DCHECK_EQ(WaitInterval::THROTTLED, wait_interval_->mode);
  SVLOG(2) << "Unthrottled.";
  DoCanaryJob();
  wait_interval_.reset();
}

void SyncScheduler::Notify(SyncEngineEvent::EventCause cause) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  session_context_->NotifyListeners(SyncEngineEvent(cause));
}

bool SyncScheduler::IsBackingOff() const {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::EXPONENTIAL_BACKOFF;
}

void SyncScheduler::OnSilencedUntil(const base::TimeTicks& silenced_until) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  wait_interval_.reset(new WaitInterval(WaitInterval::THROTTLED,
                                        silenced_until - TimeTicks::Now()));
  wait_interval_->timer.Start(FROM_HERE, wait_interval_->length, this,
      &SyncScheduler::Unthrottle);
}

bool SyncScheduler::IsSyncingCurrentlySilenced() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return wait_interval_.get() && wait_interval_->mode ==
      WaitInterval::THROTTLED;
}

void SyncScheduler::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  syncer_short_poll_interval_seconds_ = new_interval;
}

void SyncScheduler::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  syncer_long_poll_interval_seconds_ = new_interval;
}

void SyncScheduler::OnReceivedSessionsCommitDelay(
    const base::TimeDelta& new_delay) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  sessions_commit_delay_ = new_delay;
}

void SyncScheduler::OnShouldStopSyncingPermanently() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG(2) << "OnShouldStopSyncingPermanently";
  syncer_->RequestEarlyExit();  // Thread-safe.
  Notify(SyncEngineEvent::STOP_SYNCING_PERMANENTLY);
}

void SyncScheduler::OnActionableError(
    const sessions::SyncSessionSnapshot& snap) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  SVLOG(2) << "OnActionableError";
  SyncEngineEvent event(SyncEngineEvent::ACTIONABLE_ERROR);
  sessions::SyncSessionSnapshot snapshot(snap);
  event.snapshot = &snapshot;
  session_context_->NotifyListeners(event);
}

void SyncScheduler::OnSyncProtocolError(
    const sessions::SyncSessionSnapshot& snapshot) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  if (ShouldRequestEarlyExit(snapshot.errors.sync_protocol_error)) {
    SVLOG(2) << "Sync Scheduler requesting early exit.";
    syncer_->RequestEarlyExit();  // Thread-safe.
  }
  if (IsActionableError(snapshot.errors.sync_protocol_error))
    OnActionableError(snapshot);
}


void SyncScheduler::OnServerConnectionEvent(
    const ServerConnectionEvent& event) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  PostTask(FROM_HERE, "CheckServerConnectionManagerStatus",
           base::Bind(&SyncScheduler::CheckServerConnectionManagerStatus,
                      weak_ptr_factory_.GetWeakPtr(),
                      event.connection_code));
}

void SyncScheduler::set_notifications_enabled(bool notifications_enabled) {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  session_context_->set_notifications_enabled(notifications_enabled);
}

base::TimeDelta SyncScheduler::sessions_commit_delay() const {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  return sessions_commit_delay_;
}

#undef SVLOG_LOC

#undef SVLOG

#undef SLOG

#undef ENUM_CASE

}  // browser_sync
