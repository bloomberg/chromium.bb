// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that collects information about the user
// experience in order to help improve future versions of the app.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICE_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/user_metrics.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/metrics/tracking_synchronizer_observer.h"
#include "chrome/common/metrics/metrics_service_base.h"
#include "chrome/installer/util/google_update_settings.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/user_metrics.h"
#include "net/url_request/url_fetcher_delegate.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/activity_type_ids.h"
#elif defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/external_metrics.h"
#endif

class MetricsReportingScheduler;
class PrefService;
class PrefRegistrySimple;
class Profile;
class TemplateURLService;

namespace {
class CrashesDOMHandler;
class FlashDOMHandler;
}

namespace base {
class DictionaryValue;
class MessageLoopProxy;
}

namespace chrome_variations {
struct ActiveGroupId;
}

namespace content {
class RenderProcessHost;
class WebContents;
struct WebPluginInfo;
}

namespace extensions {
class ExtensionDownloader;
class ManifestFetchData;
class MetricsPrivateGetIsCrashReportingEnabledFunction;
}

namespace metrics {
class ClonedInstallDetector;
}

namespace net {
class URLFetcher;
}

namespace prerender {
bool IsOmniboxEnabled(Profile* profile);
}

namespace system_logs {
class ChromeInternalLogSource;
}

namespace tracked_objects {
struct ProcessDataSnapshot;
}

// A Field Trial and its selected group, which represent a particular
// Chrome configuration state. For example, the trial name could map to
// a preference name, and the group name could map to a preference value.
struct SyntheticTrialGroup {
 public:
  ~SyntheticTrialGroup();

  chrome_variations::ActiveGroupId id;
  base::TimeTicks start_time;

 private:
  friend class MetricsService;
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, RegisterSyntheticTrial);

  // This constructor is private specifically so as to control which code is
  // able to access it. New code that wishes to use it should be added as a
  // friend class.
  SyntheticTrialGroup(uint32 trial, uint32 group);
};

class MetricsService
    : public chrome_browser_metrics::TrackingSynchronizerObserver,
      public content::BrowserChildProcessObserver,
      public content::NotificationObserver,
      public net::URLFetcherDelegate,
      public MetricsServiceBase {
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

  enum ReportingState {
    REPORTING_ENABLED,
    REPORTING_DISABLED,
  };

  MetricsService();
  virtual ~MetricsService();

  // Initializes metrics recording state. Updates various bookkeeping values in
  // prefs and sets up the scheduler. This is a separate function rather than
  // being done by the constructor so that field trials could be created before
  // this is run. Takes |reporting_state| parameter which specifies whether UMA
  // is enabled.
  void InitializeMetricsRecordingState(ReportingState reporting_state);

  // Starts the metrics system, turning on recording and uploading of metrics.
  // Should be called when starting up with metrics enabled, or when metrics
  // are turned on.
  void Start();

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

  // Returns the preferred entropy provider used to seed persistent activities
  // based on whether or not metrics reporting will be permitted on this client.
  // The caller must determine if metrics reporting will be enabled for this
  // client and pass that state in as |reporting_will_be_enabled|.
  //
  // If |reporting_will_be_enabled| is true, this method returns an entropy
  // provider that has a high source of entropy, partially based on the client
  // ID. Otherwise, an entropy provider that is based on a low entropy source
  // is returned.
  //
  // Note that this reporting state can not be checked by reporting_active()
  // because this method may need to be called before the MetricsService needs
  // to be started.
  scoped_ptr<const base::FieldTrial::EntropyProvider> CreateEntropyProvider(
      ReportingState reporting_state);

  // Force the client ID to be generated. This is useful in case it's needed
  // before recording.
  void ForceClientIdCreation();

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefRegistrySimple* registry);
#if defined(OS_ANDROID)
  static void RegisterPrefsAndroid(PrefRegistrySimple* registry);
#endif  // defined(OS_ANDROID)

  // Set up notifications which indicate that a user is performing work. This is
  // useful to allow some features to sleep, until the machine becomes active,
  // such as precluding UMA uploads unless there was recent activity.
  static void SetUpNotifications(content::NotificationRegistrar* registrar,
                                 content::NotificationObserver* observer);

  // Implementation of content::BrowserChildProcessObserver
  virtual void BrowserChildProcessHostConnected(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessCrashed(
      const content::ChildProcessData& data) OVERRIDE;
  virtual void BrowserChildProcessInstanceCreated(
      const content::ChildProcessData& data) OVERRIDE;

  // Implementation of content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Invoked when we get a WM_SESSIONEND. This places a value in prefs that is
  // reset when RecordCompletedSessionEnd is invoked.
  void RecordStartOfSessionEnd();

  // This should be called when the application is shutting down. It records
  // that session end was successful.
  void RecordCompletedSessionEnd();

#if defined(OS_ANDROID)
  // Called to log launch and crash stats to preferences.
  void LogAndroidStabilityToPrefs(PrefService* pref);

  // Converts crash stats stored in the preferences into histograms.
  void ConvertAndroidStabilityPrefsToHistograms(PrefService* pref);

  // Called when the Activity that the user interacts with is swapped out.
  void OnForegroundActivityChanged(PrefService* pref,
                                   ActivityTypeIds::Type type);
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Called when the application is going into background mode.
  void OnAppEnterBackground();

  // Called when the application is coming out of background mode.
  void OnAppEnterForeground();
#else
  // Set the dirty flag, which will require a later call to LogCleanShutdown().
  static void LogNeedForCleanShutdown();
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

  static void SetExecutionPhase(ExecutionPhase execution_phase);

  // Saves in the preferences if the crash report registration was successful.
  // This count is eventually send via UMA logs.
  void RecordBreakpadRegistration(bool success);

  // Saves in the preferences if the browser is running under a debugger.
  // This count is eventually send via UMA logs.
  void RecordBreakpadHasDebugger(bool has_debugger);

#if defined(OS_WIN)
  // Counts (and removes) the browser crash dump attempt signals left behind by
  // any previous browser processes which generated a crash dump.
  void CountBrowserCrashDumpAttempts();
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
  // Start the external metrics service, which collects metrics from Chrome OS
  // and passes them to UMA.
  void StartExternalMetrics();

  // Records a Chrome OS crash.
  void LogChromeOSCrash(const std::string &crash_type);
#endif

  bool recording_active() const;
  bool reporting_active() const;

  void LogPluginLoadingError(const base::FilePath& plugin_path);

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

  // Check if this install was cloned or imaged from another machine. If a
  // clone is detected, reset the client id and low entropy source. This
  // should not be called more than once.
  void CheckForClonedInstall();

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

  // Designates which entropy source was returned from this MetricsService.
  // This is used for testing to validate that we return the correct source
  // depending on the state of the service.
  enum EntropySourceReturned {
    LAST_ENTROPY_NONE,
    LAST_ENTROPY_LOW,
    LAST_ENTROPY_HIGH,
  };

  struct ChildProcessStats;

  typedef std::vector<SyntheticTrialGroup> SyntheticTrialGroups;

  // First part of the init task. Called on the FILE thread to load hardware
  // class information.
  static void InitTaskGetHardwareClass(base::WeakPtr<MetricsService> self,
                                       base::MessageLoopProxy* target_loop);

  // Callback from InitTaskGetHardwareClass() that continues the init task by
  // loading plugin information.
  void OnInitTaskGotHardwareClass(const std::string& hardware_class);

  // Callback from PluginService::GetPlugins() that continues the init task by
  // launching a task to gather Google Update statistics.
  void OnInitTaskGotPluginInfo(
      const std::vector<content::WebPluginInfo>& plugins);

  // Task launched by OnInitTaskGotPluginInfo() that continues the init task by
  // loading Google Update statistics.  Called on a blocking pool thread.
  static void InitTaskGetGoogleUpdateData(base::WeakPtr<MetricsService> self,
                                          base::MessageLoopProxy* target_loop);

  // Callback from InitTaskGetGoogleUpdateData() that continues the init task by
  // loading profiler data.
  void OnInitTaskGotGoogleUpdateData(
      const GoogleUpdateMetrics& google_update_metrics);

  void OnUserAction(const std::string& action);

  // TrackingSynchronizerObserver:
  virtual void ReceivedProfilerData(
      const tracked_objects::ProcessDataSnapshot& process_data,
      int process_type) OVERRIDE;
  // Callback that moves the state to INIT_TASK_DONE.
  virtual void FinishedReceivingProfilerData() OVERRIDE;

  // Get the amount of uptime since this process started and since the last
  // call to this function.  Also updates the cumulative uptime metric (stored
  // as a pref) for uninstall.  Uptimes are measured using TimeTicks, which
  // guarantees that it is monotonic and does not jump if the user changes
  // his/her clock.  The TimeTicks implementation also makes the clock not
  // count time the computer is suspended.
  void GetUptimes(PrefService* pref,
                  base::TimeDelta* incremental_uptime,
                  base::TimeDelta* uptime);

  // Reset the client id and low entropy source if the kMetricsResetMetricIDs
  // pref is true.
  void ResetMetricsIDsIfNecessary();

  // Returns the low entropy source for this client. This is a random value
  // that is non-identifying amongst browser clients. This method will
  // generate the entropy source value if it has not been called before.
  int GetLowEntropySource();

  // Returns the first entropy source that was returned by this service since
  // start up, or NONE if neither was returned yet. This is exposed for testing
  // only.
  EntropySourceReturned entropy_source_returned() const {
    return entropy_source_returned_;
  }

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
  void InitializeMetricsState(ReportingState reporting_state);

  // Generates a new client ID to use to identify self to metrics server.
  static std::string GenerateClientID();

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

  // Starts collecting any data that should be added to a log just before it is
  // closed.
  void StartFinalLogInfoCollection();
  // Callbacks for various stages of final log info collection. Do not call
  // these directly.
  void OnMemoryDetailCollectionDone();
  void OnHistogramSynchronizationDone();
  void OnFinalLogInfoCollectionDone();

  // Either closes the current log or creates and closes the initial log
  // (depending on |state_|), and stages it for upload.
  void StageNewLog();

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

  // Prepared the staged log to be passed to the server. Upon return,
  // current_fetch_ should be reset with its upload data set to a compressed
  // copy of the staged log.
  void PrepareFetchWithStagedLog();

  // Implementation of net::URLFetcherDelegate. Called after transmission
  // completes (either successfully or with failure).
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Reads, increments and then sets the specified integer preference.
  void IncrementPrefValue(const char* path);

  // Reads, increments and then sets the specified long preference that is
  // stored as a string.
  void IncrementLongPrefsValue(const char* path);

  // Records a renderer process crash.
  void LogRendererCrash(content::RenderProcessHost* host,
                        base::TerminationStatus status,
                        int exit_code);

  // Records a renderer process hang.
  void LogRendererHang();

  // Records that the browser was shut down cleanly.
  void LogCleanShutdown();

  // Returns reference to ChildProcessStats corresponding to |data|.
  ChildProcessStats& GetChildProcessStats(
      const content::ChildProcessData& data);

  // Saves plugin-related updates from the in-object buffer to Local State
  // for retrieval next time we send a Profile log (generally next launch).
  void RecordPluginChanges(PrefService* pref);

  // Records state that should be periodically saved, like uptime and
  // buffered plugin stability statistics.
  void RecordCurrentState(PrefService* pref);

  // Logs the initiation of a page load and uses |web_contents| to do
  // additional logging of the type of page loaded.
  void LogLoadStarted(content::WebContents* web_contents);

  // Checks whether events should currently be logged.
  bool ShouldLogEvents();

  // Sets the value of the specified path in prefs and schedules a save.
  void RecordBooleanPrefValue(const char* path, bool value);

  // Returns true if process of type |type| should be counted as a plugin
  // process, and false otherwise.
  static bool IsPluginProcess(int process_type);

  // Returns a list of synthetic field trials that were active for the entire
  // duration of the current log.
  void GetCurrentSyntheticFieldTrials(
      std::vector<chrome_variations::ActiveGroupId>* synthetic_trials);

  base::ActionCallback action_callback_;

  content::NotificationRegistrar registrar_;

  // Set to true when |ResetMetricsIDsIfNecessary| is called for the first time.
  // This prevents multiple resets within the same Chrome session.
  bool metrics_ids_reset_check_performed_;

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

  // Chrome OS hardware class (e.g., hardware qualification ID). This
  // class identifies the configured system components such as CPU,
  // WiFi adapter, etc.  For non Chrome OS hosts, this will be an
  // empty string.
  std::string hardware_class_;

  // The list of plugins which was retrieved on the file thread.
  std::vector<content::WebPluginInfo> plugins_;

  // Google Update statistics, which were retrieved on a blocking pool thread.
  GoogleUpdateMetrics google_update_metrics_;

  // The initial metrics log, used to record startup metrics (histograms and
  // profiler data). Note that if a crash occurred in the previous session, an
  // initial stability log may be sent before this.
  scoped_ptr<MetricsLog> initial_metrics_log_;

  // The outstanding transmission appears as a URL Fetch operation.
  scoped_ptr<net::URLFetcher> current_fetch_;

  // The TCP/UDP echo server to collect network connectivity stats.
  std::string network_stats_server_;

  // The HTTP pipelining test server.
  std::string http_pipelining_test_server_;

  // The identifier that's sent to the server with the log reports.
  std::string client_id_;

  // The non-identifying low entropy source value.
  int low_entropy_source_;

  // Whether the MetricsService object has received any notifications since
  // the last time a transmission was sent.
  bool idle_since_last_transmission_;

  // A number that identifies the how many times the app has been launched.
  int session_id_;

  // Maps WebContentses (corresponding to tabs) or Browsers (corresponding to
  // Windows) to a unique integer that we will use to identify them.
  // |next_window_id_| is used to track which IDs we have used so far.
  typedef std::map<uintptr_t, int> WindowMap;
  WindowMap window_map_;
  int next_window_id_;

  // Buffer of child process notifications for quick access.
  std::map<base::string16, ChildProcessStats> child_process_stats_buffer_;

  // Weak pointers factory used to post task on different threads. All weak
  // pointers managed by this factory have the same lifetime as MetricsService.
  base::WeakPtrFactory<MetricsService> self_ptr_factory_;

  // Weak pointers factory used for saving state. All weak pointers managed by
  // this factory are invalidated in ScheduleNextStateSave.
  base::WeakPtrFactory<MetricsService> state_saver_factory_;

  // The scheduler for determining when uploads should happen.
  scoped_ptr<MetricsReportingScheduler> scheduler_;

  // Indicates that an asynchronous reporting step is running.
  // This is used only for debugging.
  bool waiting_for_asynchronous_reporting_step_;

  // Number of async histogram fetch requests in progress.
  int num_async_histogram_fetches_in_progress_;

#if defined(OS_CHROMEOS)
  // The external metric service is used to log ChromeOS UMA events.
  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;
#endif

  // The last entropy source returned by this service, used for testing.
  EntropySourceReturned entropy_source_returned_;

  // Stores the time of the first call to |GetUptimes()|.
  base::TimeTicks first_updated_time_;

  // Stores the time of the last call to |GetUptimes()|.
  base::TimeTicks last_updated_time_;

  // Execution phase the browser is in.
  static ExecutionPhase execution_phase_;

  // Reduntant marker to check that we completed our shutdown, and set the
  // exited-cleanly bit in the prefs.
  static ShutdownCleanliness clean_shutdown_status_;

  // Field trial groups that map to Chrome configuration states.
  SyntheticTrialGroups synthetic_trial_groups_;

  scoped_ptr<metrics::ClonedInstallDetector> cloned_install_detector_;

  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, ClientIdCorrectlyFormatted);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, IsPluginProcess);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, LowEntropySource0NotReset);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest,
                           PermutedEntropyCacheClearedWhenLowEntropyReset);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, RegisterSyntheticTrial);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, ResetMetricsIDs);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceBrowserTest,
                           CheckLowEntropySourceUsed);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceReportingTest,
                           CheckHighEntropySourceUsed);

  DISALLOW_COPY_AND_ASSIGN(MetricsService);
};

// This class limits and documents access to the IsMetricsReportingEnabled() and
// IsCrashReportingEnabled() methods. Since these methods are private, each user
// has to be explicitly declared as a 'friend' below.
class MetricsServiceHelper {
 private:
  friend bool prerender::IsOmniboxEnabled(Profile* profile);
  friend class ChromeRenderMessageFilter;
  friend class ::CrashesDOMHandler;
  friend class extensions::ExtensionDownloader;
  friend class extensions::ManifestFetchData;
  friend class extensions::MetricsPrivateGetIsCrashReportingEnabledFunction;
  friend class ::FlashDOMHandler;
  friend class system_logs::ChromeInternalLogSource;
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, MetricsReportingEnabled);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, CrashReportingEnabled);

  // Returns true if prefs::kMetricsReportingEnabled is set.
  static bool IsMetricsReportingEnabled();

  // Returns true if crash reporting is enabled.  This is set at the platform
  // level for Android and ChromeOS, and otherwise is the same as
  // IsMetricsReportingEnabled for desktop Chrome.
  static bool IsCrashReportingEnabled();

  DISALLOW_IMPLICIT_CONSTRUCTORS(MetricsServiceHelper);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICE_H_
