// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// Description of the life cycle of a instance of MetricsService.
//
//  OVERVIEW
//
// A MetricsService instance is typically created at application startup.  It is
// the central controller for the acquisition of log data, and the automatic
// transmission of that log data to an external server.  Its major job is to
// manage logs, grouping them for transmission, and transmitting them.  As part
// of its grouping, MS finalizes logs by including some just-in-time gathered
// memory statistics, snapshotting the current stats of numerous histograms,
// closing the logs, translating to protocol buffer format, and compressing the
// results for transmission.  Transmission includes submitting a compressed log
// as data in a URL-post, and retransmitting (or retaining at process
// termination) if the attempted transmission failed.  Retention across process
// terminations is done using the the PrefServices facilities. The retained logs
// (the ones that never got transmitted) are compressed and base64-encoded
// before being persisted.
//
// Logs fall into one of two categories: "initial logs," and "ongoing logs."
// There is at most one initial log sent for each complete run of Chrome (from
// startup, to browser shutdown).  An initial log is generally transmitted some
// short time (1 minute?) after startup, and includes stats such as recent crash
// info, the number and types of plugins, etc.  The external server's response
// to the initial log conceptually tells this MS if it should continue
// transmitting logs (during this session). The server response can actually be
// much more detailed, and always includes (at a minimum) how often additional
// ongoing logs should be sent.
//
// After the above initial log, a series of ongoing logs will be transmitted.
// The first ongoing log actually begins to accumulate information stating when
// the MS was first constructed.  Note that even though the initial log is
// commonly sent a full minute after startup, the initial log does not include
// much in the way of user stats.   The most common interlog period (delay)
// is 30 minutes. That time period starts when the first user action causes a
// logging event.  This means that if there is no user action, there may be long
// periods without any (ongoing) log transmissions.  Ongoing logs typically
// contain very detailed records of user activities (ex: opened tab, closed
// tab, fetched URL, maximized window, etc.)  In addition, just before an
// ongoing log is closed out, a call is made to gather memory statistics.  Those
// memory statistics are deposited into a histogram, and the log finalization
// code is then called.  In the finalization, a call to a Histogram server
// acquires a list of all local histograms that have been flagged for upload
// to the UMA server.  The finalization also acquires the most recent number
// of page loads, along with any counts of renderer or plugin crashes.
//
// When the browser shuts down, there will typically be a fragment of an ongoing
// log that has not yet been transmitted.  At shutdown time, that fragment is
// closed (including snapshotting histograms), and persisted, for potential
// transmission during a future run of the product.
//
// There are two slightly abnormal shutdown conditions.  There is a
// "disconnected scenario," and a "really fast startup and shutdown" scenario.
// In the "never connected" situation, the user has (during the running of the
// process) never established an internet connection.  As a result, attempts to
// transmit the initial log have failed, and a lot(?) of data has accumulated in
// the ongoing log (which didn't yet get closed, because there was never even a
// contemplation of sending it).  There is also a kindred "lost connection"
// situation, where a loss of connection prevented an ongoing log from being
// transmitted, and a (still open) log was stuck accumulating a lot(?) of data,
// while the earlier log retried its transmission.  In both of these
// disconnected situations, two logs need to be, and are, persistently stored
// for future transmission.
//
// The other unusual shutdown condition, termed "really fast startup and
// shutdown," involves the deliberate user termination of the process before
// the initial log is even formed or transmitted. In that situation, no logging
// is done, but the historical crash statistics remain (unlogged) for inclusion
// in a future run's initial log.  (i.e., we don't lose crash stats).
//
// With the above overview, we can now describe the state machine's various
// states, based on the State enum specified in the state_ member.  Those states
// are:
//
//  INITIALIZED,          // Constructor was called.
//  INIT_TASK_SCHEDULED,  // Waiting for deferred init tasks to finish.
//  INIT_TASK_DONE,       // Waiting for timer to send initial log.
//  SENDING_LOGS,         // Sending logs and creating new ones when we run out.
//
// In more detail, we have:
//
//    INITIALIZED,            // Constructor was called.
// The MS has been constructed, but has taken no actions to compose the
// initial log.
//
//    INIT_TASK_SCHEDULED,    // Waiting for deferred init tasks to finish.
// Typically about 30 seconds after startup, a task is sent to a second thread
// (the file thread) to perform deferred (lower priority and slower)
// initialization steps such as getting the list of plugins.  That task will
// (when complete) make an async callback (via a Task) to indicate the
// completion.
//
//    INIT_TASK_DONE,         // Waiting for timer to send initial log.
// The callback has arrived, and it is now possible for an initial log to be
// created.  This callback typically arrives back less than one second after
// the deferred init task is dispatched.
//
//    SENDING_LOGS,  // Sending logs an creating new ones when we run out.
// Logs from previous sessions have been loaded, and initial logs have been
// created (an optional stability log and the first metrics log).  We will
// send all of these logs, and when run out, we will start cutting new logs
// to send.  We will also cut a new log if we expect a shutdown.
//
// The progression through the above states is simple, and sequential.
// States proceed from INITIAL to SENDING_LOGS, and remain in the latter until
// shutdown.
//
// Also note that whenever we successfully send a log, we mirror the list
// of logs into the PrefService. This ensures that IF we crash, we won't start
// up and retransmit our old logs again.
//
// Due to race conditions, it is always possible that a log file could be sent
// twice.  For example, if a log file is sent, but not yet acknowledged by
// the external server, and the user shuts down, then a copy of the log may be
// saved for re-transmission.  These duplicates could be filtered out server
// side, but are not expected to be a significant problem.
//
//
//------------------------------------------------------------------------------

#include "components/metrics/metrics_service.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "build/build_config.h"
#include "components/metrics/data_use_tracker.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_manager.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_scheduler.h"
#include "components/metrics/metrics_rotation_scheduler.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/metrics_upload_scheduler.h"
#include "components/metrics/stability_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"

namespace metrics {

namespace {

// This drops records if the number of events (user action and omnibox) exceeds
// the kEventLimit.
const base::Feature kUMAThrottleEvents{"UMAThrottleEvents",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// The delay, in seconds, after starting recording before doing expensive
// initialization work.
#if defined(OS_ANDROID) || defined(OS_IOS)
// On mobile devices, a significant portion of sessions last less than a minute.
// Use a shorter timer on these platforms to avoid losing data.
// TODO(dfalcantara): To avoid delaying startup, tighten up initialization so
//                    that it occurs after the user gets their initial page.
const int kInitializationDelaySeconds = 5;
#else
const int kInitializationDelaySeconds = 30;
#endif

// The maximum number of events in a log uploaded to the UMA server.
const int kEventLimit = 2400;

// If an upload fails, and the transmission was over this byte count, then we
// will discard the log, and not try to retransmit it.  We also don't persist
// the log to the prefs for transmission during the next chrome session if this
// limit is exceeded.
const size_t kUploadLogAvoidRetransmitSize = 100 * 1024;

enum ResponseStatus {
  UNKNOWN_FAILURE,
  SUCCESS,
  BAD_REQUEST,  // Invalid syntax or log too large.
  NO_RESPONSE,
  NUM_RESPONSE_STATUSES
};

ResponseStatus ResponseCodeToStatus(int response_code) {
  switch (response_code) {
    case -1:
      return NO_RESPONSE;
    case 200:
      return SUCCESS;
    case 400:
      return BAD_REQUEST;
    default:
      return UNKNOWN_FAILURE;
  }
}

#if defined(OS_ANDROID) || defined(OS_IOS)
void MarkAppCleanShutdownAndCommit(CleanExitBeacon* clean_exit_beacon,
                                   PrefService* local_state) {
  clean_exit_beacon->WriteBeaconValue(true);
  ExecutionPhaseManager(local_state).OnAppEnterBackground();
  // Start writing right away (write happens on a different thread).
  local_state->CommitPendingWrite();
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}  // namespace

// static
MetricsService::ShutdownCleanliness MetricsService::clean_shutdown_status_ =
    MetricsService::CLEANLY_SHUTDOWN;

// static
void MetricsService::RegisterPrefs(PrefRegistrySimple* registry) {
  CleanExitBeacon::RegisterPrefs(registry);
  MetricsStateManager::RegisterPrefs(registry);
  MetricsLog::RegisterPrefs(registry);
  StabilityMetricsProvider::RegisterPrefs(registry);
  DataUseTracker::RegisterPrefs(registry);
  ExecutionPhaseManager::RegisterPrefs(registry);

  registry->RegisterInt64Pref(prefs::kInstallDate, 0);

  registry->RegisterIntegerPref(prefs::kMetricsSessionID, -1);

  registry->RegisterListPref(prefs::kMetricsInitialLogs);
  registry->RegisterListPref(prefs::kMetricsOngoingLogs);

  registry->RegisterInt64Pref(prefs::kUninstallLaunchCount, 0);
  registry->RegisterInt64Pref(prefs::kUninstallMetricsUptimeSec, 0);
}

MetricsService::MetricsService(MetricsStateManager* state_manager,
                               MetricsServiceClient* client,
                               PrefService* local_state)
    : log_store_(local_state, kUploadLogAvoidRetransmitSize),
      histogram_snapshot_manager_(this),
      state_manager_(state_manager),
      client_(client),
      local_state_(local_state),
      clean_exit_beacon_(client->GetRegistryBackupKey(), local_state),
      recording_state_(UNSET),
      reporting_active_(false),
      test_mode_active_(false),
      state_(INITIALIZED),
      log_upload_in_progress_(false),
      idle_since_last_transmission_(false),
      session_id_(-1),
      data_use_tracker_(DataUseTracker::Create(local_state_)),
      self_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_manager_);
  DCHECK(client_);
  DCHECK(local_state_);

  // Set the install date if this is our first run.
  int64_t install_date = local_state_->GetInt64(prefs::kInstallDate);
  if (install_date == 0)
    local_state_->SetInt64(prefs::kInstallDate, base::Time::Now().ToTimeT());

  RegisterMetricsProvider(std::unique_ptr<metrics::MetricsProvider>(
      new StabilityMetricsProvider(local_state_)));
}

MetricsService::~MetricsService() {
  DisableRecording();
}

void MetricsService::InitializeMetricsRecordingState() {
  InitializeMetricsState();

  base::Closure upload_callback =
      base::Bind(&MetricsService::StartScheduledUpload,
                 self_ptr_factory_.GetWeakPtr());

  rotation_scheduler_.reset(new MetricsRotationScheduler(
      upload_callback,
      // MetricsServiceClient outlives MetricsService, and
      // MetricsRotationScheduler is tied to the lifetime of |this|.
      base::Bind(&MetricsServiceClient::GetStandardUploadInterval,
                 base::Unretained(client_))));
  base::Closure send_next_log_callback =
      base::Bind(&MetricsService::SendNextLog, self_ptr_factory_.GetWeakPtr());
  upload_scheduler_.reset(new MetricsUploadScheduler(send_next_log_callback));

  for (auto& provider : metrics_providers_)
    provider->Init();
}

void MetricsService::Start() {
  HandleIdleSinceLastTransmission(false);
  EnableRecording();
  EnableReporting();
}

void MetricsService::StartRecordingForTests() {
  test_mode_active_ = true;
  EnableRecording();
  DisableReporting();
}

void MetricsService::Stop() {
  HandleIdleSinceLastTransmission(false);
  DisableReporting();
  DisableRecording();
}

void MetricsService::EnableReporting() {
  if (reporting_active_)
    return;
  reporting_active_ = true;
  StartSchedulerIfNecessary();
}

void MetricsService::DisableReporting() {
  reporting_active_ = false;
}

std::string MetricsService::GetClientId() {
  return state_manager_->client_id();
}

int64_t MetricsService::GetInstallDate() {
  return local_state_->GetInt64(prefs::kInstallDate);
}

int64_t MetricsService::GetMetricsReportingEnabledDate() {
  return local_state_->GetInt64(prefs::kMetricsReportingEnabledTimestamp);
}

bool MetricsService::WasLastShutdownClean() const {
  return clean_exit_beacon_.exited_cleanly();
}

void MetricsService::EnableRecording() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (recording_state_ == ACTIVE)
    return;
  recording_state_ = ACTIVE;

  state_manager_->ForceClientIdCreation();
  client_->SetMetricsClientId(state_manager_->client_id());
  if (!log_manager_.current_log())
    OpenNewLog();

  for (auto& provider : metrics_providers_)
    provider->OnRecordingEnabled();

  base::RemoveActionCallback(action_callback_);
  action_callback_ = base::Bind(&MetricsService::OnUserAction,
                                base::Unretained(this));
  base::AddActionCallback(action_callback_);
}

void MetricsService::DisableRecording() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (recording_state_ == INACTIVE)
    return;
  recording_state_ = INACTIVE;

  base::RemoveActionCallback(action_callback_);

  for (auto& provider : metrics_providers_)
    provider->OnRecordingDisabled();

  PushPendingLogsToPersistentStorage();
}

bool MetricsService::recording_active() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return recording_state_ == ACTIVE;
}

bool MetricsService::reporting_active() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return reporting_active_;
}

void MetricsService::RecordDelta(const base::HistogramBase& histogram,
                                 const base::HistogramSamples& snapshot) {
  log_manager_.current_log()->RecordHistogramDelta(histogram.histogram_name(),
                                                   snapshot);
}

void MetricsService::InconsistencyDetected(
    base::HistogramBase::Inconsistency problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowser",
                            problem, base::HistogramBase::NEVER_EXCEEDED_VALUE);
}

void MetricsService::UniqueInconsistencyDetected(
    base::HistogramBase::Inconsistency problem) {
  UMA_HISTOGRAM_ENUMERATION("Histogram.InconsistenciesBrowserUnique",
                            problem, base::HistogramBase::NEVER_EXCEEDED_VALUE);
}

void MetricsService::InconsistencyDetectedInLoggedCount(int amount) {
  UMA_HISTOGRAM_COUNTS("Histogram.InconsistentSnapshotBrowser",
                       std::abs(amount));
}

void MetricsService::HandleIdleSinceLastTransmission(bool in_idle) {
  // If there wasn't a lot of action, maybe the computer was asleep, in which
  // case, the log transmissions should have stopped.  Here we start them up
  // again.
  if (!in_idle && idle_since_last_transmission_)
    StartSchedulerIfNecessary();
  idle_since_last_transmission_ = in_idle;
}

void MetricsService::OnApplicationNotIdle() {
  if (recording_state_ == ACTIVE)
    HandleIdleSinceLastTransmission(false);
}

void MetricsService::RecordStartOfSessionEnd() {
  LogCleanShutdown(false);
}

void MetricsService::RecordCompletedSessionEnd() {
  LogCleanShutdown(true);
}

#if defined(OS_ANDROID) || defined(OS_IOS)
void MetricsService::OnAppEnterBackground() {
  rotation_scheduler_->Stop();
  upload_scheduler_->Stop();

  MarkAppCleanShutdownAndCommit(&clean_exit_beacon_, local_state_);

  // Give providers a chance to persist histograms as part of being
  // backgrounded.
  for (auto& provider : metrics_providers_)
    provider->OnAppEnterBackground();

  // At this point, there's no way of knowing when the process will be
  // killed, so this has to be treated similar to a shutdown, closing and
  // persisting all logs. Unlinke a shutdown, the state is primed to be ready
  // to continue logging and uploading if the process does return.
  if (recording_active() && state_ >= SENDING_LOGS) {
    PushPendingLogsToPersistentStorage();
    // Persisting logs closes the current log, so start recording a new log
    // immediately to capture any background work that might be done before the
    // process is killed.
    OpenNewLog();
  }
}

void MetricsService::OnAppEnterForeground() {
  clean_exit_beacon_.WriteBeaconValue(false);
  ExecutionPhaseManager(local_state_).OnAppEnterForeground();
  StartSchedulerIfNecessary();
}
#else
void MetricsService::LogNeedForCleanShutdown() {
  clean_exit_beacon_.WriteBeaconValue(false);
  // Redundant setting to be sure we call for a clean shutdown.
  clean_shutdown_status_ = NEED_TO_SHUTDOWN;
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

// static
void MetricsService::SetExecutionPhase(ExecutionPhase execution_phase,
                                       PrefService* local_state) {
  ExecutionPhaseManager(local_state).SetExecutionPhase(execution_phase);
}

void MetricsService::RecordBreakpadRegistration(bool success) {
  StabilityMetricsProvider(local_state_).RecordBreakpadRegistration(success);
}

void MetricsService::RecordBreakpadHasDebugger(bool has_debugger) {
  StabilityMetricsProvider(local_state_)
      .RecordBreakpadHasDebugger(has_debugger);
}

void MetricsService::ClearSavedStabilityMetrics() {
  for (auto& provider : metrics_providers_)
    provider->ClearSavedStabilityMetrics();
}

void MetricsService::PushExternalLog(const std::string& log) {
  log_store_.StoreLog(log, MetricsLog::ONGOING_LOG);
}

void MetricsService::UpdateMetricsUsagePrefs(const std::string& service_name,
                                             int message_size,
                                             bool is_cellular) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_use_tracker_) {
    data_use_tracker_->UpdateMetricsUsagePrefs(service_name,
                                               message_size,
                                               is_cellular);
  }
}

//------------------------------------------------------------------------------
// private methods
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Initialization methods

void MetricsService::InitializeMetricsState() {
  const int64_t buildtime = MetricsLog::GetBuildTime();
  const std::string version = client_->GetVersionString();

  bool version_changed = false;
  EnvironmentRecorder recorder(local_state_);
  int64_t previous_buildtime = recorder.GetLastBuildtime();
  std::string previous_version = recorder.GetLastVersion();
  if (previous_buildtime != buildtime || previous_version != version) {
    recorder.SetBuildtimeAndVersion(buildtime, version);
    version_changed = true;
  }

  log_store_.LoadPersistedUnsentLogs();

  session_id_ = local_state_->GetInteger(prefs::kMetricsSessionID);

  StabilityMetricsProvider provider(local_state_);
  if (!clean_exit_beacon_.exited_cleanly()) {
    provider.LogCrash();
    // Reset flag, and wait until we call LogNeedForCleanShutdown() before
    // monitoring.
    clean_exit_beacon_.WriteBeaconValue(true);
    ExecutionPhaseManager manager(local_state_);
    UMA_HISTOGRAM_SPARSE_SLOWLY("Chrome.Browser.CrashedExecutionPhase",
                                static_cast<int>(manager.GetExecutionPhase()));
    manager.SetExecutionPhase(ExecutionPhase::UNINITIALIZED_PHASE);
  }

  // ProvidersHaveInitialStabilityMetrics is called first to ensure it is never
  // bypassed.
  const bool is_initial_stability_log_required =
      ProvidersHaveInitialStabilityMetrics() ||
      !clean_exit_beacon_.exited_cleanly();
  bool has_initial_stability_log = false;
  if (is_initial_stability_log_required) {
    // If the previous session didn't exit cleanly, or if any provider
    // explicitly requests it, prepare an initial stability log -
    // provided UMA is enabled.
    if (state_manager_->IsMetricsReportingEnabled()) {
      has_initial_stability_log = PrepareInitialStabilityLog(previous_version);
      if (!has_initial_stability_log)
        provider.LogStabilityLogDeferred();
    }
  }

  // If the version changed, but no initial stability log was generated, clear
  // the stability stats from the previous version (so that they don't get
  // attributed to the current version). This could otherwise happen due to a
  // number of different edge cases, such as if the last version crashed before
  // it could save off a system profile or if UMA reporting is disabled (which
  // normally results in stats being accumulated).
  if (version_changed && !has_initial_stability_log) {
    ClearSavedStabilityMetrics();
    provider.LogStabilityDataDiscarded();
  }

  // If the version changed, the system profile is obsolete and needs to be
  // cleared. This is to avoid the stability data misattribution that could
  // occur if the current version crashed before saving its own system profile.
  // Note however this clearing occurs only after preparing the initial
  // stability log, an operation that requires the previous version's system
  // profile. At this point, stability metrics pertaining to the previous
  // version have been cleared.
  if (version_changed)
    recorder.ClearEnvironmentFromPrefs();

  // Update session ID.
  ++session_id_;
  local_state_->SetInteger(prefs::kMetricsSessionID, session_id_);

  // Notify stability metrics providers about the launch.
  provider.LogLaunch();
  SetExecutionPhase(ExecutionPhase::START_METRICS_RECORDING, local_state_);
  provider.CheckLastSessionEndCompleted();

  // Call GetUptimes() for the first time, thus allowing all later calls
  // to record incremental uptimes accurately.
  base::TimeDelta ignored_uptime_parameter;
  base::TimeDelta startup_uptime;
  GetUptimes(local_state_, &startup_uptime, &ignored_uptime_parameter);
  DCHECK_EQ(0, startup_uptime.InMicroseconds());

  // Bookkeeping for the uninstall metrics.
  IncrementLongPrefsValue(prefs::kUninstallLaunchCount);
}

void MetricsService::OnUserAction(const std::string& action) {
  log_manager_.current_log()->RecordUserAction(action);
  HandleIdleSinceLastTransmission(false);
}

void MetricsService::FinishedInitTask() {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);
  state_ = INIT_TASK_DONE;

  // Create the initial log.
  if (!initial_metrics_log_.get()) {
    initial_metrics_log_ = CreateLog(MetricsLog::ONGOING_LOG);
    NotifyOnDidCreateMetricsLog();
  }

  rotation_scheduler_->InitTaskComplete();
}

void MetricsService::GetUptimes(PrefService* pref,
                                base::TimeDelta* incremental_uptime,
                                base::TimeDelta* uptime) {
  base::TimeTicks now = base::TimeTicks::Now();
  // If this is the first call, init |first_updated_time_| and
  // |last_updated_time_|.
  if (last_updated_time_.is_null()) {
    first_updated_time_ = now;
    last_updated_time_ = now;
  }
  *incremental_uptime = now - last_updated_time_;
  *uptime = now - first_updated_time_;
  last_updated_time_ = now;

  const int64_t incremental_time_secs = incremental_uptime->InSeconds();
  if (incremental_time_secs > 0) {
    int64_t metrics_uptime = pref->GetInt64(prefs::kUninstallMetricsUptimeSec);
    metrics_uptime += incremental_time_secs;
    pref->SetInt64(prefs::kUninstallMetricsUptimeSec, metrics_uptime);
  }
}

void MetricsService::NotifyOnDidCreateMetricsLog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& provider : metrics_providers_)
    provider->OnDidCreateMetricsLog();
}


//------------------------------------------------------------------------------
// Recording control methods

void MetricsService::OpenNewLog() {
  DCHECK(!log_manager_.current_log());

  log_manager_.BeginLoggingWithLog(CreateLog(MetricsLog::ONGOING_LOG));
  NotifyOnDidCreateMetricsLog();
  if (state_ == INITIALIZED) {
    // We only need to schedule that run once.
    state_ = INIT_TASK_SCHEDULED;

    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&MetricsService::StartInitTask,
                              self_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(kInitializationDelaySeconds));
  }
}

void MetricsService::StartInitTask() {
  client_->InitializeSystemProfileMetrics(
      base::Bind(&MetricsService::FinishedInitTask,
                 self_ptr_factory_.GetWeakPtr()));
}

void MetricsService::CloseCurrentLog() {
  if (!log_manager_.current_log())
    return;

  // TODO(rkaplow): Evaluate if this is needed.
  if (log_manager_.current_log()->num_events() > kEventLimit) {
    UMA_HISTOGRAM_COUNTS("UMA.Discarded Log Events",
                         log_manager_.current_log()->num_events());
    if (base::FeatureList::IsEnabled(kUMAThrottleEvents)) {
      log_manager_.DiscardCurrentLog();
      OpenNewLog();  // Start trivial log to hold our histograms.
    }
  }

  // If a persistent allocator is in use, update its internal histograms (such
  // as how much memory is being used) before reporting.
  base::PersistentHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (allocator)
    allocator->UpdateTrackingHistograms();

  // Put incremental data (histogram deltas, and realtime stats deltas) at the
  // end of all log transmissions (initial log handles this separately).
  // RecordIncrementalStabilityElements only exists on the derived
  // MetricsLog class.
  MetricsLog* current_log = log_manager_.current_log();
  DCHECK(current_log);
  RecordCurrentEnvironment(current_log);
  base::TimeDelta incremental_uptime;
  base::TimeDelta uptime;
  GetUptimes(local_state_, &incremental_uptime, &uptime);
  current_log->RecordStabilityMetrics(metrics_providers_, incremental_uptime,
                                      uptime);

  current_log->RecordGeneralMetrics(metrics_providers_);
  RecordCurrentHistograms();
  DVLOG(1) << "Generated an ongoing log.";
  log_manager_.FinishCurrentLog(&log_store_);
}

void MetricsService::PushPendingLogsToPersistentStorage() {
  if (state_ < SENDING_LOGS)
    return;  // We didn't and still don't have time to get plugin list etc.

  CloseCurrentLog();
  log_store_.PersistUnsentLogs();
}

//------------------------------------------------------------------------------
// Transmission of logs methods

void MetricsService::StartSchedulerIfNecessary() {
  // Never schedule cutting or uploading of logs in test mode.
  if (test_mode_active_)
    return;

  // Even if reporting is disabled, the scheduler is needed to trigger the
  // creation of the initial log, which must be done in order for any logs to be
  // persisted on shutdown or backgrounding.
  if (recording_active() &&
      (reporting_active() || state_ < SENDING_LOGS)) {
    rotation_scheduler_->Start();
    upload_scheduler_->Start();
  }
}

void MetricsService::StartScheduledUpload() {
  DVLOG(1) << "StartScheduledUpload";
  DCHECK(state_ >= INIT_TASK_DONE);
  // If we're getting no notifications, then the log won't have much in it, and
  // it's possible the computer is about to go to sleep, so don't upload and
  // stop the scheduler.
  // If recording has been turned off, the scheduler doesn't need to run.
  // If reporting is off, proceed if the initial log hasn't been created, since
  // that has to happen in order for logs to be cut and stored when persisting.
  // TODO(stuartmorgan): Call Stop() on the scheduler when reporting and/or
  // recording are turned off instead of letting it fire and then aborting.
  if (idle_since_last_transmission_ ||
      !recording_active() ||
      (!reporting_active() && state_ >= SENDING_LOGS)) {
    rotation_scheduler_->Stop();
    rotation_scheduler_->RotationFinished();
    return;
  }

  // If there are unsent logs, send the next one. If not, start the asynchronous
  // process of finalizing the current log for upload.
  if (state_ == SENDING_LOGS && log_store_.has_unsent_logs()) {
    upload_scheduler_->Start();
    rotation_scheduler_->RotationFinished();
  } else {
    // There are no logs left to send, so start creating a new one.
    client_->CollectFinalMetricsForLog(
        base::Bind(&MetricsService::OnFinalLogInfoCollectionDone,
                   self_ptr_factory_.GetWeakPtr()));
  }
}

void MetricsService::OnFinalLogInfoCollectionDone() {
  DVLOG(1) << "OnFinalLogInfoCollectionDone";
  // Abort if metrics were turned off during the final info gathering.
  if (!recording_active()) {
    rotation_scheduler_->Stop();
    rotation_scheduler_->RotationFinished();
    return;
  }

  if (state_ == INIT_TASK_DONE) {
    PrepareInitialMetricsLog();
  } else {
    DCHECK_EQ(SENDING_LOGS, state_);
    CloseCurrentLog();
    OpenNewLog();
  }
  HandleIdleSinceLastTransmission(true);
  upload_scheduler_->Start();
  rotation_scheduler_->RotationFinished();
}

void MetricsService::SendNextLog() {
  DVLOG(1) << "SendNextLog";
  if (!reporting_active()) {
    upload_scheduler_->StopAndUploadCancelled();
    return;
  }
  if (!log_store_.has_unsent_logs()) {
    // Should only get here if serializing the log failed somehow.
    upload_scheduler_->Stop();
    // Reset backoff interval
    upload_scheduler_->UploadFinished(true);
    return;
  }
  if (!log_store_.has_staged_log())
    log_store_.StageNextLog();

  // Proceed to stage the log for upload if log size satisfies cellular log
  // upload constrains.
  bool upload_canceled = false;
  bool is_cellular_logic = client_->IsUMACellularUploadLogicEnabled();
  if (is_cellular_logic && data_use_tracker_ &&
      !data_use_tracker_->ShouldUploadLogOnCellular(
          log_store_.staged_log_hash().size())) {
    upload_scheduler_->UploadOverDataUsageCap();
    upload_canceled = true;
  } else {
    SendStagedLog();
  }
  if (is_cellular_logic) {
    UMA_HISTOGRAM_BOOLEAN("UMA.LogUpload.Canceled.CellularConstraint",
                          upload_canceled);
  }
}

bool MetricsService::ProvidersHaveInitialStabilityMetrics() {
  // Check whether any metrics provider has initial stability metrics.
  // All providers are queried (rather than stopping after the first "true"
  // response) in case they do any kind of setup work in preparation for
  // the later call to RecordInitialHistogramSnapshots().
  bool has_stability_metrics = false;
  for (auto& provider : metrics_providers_)
    has_stability_metrics |= provider->HasInitialStabilityMetrics();

  return has_stability_metrics;
}

bool MetricsService::PrepareInitialStabilityLog(
    const std::string& prefs_previous_version) {
  DCHECK_EQ(INITIALIZED, state_);

  std::unique_ptr<MetricsLog> initial_stability_log(
      CreateLog(MetricsLog::INITIAL_STABILITY_LOG));

  // Do not call NotifyOnDidCreateMetricsLog here because the stability
  // log describes stats from the _previous_ session.
  std::string system_profile_app_version;
  if (!initial_stability_log->LoadSavedEnvironmentFromPrefs(
          &system_profile_app_version)) {
    return false;
  }
  if (system_profile_app_version != prefs_previous_version)
    StabilityMetricsProvider(local_state_).LogStabilityVersionMismatch();

  log_manager_.PauseCurrentLog();
  log_manager_.BeginLoggingWithLog(std::move(initial_stability_log));

  // Note: Some stability providers may record stability stats via histograms,
  //       so this call has to be after BeginLoggingWithLog().
  log_manager_.current_log()->RecordStabilityMetrics(
      metrics_providers_, base::TimeDelta(), base::TimeDelta());
  RecordCurrentStabilityHistograms();

  // Note: RecordGeneralMetrics() intentionally not called since this log is for
  //       stability stats from a previous session only.

  DVLOG(1) << "Generated an stability log.";
  log_manager_.FinishCurrentLog(&log_store_);
  log_manager_.ResumePausedLog();

  // Store unsent logs, including the stability log that was just saved, so
  // that they're not lost in case of a crash before upload time.
  log_store_.PersistUnsentLogs();

  return true;
}

void MetricsService::PrepareInitialMetricsLog() {
  DCHECK_EQ(INIT_TASK_DONE, state_);

  RecordCurrentEnvironment(initial_metrics_log_.get());
  base::TimeDelta incremental_uptime;
  base::TimeDelta uptime;
  GetUptimes(local_state_, &incremental_uptime, &uptime);

  // Histograms only get written to the current log, so make the new log current
  // before writing them.
  log_manager_.PauseCurrentLog();
  log_manager_.BeginLoggingWithLog(std::move(initial_metrics_log_));

  // Note: Some stability providers may record stability stats via histograms,
  //       so this call has to be after BeginLoggingWithLog().
  MetricsLog* current_log = log_manager_.current_log();
  current_log->RecordStabilityMetrics(metrics_providers_, base::TimeDelta(),
                                      base::TimeDelta());
  current_log->RecordGeneralMetrics(metrics_providers_);
  RecordCurrentHistograms();

  DVLOG(1) << "Generated an initial log.";
  log_manager_.FinishCurrentLog(&log_store_);
  log_manager_.ResumePausedLog();

  // Store unsent logs, including the initial log that was just saved, so
  // that they're not lost in case of a crash before upload time.
  log_store_.PersistUnsentLogs();

  state_ = SENDING_LOGS;
}

void MetricsService::SendStagedLog() {
  DCHECK(log_store_.has_staged_log());
  if (!log_store_.has_staged_log())
    return;

  DCHECK(!log_upload_in_progress_);
  log_upload_in_progress_ = true;

  if (!log_uploader_) {
    log_uploader_ = client_->CreateUploader(
        client_->GetMetricsServerUrl(), metrics::kDefaultMetricsMimeType,
        metrics::MetricsLogUploader::UMA,
        base::Bind(&MetricsService::OnLogUploadComplete,
                   self_ptr_factory_.GetWeakPtr()));
  }
  const std::string hash = base::HexEncode(log_store_.staged_log_hash().data(),
                                           log_store_.staged_log_hash().size());
  log_uploader_->UploadLog(log_store_.staged_log(), hash);
}


void MetricsService::OnLogUploadComplete(int response_code) {
  DVLOG(1) << "OnLogUploadComplete:" << response_code;
  DCHECK(log_upload_in_progress_);
  log_upload_in_progress_ = false;

  // Log a histogram to track response success vs. failure rates.
  UMA_HISTOGRAM_ENUMERATION("UMA.UploadResponseStatus.Protobuf",
                            ResponseCodeToStatus(response_code),
                            NUM_RESPONSE_STATUSES);

  bool upload_succeeded = response_code == 200;

  // Provide boolean for error recovery (allow us to ignore response_code).
  bool discard_log = false;
  const size_t log_size = log_store_.staged_log().length();
  if (upload_succeeded) {
    UMA_HISTOGRAM_COUNTS_10000("UMA.LogSize.OnSuccess", log_size / 1024);
  } else if (log_size > kUploadLogAvoidRetransmitSize) {
    UMA_HISTOGRAM_COUNTS("UMA.Large Rejected Log was Discarded",
                         static_cast<int>(log_size));
    discard_log = true;
  } else if (response_code == 400) {
    // Bad syntax.  Retransmission won't work.
    discard_log = true;
  }

  if (upload_succeeded || discard_log) {
    log_store_.DiscardStagedLog();
    // Store the updated list to disk now that the removed log is uploaded.
    log_store_.PersistUnsentLogs();
  }

  // Error 400 indicates a problem with the log, not with the server, so
  // don't consider that a sign that the server is in trouble.
  bool server_is_healthy = upload_succeeded || response_code == 400;
  if (!log_store_.has_unsent_logs()) {
    DVLOG(1) << "Stopping upload_scheduler_";
    upload_scheduler_->Stop();
  }
  upload_scheduler_->UploadFinished(server_is_healthy);
}

void MetricsService::IncrementLongPrefsValue(const char* path) {
  int64_t value = local_state_->GetInt64(path);
  local_state_->SetInt64(path, value + 1);
}

bool MetricsService::UmaMetricsProperlyShutdown() {
  CHECK(clean_shutdown_status_ == CLEANLY_SHUTDOWN ||
        clean_shutdown_status_ == NEED_TO_SHUTDOWN);
  return clean_shutdown_status_ == CLEANLY_SHUTDOWN;
}

void MetricsService::AddSyntheticTrialObserver(
    variations::SyntheticTrialObserver* observer) {
  synthetic_trial_observer_list_.AddObserver(observer);
  if (!synthetic_trial_groups_.empty())
    observer->OnSyntheticTrialsChanged(synthetic_trial_groups_);
}

void MetricsService::RemoveSyntheticTrialObserver(
    variations::SyntheticTrialObserver* observer) {
  synthetic_trial_observer_list_.RemoveObserver(observer);
}

void MetricsService::RegisterSyntheticFieldTrial(
    const variations::SyntheticTrialGroup& trial) {
  for (size_t i = 0; i < synthetic_trial_groups_.size(); ++i) {
    if (synthetic_trial_groups_[i].id.name == trial.id.name) {
      if (synthetic_trial_groups_[i].id.group != trial.id.group) {
        synthetic_trial_groups_[i].id.group = trial.id.group;
        synthetic_trial_groups_[i].start_time = base::TimeTicks::Now();
        NotifySyntheticTrialObservers();
      }
      return;
    }
  }

  variations::SyntheticTrialGroup trial_group = trial;
  trial_group.start_time = base::TimeTicks::Now();
  synthetic_trial_groups_.push_back(trial_group);
  NotifySyntheticTrialObservers();
}

void MetricsService::RegisterSyntheticMultiGroupFieldTrial(
    uint32_t trial_name_hash,
    const std::vector<uint32_t>& group_name_hashes) {
  auto has_same_trial_name =
      [trial_name_hash](const variations::SyntheticTrialGroup& x) {
        return x.id.name == trial_name_hash;
      };
  synthetic_trial_groups_.erase(
      std::remove_if(synthetic_trial_groups_.begin(),
                     synthetic_trial_groups_.end(), has_same_trial_name),
      synthetic_trial_groups_.end());

  if (group_name_hashes.empty())
    return;

  variations::SyntheticTrialGroup trial_group(trial_name_hash,
                                              group_name_hashes[0]);
  trial_group.start_time = base::TimeTicks::Now();
  for (uint32_t group_name_hash : group_name_hashes) {
    // Note: Adding the trial group will copy it, so this re-uses the same
    // |trial_group| struct for convenience (e.g. so start_time's all match).
    trial_group.id.group = group_name_hash;
    synthetic_trial_groups_.push_back(trial_group);
  }
  NotifySyntheticTrialObservers();
}

void MetricsService::GetCurrentSyntheticFieldTrialsForTesting(
    std::vector<variations::ActiveGroupId>* synthetic_trials) {
  GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(), synthetic_trials);
}

void MetricsService::RegisterMetricsProvider(
    std::unique_ptr<MetricsProvider> provider) {
  DCHECK_EQ(INITIALIZED, state_);
  metrics_providers_.push_back(std::move(provider));
}

void MetricsService::CheckForClonedInstall(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  state_manager_->CheckForClonedInstall(task_runner);
}

void MetricsService::NotifySyntheticTrialObservers() {
  for (variations::SyntheticTrialObserver& observer :
       synthetic_trial_observer_list_) {
    observer.OnSyntheticTrialsChanged(synthetic_trial_groups_);
  }
}

void MetricsService::GetSyntheticFieldTrialsOlderThan(
    base::TimeTicks time,
    std::vector<variations::ActiveGroupId>* synthetic_trials) {
  DCHECK(synthetic_trials);
  synthetic_trials->clear();
  for (size_t i = 0; i < synthetic_trial_groups_.size(); ++i) {
    if (synthetic_trial_groups_[i].start_time <= time)
      synthetic_trials->push_back(synthetic_trial_groups_[i].id);
  }
}

std::unique_ptr<MetricsLog> MetricsService::CreateLog(
    MetricsLog::LogType log_type) {
  return base::MakeUnique<MetricsLog>(state_manager_->client_id(), session_id_,
                                      log_type, client_, local_state_);
}

void MetricsService::RecordCurrentEnvironment(MetricsLog* log) {
  DCHECK(client_);
  std::vector<variations::ActiveGroupId> synthetic_trials;
  GetSyntheticFieldTrialsOlderThan(log->creation_time(), &synthetic_trials);
  std::string serialized_environment = log->RecordEnvironment(
      metrics_providers_, synthetic_trials, GetInstallDate(),
      GetMetricsReportingEnabledDate());
  client_->OnEnvironmentUpdate(&serialized_environment);
}

void MetricsService::RecordCurrentHistograms() {
  DCHECK(log_manager_.current_log());
  SCOPED_UMA_HISTOGRAM_TIMER("UMA.MetricsService.RecordCurrentHistograms.Time");

  // "true" to the begin() call indicates that StatisticsRecorder should include
  // histograms held in persistent storage.
  histogram_snapshot_manager_.PrepareDeltas(
      base::StatisticsRecorder::begin(true), base::StatisticsRecorder::end(),
      base::Histogram::kNoFlags, base::Histogram::kUmaTargetedHistogramFlag);
  for (auto& provider : metrics_providers_)
    provider->RecordHistogramSnapshots(&histogram_snapshot_manager_);
}

void MetricsService::RecordCurrentStabilityHistograms() {
  DCHECK(log_manager_.current_log());
  // "true" indicates that StatisticsRecorder should include histograms in
  // persistent storage.
  histogram_snapshot_manager_.PrepareDeltas(
      base::StatisticsRecorder::begin(true), base::StatisticsRecorder::end(),
      base::Histogram::kNoFlags, base::Histogram::kUmaStabilityHistogramFlag);
  for (auto& provider : metrics_providers_)
    provider->RecordInitialHistogramSnapshots(&histogram_snapshot_manager_);
}

void MetricsService::LogCleanShutdown(bool end_completed) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Redundant setting to assure that we always reset this value at shutdown
  // (and that we don't use some alternate path, and not call LogCleanShutdown).
  clean_shutdown_status_ = CLEANLY_SHUTDOWN;
  client_->OnLogCleanShutdown();
  clean_exit_beacon_.WriteBeaconValue(true);
  SetExecutionPhase(ExecutionPhase::SHUTDOWN_COMPLETE, local_state_);
  StabilityMetricsProvider(local_state_).MarkSessionEndCompleted(end_completed);
}

}  // namespace metrics
