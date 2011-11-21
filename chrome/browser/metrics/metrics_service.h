// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that collects information about the user
// experience in order to help improve future versions of the app.

#ifndef CHROME_BROWSER_METRICS_METRICS_SERVICE_H_
#define CHROME_BROWSER_METRICS_METRICS_SERVICE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/process_util.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/metrics_helpers.h"
#include "content/common/child_process_info.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/external_metrics.h"
#endif

class BookmarkModel;
class BookmarkNode;
class MetricsReportingScheduler;
class PrefService;
class Profile;
class TemplateURLService;

namespace base {
class DictionaryValue;
class MessageLoopProxy;
}

namespace content {
class RenderProcessHost;
}

namespace prerender {
bool IsOmniboxEnabled(Profile* profile);
}

namespace webkit {
struct WebPluginInfo;
}


class MetricsService : public content::NotificationObserver,
                       public content::URLFetcherDelegate,
                       public MetricsServiceBase {
 public:
  MetricsService();
  virtual ~MetricsService();

  // Start/stop the metrics recording and uploading machine.  These should be
  // used on startup and when the user clicks the checkbox in the prefs.
  // StartRecordingOnly starts the metrics recording but not reporting, for use
  // in tests only.
  void Start();
  void StartRecordingOnly();
  void Stop();

  // Returns the client ID for this client, or the empty string if metrics
  // recording is not currently running.
  std::string GetClientId();

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefService* local_state);

  // Set up notifications which indicate that a user is performing work. This is
  // useful to allow some features to sleep, until the machine becomes active,
  // such as precluding UMA uploads unless there was recent activity.
  static void SetUpNotifications(content::NotificationRegistrar* registrar,
                                 content::NotificationObserver* observer);

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

  // Saves in the preferences if the crash report registration was successful.
  // This count is eventually send via UMA logs.
  void RecordBreakpadRegistration(bool success);

  // Saves in the preferences if the browser is running under a debugger.
  // This count is eventually send via UMA logs.
  void RecordBreakpadHasDebugger(bool has_debugger);

  // Save any unsent logs into a persistent store in a pref.  We always do this
  // at shutdown, but we can do it as we reduce the list as well.
  void StoreUnsentLogs();

#if defined(OS_CHROMEOS)
  // Start the external metrics service, which collects metrics from Chrome OS
  // and passes them to UMA.
  void StartExternalMetrics();

  // Records a Chrome OS crash.
  void LogChromeOSCrash(const std::string &crash_type);
#endif

  bool recording_active() const;
  bool reporting_active() const;

  // Redundant test to ensure that we are notified of a clean exit.
  // This value should be true when process has completed shutdown.
  static bool UmaMetricsProperlyShutdown();

  // Set the dirty flag, which will require a later call to LogCleanShutdown().
  static void LogNeedForCleanShutdown();

 private:
  // The MetricsService has a lifecycle that is stored as a state.
  // See metrics_service.cc for description of this lifecycle.
  enum State {
    INITIALIZED,            // Constructor was called.
    INIT_TASK_SCHEDULED,    // Waiting for deferred init tasks to complete.
    INIT_TASK_DONE,         // Waiting for timer to send initial log.
    INITIAL_LOG_READY,      // Initial log generated, and waiting for reply.
    SENDING_OLD_LOGS,       // Sending unsent logs from previous session.
    SENDING_CURRENT_LOGS,   // Sending standard current logs as they acrue.
  };

  enum ShutdownCleanliness {
    CLEANLY_SHUTDOWN = 0xdeadbeef,
    NEED_TO_SHUTDOWN = ~CLEANLY_SHUTDOWN
  };

  // First part of the init task. Called on the FILE thread to load hardware
  // class information.
  void InitTaskGetHardwareClass(base::MessageLoopProxy* target_loop);

  // Callback from InitTaskGetHardwareClass() that continues the init task by
  // loading plugin information.
  void OnInitTaskGotHardwareClass(const std::string& hardware_class);

  // Callback from PluginService::GetPlugins() that moves the state to
  // INIT_TASK_DONE.
  void OnInitTaskGotPluginInfo(
      const std::vector<webkit::WebPluginInfo>& plugins);

  // When we start a new version of Chromium (different from our last run), we
  // need to discard the old crash stats so that we don't attribute crashes etc.
  // in the old version to the current version (via current logs).
  // Without this, a common reason to finally start a new version is to crash
  // the old version (after an autoupdate has arrived), and so we'd bias
  // initial results towards showing crashes :-(.
  static void DiscardOldStabilityStats(PrefService* local_state);

  // Sets and gets whether metrics recording is active.
  // SetRecording(false) also forces a persistent save of logging state (if
  // anything has been recorded, or transmitted).
  void SetRecording(bool enabled);

  // Enable/disable transmission of accumulated logs and crash reports (dumps).
  void SetReporting(bool enabled);

  // If in_idle is true, sets idle_since_last_transmission to true.
  // If in_idle is false and idle_since_last_transmission_ is true, sets
  // idle_since_last_transmission to false and starts the timer (provided
  // starting the timer is permitted).
  void HandleIdleSinceLastTransmission(bool in_idle);

  // Set up client ID, session ID, etc.
  void InitializeMetricsState();

  // Generates a new client ID to use to identify self to metrics server.
  static std::string GenerateClientID();

  // Schedule the next save of LocalState information.  This is called
  // automatically by the task that performs each save to schedule the next one.
  void ScheduleNextStateSave();

  // Save the LocalState information immediately. This should not be called by
  // anybody other than the scheduler to avoid doing too many writes. When you
  // make a change, call ScheduleNextStateSave() instead.
  void SaveLocalState();

  // Called to start recording user experience metrics.
  // Constructs a new, empty current_log_.
  void StartRecording();

  // Called to stop recording user experience metrics.
  // Adds any last information to current_log_ and then stages it for upload.
  void StopRecording();

  // Pushes the text of the current and staged logs into persistent storage.
  // Called when Chrome shuts down.
  void PushPendingLogsToPersistentStorage();

  // Ensures that scheduler is running, assuming the current settings are such
  // that metrics should be reported. If not, this is a no-op.
  void StartSchedulerIfNecessary();

  // Starts the process of uploading metrics data.
  void StartScheduledUpload();

  // Do not call OnMemoryDetailCollectionDone() or
  // OnHistogramSynchronizationDone() directly; use
  // StartSchedulerIfNecessary() to schedule a call.
  void OnMemoryDetailCollectionDone();
  void OnHistogramSynchronizationDone();

  // Takes whatever log should be uploaded next (according to the state_)
  // and makes it the staged log.  If there is already a staged log, this is a
  // no-op.
  void MakeStagedLog();

  // Record stats, client ID, Session ID, etc. in a special "first" log.
  void PrepareInitialLog();

  // Prepared the staged log to be passed to the server. Upon return,
  // current_fetch_ should be reset with its upload data set to a compressed
  // copy of the staged log.
  void PrepareFetchWithStagedLog();

  // Implementation of content::URLFetcherDelegate. Called after transmission
  // completes (either successfully or with failure).
  virtual void OnURLFetchComplete(const content::URLFetcher* source) OVERRIDE;

  // Logs debugging details, for the case where the server returns a response
  // code other than 200.
  void LogBadResponseCode();

  // Records a window-related notification.
  void LogWindowChange(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Reads, increments and then sets the specified integer preference.
  void IncrementPrefValue(const char* path);

  // Reads, increments and then sets the specified long preference that is
  // stored as a string.
  void IncrementLongPrefsValue(const char* path);

  // Records a renderer process crash.
  void LogRendererCrash(content::RenderProcessHost* host,
                        base::TerminationStatus status,
                        bool was_alive);

  // Records a renderer process hang.
  void LogRendererHang();

  // Records that the browser was shut down cleanly.
  void LogCleanShutdown();

  // Set the value in preferences for the number of bookmarks and folders
  // in node. The pref key for the number of bookmarks in num_bookmarks_key and
  // the pref key for number of folders in num_folders_key.
  void LogBookmarks(const BookmarkNode* node,
                    const char* num_bookmarks_key,
                    const char* num_folders_key);

  // Sets preferences for the number of bookmarks in model.
  void LogBookmarks(BookmarkModel* model);

  // Records a child process related notification.  These are recorded to an
  // in-object buffer because these notifications are sent on page load, and we
  // don't want to slow that down.
  void LogChildProcessChange(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details);

  // Logs keywords specific metrics. Keyword metrics are recorded in the
  // profile specific metrics.
  void LogKeywords(const TemplateURLService* url_model);

  // Saves plugin-related updates from the in-object buffer to Local State
  // for retrieval next time we send a Profile log (generally next launch).
  void RecordPluginChanges(PrefService* pref);

  // Records state that should be periodically saved, like uptime and
  // buffered plugin stability statistics.
  void RecordCurrentState(PrefService* pref);

  // Logs the initiation of a page load
  void LogLoadStarted();

  // Records a page load notification.
  void LogLoadComplete(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Checks whether a notification can be logged.
  bool CanLogNotification(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details);

  // Sets the value of the specified path in prefs and schedules a save.
  void RecordBooleanPrefValue(const char* path, bool value);

  // Returns true if process of type |type| should be counted as a plugin
  // process, and false otherwise.
  static bool IsPluginProcess(ChildProcessInfo::ProcessType type);

  content::NotificationRegistrar registrar_;

  // Indicate whether recording and reporting are currently happening.
  // These should not be set directly, but by calling SetRecording and
  // SetReporting.
  bool recording_active_;
  bool reporting_active_;

  // The progession of states made by the browser are recorded in the following
  // state.
  State state_;

  // Chrome OS hardware class (e.g., hardware qualification ID). This
  // class identifies the configured system components such as CPU,
  // WiFi adapter, etc.  For non Chrome OS hosts, this will be an
  // empty string.
  std::string hardware_class_;

  // The list of plugins which was retrieved on the file thread.
  std::vector<webkit::WebPluginInfo> plugins_;

  // The outstanding transmission appears as a URL Fetch operation.
  scoped_ptr<content::URLFetcher> current_fetch_;

  // The URL for the metrics server.
  std::wstring server_url_;

  // The TCP/UDP echo server to collect network connectivity stats.
  std::string network_stats_server_;

  // The IOThread for accessing global HostResolver to resolve
  // network_stats_server_ host. |io_thread_| is accessed on IO thread and it is
  // safe to access it on IO thread.
  IOThread* io_thread_;

  // The identifier that's sent to the server with the log reports.
  std::string client_id_;

  // Whether the MetricsService object has received any notifications since
  // the last time a transmission was sent.
  bool idle_since_last_transmission_;

  // A number that identifies the how many times the app has been launched.
  int session_id_;

  // Maps NavigationControllers (corresponding to tabs) or Browser
  // (corresponding to Windows) to a unique integer that we will use to identify
  // it. |next_window_id_| is used to track which IDs we have used so far.
  typedef std::map<uintptr_t, int> WindowMap;
  WindowMap window_map_;
  int next_window_id_;

  // Buffer of child process notifications for quick access.  See
  // ChildProcessStats documentation above for more details.
  struct ChildProcessStats;
  std::map<string16, ChildProcessStats> child_process_stats_buffer_;

  ScopedRunnableMethodFactory<MetricsService> log_sender_factory_;
  base::WeakPtrFactory<MetricsService> state_saver_factory_;

  // Dictionary containing all the profile specific metrics. This is set
  // at creation time from the prefs.
  scoped_ptr<base::DictionaryValue> profile_dictionary_;

  // The scheduler for determining when uploads should happen.
  scoped_ptr<MetricsReportingScheduler> scheduler_;

  // Indicates that an asynchronous reporting step is running.
  // This is used only for debugging.
  bool waiting_for_asynchronus_reporting_step_;

#if defined(OS_CHROMEOS)
  // The external metric service is used to log ChromeOS UMA events.
  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;
#endif

  // Reduntant marker to check that we completed our shutdown, and set the
  // exited-cleanly bit in the prefs.
  static ShutdownCleanliness clean_shutdown_status_;

  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, ClientIdCorrectlyFormatted);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, IsPluginProcess);

  DISALLOW_COPY_AND_ASSIGN(MetricsService);
};

// This class limits and documents access to the IsMetricsReportingEnabled()
// method. Since the method is private, each user has to be explicitly declared
// as a 'friend' below.
class MetricsServiceHelper {
 private:
  friend class InstantFieldTrial;
  friend bool prerender::IsOmniboxEnabled(Profile* profile);

  // Returns true if prefs::kMetricsReportingEnabled is set.
  static bool IsMetricsReportingEnabled();

  DISALLOW_IMPLICIT_CONSTRUCTORS(MetricsServiceHelper);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICE_H_
