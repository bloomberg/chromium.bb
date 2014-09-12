// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that collects information about the user
// experience in order to help improve future versions of the app.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_manager.h"
#include "components/metrics/metrics_provider.h"
#include "components/variations/active_field_trials.h"

class MetricsServiceAccessor;
class PrefService;
class PrefRegistrySimple;

namespace base {
class DictionaryValue;
class HistogramSamples;
class MessageLoopProxy;
class PrefService;
}

namespace variations {
struct ActiveGroupId;
}

namespace net {
class URLFetcher;
}

namespace metrics {

class MetricsLogUploader;
class MetricsReportingScheduler;
class MetricsServiceClient;
class MetricsStateManager;

// A Field Trial and its selected group, which represent a particular
// Chrome configuration state. For example, the trial name could map to
// a preference name, and the group name could map to a preference value.
struct SyntheticTrialGroup {
 public:
  ~SyntheticTrialGroup();

  variations::ActiveGroupId id;
  base::TimeTicks start_time;

 private:
  // Synthetic field trial users:
  friend class ::MetricsServiceAccessor;
  friend class MetricsService;
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, RegisterSyntheticTrial);

  // This constructor is private specifically so as to control which code is
  // able to access it. New code that wishes to use it should be added as a
  // friend class.
  SyntheticTrialGroup(uint32 trial, uint32 group);
};

// See metrics_service.cc for a detailed description.
class MetricsService : public base::HistogramFlattener {
 public:
  // The execution phase of the browser.
  enum ExecutionPhase {
    UNINITIALIZED_PHASE = 0,
    START_METRICS_RECORDING = 100,
    CREATE_PROFILE = 200,
    STARTUP_TIMEBOMB_ARM = 300,
    THREAD_WATCHER_START = 400,
    MAIN_MESSAGE_LOOP_RUN = 500,
    SHUTDOWN_TIMEBOMB_ARM = 600,
    SHUTDOWN_COMPLETE = 700,
  };

  // Creates the MetricsService with the given |state_manager|, |client|, and
  // |local_state|.  Does not take ownership of the paramaters; instead stores
  // a weak pointer to each. Caller should ensure that the parameters are valid
  // for the lifetime of this class.
  MetricsService(MetricsStateManager* state_manager,
                 MetricsServiceClient* client,
                 PrefService* local_state);
  virtual ~MetricsService();

  // Initializes metrics recording state. Updates various bookkeeping values in
  // prefs and sets up the scheduler. This is a separate function rather than
  // being done by the constructor so that field trials could be created before
  // this is run.
  void InitializeMetricsRecordingState();

  // Starts the metrics system, turning on recording and uploading of metrics.
  // Should be called when starting up with metrics enabled, or when metrics
  // are turned on.
  void Start();

  // If metrics reporting is enabled, starts the metrics service. Returns
  // whether the metrics service was started.
  bool StartIfMetricsReportingEnabled();

  // Starts the metrics system in a special test-only mode. Metrics won't ever
  // be uploaded or persisted in this mode, but metrics will be recorded in
  // memory.
  void StartRecordingForTests();

  // Shuts down the metrics system. Should be called at shutdown, or if metrics
  // are turned off.
  void Stop();

  // Enable/disable transmission of accumulated logs and crash reports (dumps).
  // Calling Start() automatically enables reporting, but sending is
  // asyncronous so this can be called immediately after Start() to prevent
  // any uploading.
  void EnableReporting();
  void DisableReporting();

  // Returns the client ID for this client, or the empty string if metrics
  // recording is not currently running.
  std::string GetClientId();

  // Returns the install date of the application, in seconds since the epoch.
  int64 GetInstallDate();

  // Returns the preferred entropy provider used to seed persistent activities
  // based on whether or not metrics reporting will be permitted on this client.
  //
  // If metrics reporting is enabled, this method returns an entropy provider
  // that has a high source of entropy, partially based on the client ID.
  // Otherwise, it returns an entropy provider that is based on a low entropy
  // source.
  scoped_ptr<const base::FieldTrial::EntropyProvider> CreateEntropyProvider();

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // HistogramFlattener:
  virtual void RecordDelta(const base::HistogramBase& histogram,
                           const base::HistogramSamples& snapshot) OVERRIDE;
  virtual void InconsistencyDetected(
      base::HistogramBase::Inconsistency problem) OVERRIDE;
  virtual void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) OVERRIDE;
  virtual void InconsistencyDetectedInLoggedCount(int amount) OVERRIDE;

  // This should be called when the application is not idle, i.e. the user seems
  // to be interacting with the application.
  void OnApplicationNotIdle();

  // Invoked when we get a WM_SESSIONEND. This places a value in prefs that is
  // reset when RecordCompletedSessionEnd is invoked.
  void RecordStartOfSessionEnd();

  // This should be called when the application is shutting down. It records
  // that session end was successful.
  void RecordCompletedSessionEnd();

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Called when the application is going into background mode.
  void OnAppEnterBackground();

  // Called when the application is coming out of background mode.
  void OnAppEnterForeground();
#else
  // Set the dirty flag, which will require a later call to LogCleanShutdown().
  void LogNeedForCleanShutdown();
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  static void SetExecutionPhase(ExecutionPhase execution_phase,
                                PrefService* local_state);

  // Saves in the preferences if the crash report registration was successful.
  // This count is eventually send via UMA logs.
  void RecordBreakpadRegistration(bool success);

  // Saves in the preferences if the browser is running under a debugger.
  // This count is eventually send via UMA logs.
  void RecordBreakpadHasDebugger(bool has_debugger);

  bool recording_active() const;
  bool reporting_active() const;

  // Redundant test to ensure that we are notified of a clean exit.
  // This value should be true when process has completed shutdown.
  static bool UmaMetricsProperlyShutdown();

  // Registers a field trial name and group to be used to annotate a UMA report
  // with a particular Chrome configuration state. A UMA report will be
  // annotated with this trial group if and only if all events in the report
  // were created after the trial is registered. Only one group name may be
  // registered at a time for a given trial_name. Only the last group name that
  // is registered for a given trial name will be recorded. The values passed
  // in must not correspond to any real field trial in the code.
  // To use this method, SyntheticTrialGroup should friend your class.
  void RegisterSyntheticFieldTrial(const SyntheticTrialGroup& trial_group);

  // Register the specified |provider| to provide additional metrics into the
  // UMA log. Should be called during MetricsService initialization only.
  void RegisterMetricsProvider(scoped_ptr<MetricsProvider> provider);

  // Check if this install was cloned or imaged from another machine. If a
  // clone is detected, reset the client id and low entropy source. This
  // should not be called more than once.
  void CheckForClonedInstall(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 protected:
  // Exposed for testing.
  MetricsLogManager* log_manager() { return &log_manager_; }

 private:
  // The MetricsService has a lifecycle that is stored as a state.
  // See metrics_service.cc for description of this lifecycle.
  enum State {
    INITIALIZED,                    // Constructor was called.
    INIT_TASK_SCHEDULED,            // Waiting for deferred init tasks to
                                    // complete.
    INIT_TASK_DONE,                 // Waiting for timer to send initial log.
    SENDING_INITIAL_STABILITY_LOG,  // Initial stability log being sent.
    SENDING_INITIAL_METRICS_LOG,    // Initial metrics log being sent.
    SENDING_OLD_LOGS,               // Sending unsent logs from last session.
    SENDING_CURRENT_LOGS,           // Sending ongoing logs as they accrue.
  };

  enum ShutdownCleanliness {
    CLEANLY_SHUTDOWN = 0xdeadbeef,
    NEED_TO_SHUTDOWN = ~CLEANLY_SHUTDOWN
  };

  typedef std::vector<SyntheticTrialGroup> SyntheticTrialGroups;

  // Calls into the client to start metrics gathering.
  void StartGatheringMetrics();

  // Callback that moves the state to INIT_TASK_DONE. When this is called, the
  // state should be INIT_TASK_SCHEDULED.
  void FinishedGatheringInitialMetrics();

  void OnUserAction(const std::string& action);

  // Get the amount of uptime since this process started and since the last
  // call to this function.  Also updates the cumulative uptime metric (stored
  // as a pref) for uninstall.  Uptimes are measured using TimeTicks, which
  // guarantees that it is monotonic and does not jump if the user changes
  // his/her clock.  The TimeTicks implementation also makes the clock not
  // count time the computer is suspended.
  void GetUptimes(PrefService* pref,
                  base::TimeDelta* incremental_uptime,
                  base::TimeDelta* uptime);

  // Turns recording on or off.
  // DisableRecording() also forces a persistent save of logging state (if
  // anything has been recorded, or transmitted).
  void EnableRecording();
  void DisableRecording();

  // If in_idle is true, sets idle_since_last_transmission to true.
  // If in_idle is false and idle_since_last_transmission_ is true, sets
  // idle_since_last_transmission to false and starts the timer (provided
  // starting the timer is permitted).
  void HandleIdleSinceLastTransmission(bool in_idle);

  // Set up client ID, session ID, etc.
  void InitializeMetricsState();

  // Notifies providers when a new metrics log is created.
  void NotifyOnDidCreateMetricsLog();

  // Schedule the next save of LocalState information.  This is called
  // automatically by the task that performs each save to schedule the next one.
  void ScheduleNextStateSave();

  // Save the LocalState information immediately. This should not be called by
  // anybody other than the scheduler to avoid doing too many writes. When you
  // make a change, call ScheduleNextStateSave() instead.
  void SaveLocalState();

  // Opens a new log for recording user experience metrics.
  void OpenNewLog();

  // Closes out the current log after adding any last information.
  void CloseCurrentLog();

  // Pushes the text of the current and staged logs into persistent storage.
  // Called when Chrome shuts down.
  void PushPendingLogsToPersistentStorage();

  // Ensures that scheduler is running, assuming the current settings are such
  // that metrics should be reported. If not, this is a no-op.
  void StartSchedulerIfNecessary();

  // Starts the process of uploading metrics data.
  void StartScheduledUpload();

  // Called by the client when final log info collection is complete.
  void OnFinalLogInfoCollectionDone();

  // Either closes the current log or creates and closes the initial log
  // (depending on |state_|), and stages it for upload.
  void StageNewLog();

  // Returns true if any of the registered metrics providers have stability
  // metrics to report.
  bool ProvidersHaveStabilityMetrics();

  // Prepares the initial stability log, which is only logged when the previous
  // run of Chrome crashed.  This log contains any stability metrics left over
  // from that previous run, and only these stability metrics.  It uses the
  // system profile from the previous session.
  void PrepareInitialStabilityLog();

  // Prepares the initial metrics log, which includes startup histograms and
  // profiler data, as well as incremental stability-related metrics.
  void PrepareInitialMetricsLog();

  // Uploads the currently staged log (which must be non-null).
  void SendStagedLog();

  // Called after transmission completes (either successfully or with failure).
  void OnLogUploadComplete(int response_code);

  // Reads, increments and then sets the specified integer preference.
  void IncrementPrefValue(const char* path);

  // Reads, increments and then sets the specified long preference that is
  // stored as a string.
  void IncrementLongPrefsValue(const char* path);

  // Records that the browser was shut down cleanly.
  void LogCleanShutdown();

  // Records state that should be periodically saved, like uptime and
  // buffered plugin stability statistics.
  void RecordCurrentState(PrefService* pref);

  // Checks whether events should currently be logged.
  bool ShouldLogEvents();

  // Sets the value of the specified path in prefs and schedules a save.
  void RecordBooleanPrefValue(const char* path, bool value);

  // Returns a list of synthetic field trials that were active for the entire
  // duration of the current log.
  void GetCurrentSyntheticFieldTrials(
      std::vector<variations::ActiveGroupId>* synthetic_trials);

  // Creates a new MetricsLog instance with the given |log_type|.
  scoped_ptr<MetricsLog> CreateLog(MetricsLog::LogType log_type);

  // Record complete list of histograms into the current log.
  // Called when we close a log.
  void RecordCurrentHistograms();

  // Record complete list of stability histograms into the current log,
  // i.e., histograms with the |kUmaStabilityHistogramFlag| flag set.
  void RecordCurrentStabilityHistograms();

  // Manager for the various in-flight logs.
  MetricsLogManager log_manager_;

  // |histogram_snapshot_manager_| prepares histogram deltas for transmission.
  base::HistogramSnapshotManager histogram_snapshot_manager_;

  // Used to manage various metrics reporting state prefs, such as client id,
  // low entropy source and whether metrics reporting is enabled. Weak pointer.
  MetricsStateManager* const state_manager_;

  // Used to interact with the embedder. Weak pointer; must outlive |this|
  // instance.
  MetricsServiceClient* const client_;

  // Registered metrics providers.
  ScopedVector<MetricsProvider> metrics_providers_;

  PrefService* local_state_;

  CleanExitBeacon clean_exit_beacon_;

  base::ActionCallback action_callback_;

  // Indicate whether recording and reporting are currently happening.
  // These should not be set directly, but by calling SetRecording and
  // SetReporting.
  bool recording_active_;
  bool reporting_active_;

  // Indicate whether test mode is enabled, where the initial log should never
  // be cut, and logs are neither persisted nor uploaded.
  bool test_mode_active_;

  // The progression of states made by the browser are recorded in the following
  // state.
  State state_;

  // Whether the initial stability log has been recorded during startup.
  bool has_initial_stability_log_;

  // The initial metrics log, used to record startup metrics (histograms and
  // profiler data). Note that if a crash occurred in the previous session, an
  // initial stability log may be sent before this.
  scoped_ptr<MetricsLog> initial_metrics_log_;

  // Instance of the helper class for uploading logs.
  scoped_ptr<MetricsLogUploader> log_uploader_;

  // Whether there is a current log upload in progress.
  bool log_upload_in_progress_;

  // Whether the MetricsService object has received any notifications since
  // the last time a transmission was sent.
  bool idle_since_last_transmission_;

  // A number that identifies the how many times the app has been launched.
  int session_id_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as MetricsService.
  base::WeakPtrFactory<MetricsService> self_ptr_factory_;

  // Weak pointers factory used for saving state. All weak pointers managed by
  // this factory are invalidated in ScheduleNextStateSave.
  base::WeakPtrFactory<MetricsService> state_saver_factory_;

  // The scheduler for determining when uploads should happen.
  scoped_ptr<MetricsReportingScheduler> scheduler_;

  // Stores the time of the first call to |GetUptimes()|.
  base::TimeTicks first_updated_time_;

  // Stores the time of the last call to |GetUptimes()|.
  base::TimeTicks last_updated_time_;

  // Field trial groups that map to Chrome configuration states.
  SyntheticTrialGroups synthetic_trial_groups_;

  // Execution phase the browser is in.
  static ExecutionPhase execution_phase_;

  // Reduntant marker to check that we completed our shutdown, and set the
  // exited-cleanly bit in the prefs.
  static ShutdownCleanliness clean_shutdown_status_;

  friend class ::MetricsServiceAccessor;

  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, IsPluginProcess);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest,
                           PermutedEntropyCacheClearedWhenLowEntropyReset);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, RegisterSyntheticTrial);

  DISALLOW_COPY_AND_ASSIGN(MetricsService);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_H_
