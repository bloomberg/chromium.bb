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
#include "base/scoped_ptr.h"
#include "chrome/common/metrics_helpers.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/external_metrics.h"
#endif

class BookmarkModel;
class BookmarkNode;
class DictionaryValue;
class ListValue;
class HistogramSynchronizer;
class MetricsLogBase;
class PrefService;
class TemplateURLModel;

namespace webkit {
namespace npapi {
struct WebPluginInfo;
}
}

// Forward declaration of the xmlNode to avoid having tons of gyp files
// needing to depend on the libxml third party lib.
struct _xmlNode;
typedef struct _xmlNode xmlNode;
typedef xmlNode* xmlNodePtr;


class MetricsService : public NotificationObserver,
                       public URLFetcher::Delegate,
                       public MetricsServiceBase {
 public:
  // Used to produce a historgram that keeps track of the status of recalling
  // persisted per logs.
  enum LogRecallStatus {
    RECALL_SUCCESS,         // We were able to correctly recall a persisted log.
    LIST_EMPTY,             // Attempting to recall from an empty list.
    LIST_SIZE_MISSING,      // Failed to recover list size using GetAsInteger().
    LIST_SIZE_TOO_SMALL,    // Too few elements in the list (less than 3).
    LIST_SIZE_CORRUPTION,   // List size is not as expected.
    LOG_STRING_CORRUPTION,  // Failed to recover log string using GetAsString().
    CHECKSUM_CORRUPTION,    // Failed to verify checksum.
    CHECKSUM_STRING_CORRUPTION,  // Failed to recover checksum string using
                                 // GetAsString().
    DECODE_FAIL,            // Failed to decode log.
    END_RECALL_STATUS       // Number of bins to use to create the histogram.
  };

  // TODO(ziadh): This is here temporarily for a side experiment. Remove later
  // on.
  enum LogStoreStatus {
    STORE_SUCCESS,    // Successfully presisted log.
    ENCODE_FAIL,      // Failed to encode log.
    COMPRESS_FAIL,    // Failed to compress log.
    END_STORE_STATUS  // Number of bins to use to create the histogram.
  };

  MetricsService();
  virtual ~MetricsService();

  // Sets whether the user permits uploading.  The argument of this function
  // should match the checkbox in Options.
  void SetUserPermitsUpload(bool enabled);

  // Start/stop the metrics recording and uploading machine.  These should be
  // used on startup and when the user clicks the checkbox in the prefs.
  // StartRecordingOnly starts the metrics recording but not reporting, for use
  // in tests only.
  void Start();
  void StartRecordingOnly();
  void Stop();

  // At startup, prefs needs to be called with a list of all the pref names and
  // types we'll be using.
  static void RegisterPrefs(PrefService* local_state);

  // Setup notifications which indicate that a user is performing work. This is
  // useful to allow some features to sleep, until the machine becomes active,
  // such as precluding UMA uploads unless there was recent activity.
  static void SetupNotifications(NotificationRegistrar* registrar,
                                 NotificationObserver* observer);

  // Implementation of NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // This should be called when the application is shutting down, to record
  // the fact that this was a clean shutdown in the stability metrics.
  void RecordCleanShutdown();

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

 private:
  // The MetricsService has a lifecycle that is stored as a state.
  // See metrics_service.cc for description of this lifecycle.
  enum State {
    INITIALIZED,            // Constructor was called.
    INIT_TASK_SCHEDULED,    // Waiting for deferred init tasks to complete.
    INIT_TASK_DONE,         // Waiting for timer to send initial log.
    INITIAL_LOG_READY,      // Initial log generated, and waiting for reply.
    SEND_OLD_INITIAL_LOGS,  // Sending unsent logs from previous session.
    SENDING_OLD_LOGS,       // Sending unsent logs from previous session.
    SENDING_CURRENT_LOGS,   // Sending standard current logs as they acrue.
  };

  class InitTask;
  class InitTaskComplete;

  // Callback to let us know that the init task is done.
  void OnInitTaskComplete(
      const std::string& hardware_class,
      const std::vector<webkit::npapi::WebPluginInfo>& plugins);

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
  // Return value "true" indicates setting was definitively set as requested).
  // Return value of "false" indicates that the enable state is effectively
  // stuck in the other logical setting.
  // Google Update maintains the authoritative preference in the registry, so
  // the caller *might* not be able to actually change the setting.
  // It is always possible to set this to at least one value, which matches the
  // current value reported by querying Google Update.
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

  // Called to stop recording user experience metrics.  The caller takes
  // ownership of the resulting MetricsLog object via the log parameter,
  // or passes in NULL to indicate that the log should simply be deleted.
  void StopRecording(MetricsLogBase** log);

  // Deletes pending_log_ and current_log_, and pushes their text into the
  // appropriate unsent_log vectors.  Called when Chrome shuts down.
  void PushPendingLogsToUnsentLists();

  // Save the pending_log_text_ persistently in a pref for transmission when we
  // next run.  Note that IF this text is "too large," we just dicard it.
  void PushPendingLogTextToUnsentOngoingLogs();

  // Start timer for next log transmission.
  void StartLogTransmissionTimer();

  // Internal function to collect process memory information.
  void LogTransmissionTimerDone();

  // Do not call OnMemoryDetailCollectionDone() or
  // OnHistogramSynchronizationDone() directly.
  // Use StartLogTransmissionTimer() to schedule a call.
  void OnMemoryDetailCollectionDone();
  void OnHistogramSynchronizationDone();

  // Takes whatever log should be uploaded next (according to the state_)
  // and makes it the pending log.  If pending_log_ is not NULL,
  // MakePendingLog does nothing and returns.
  void MakePendingLog();

  // Determines from state_ and permissions set out by the server and by
  // the user whether the pending_log_ should be sent or discarded.  Called by
  // TryToStartTransmission.
  bool TransmissionPermitted() const;

  // Check to see if there are any unsent logs from previous sessions.
  bool unsent_logs() const {
    return !unsent_initial_logs_.empty() || !unsent_ongoing_logs_.empty();
  }
  // Record stats, client ID, Session ID, etc. in a special "first" log.
  void PrepareInitialLog();
  // Pull copies of unsent logs from prefs into instance variables.
  void RecallUnsentLogs();
  // Decode and verify written pref log data.
  static MetricsService::LogRecallStatus RecallUnsentLogsHelper(
      const ListValue& list,
      std::vector<std::string>* local_list);
  // Encode and write list size and checksum for perf log data.
  static void StoreUnsentLogsHelper(const std::vector<std::string>& local_list,
                                    const size_t kMaxLocalListSize,
                                    ListValue* list);
  // Convert |pending_log_| to XML in |compressed_log_|, and compress it for
  // transmission.
  void PreparePendingLogText();

  // Convert pending_log_ to XML, compress it, and prepare to pass to server.
  // Upon return, current_fetch_ should be reset with its upload data set to
  // a compressed copy of the pending log.
  void PrepareFetchWithPendingLog();

  // Implementation of URLFetcher::Delegate. Called after transmission
  // completes (either successfully or with failure).
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Called by OnURLFetchComplete to handle the case when the server returned
  // a response code not equal to 200.
  void HandleBadResponseCode();

  // Class to hold all attributes that gets inherited by children in the UMA
  // response data xml tree.  This is to make it convenient in the
  // recursive function that does the tree traversal to pass all such
  // data in the recursive call.  If you want to add more such attributes,
  // add them to this class.
  class InheritedProperties {
    public:
    InheritedProperties() : salt(123123), denominator(1000000) {}
    int salt, denominator;
    // Notice salt and denominator are inherited from parent nodes, but
    // not probability; the default value of probability is 1.

    // When a new node is reached it might have fields which overwrite inherited
    // properties for that node (and its children).  Call this method to
    // overwrite those settings.
    void OverwriteWhereNeeded(xmlNodePtr node);
  };

  // Called by OnURLFetchComplete with data as the argument
  // parses the xml returned by the server in the call to OnURLFetchComplete
  // and extracts settings for subsequent frequency and content of log posts.
  void GetSettingsFromResponseData(const std::string& data);

  // This is a helper function for GetSettingsFromResponseData which iterates
  // through the xml tree at the level of the <chrome_config> node.
  void GetSettingsFromChromeConfigNode(xmlNodePtr chrome_config_node);

  // GetSettingsFromUploadNode handles iteration over the children of the
  // <upload> child of the <chrome_config> node.  It calls the recursive
  // function GetSettingsFromUploadNodeRecursive which does the actual
  // tree traversal.
  void GetSettingsFromUploadNode(xmlNodePtr upload_node);
  void GetSettingsFromUploadNodeRecursive(xmlNodePtr node,
      InheritedProperties props,
      std::string path_prefix,
      bool uploadOn);

  // NodeProbabilityTest gets called at every node in the tree traversal
  // performed by GetSettingsFromUploadNodeRecursive.  It determines from
  // the inherited attributes (salt, denominator) and the probability
  // assiciated with the node whether that node and its contents should
  // contribute to the upload.
  bool NodeProbabilityTest(xmlNodePtr node, InheritedProperties props) const;
  bool ProbabilityTest(double probability, int salt, int denominator) const;

  // Records a window-related notification.
  void LogWindowChange(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Reads, increments and then sets the specified integer preference.
  void IncrementPrefValue(const char* path);

  // Reads, increments and then sets the specified long preference that is
  // stored as a string.
  void IncrementLongPrefsValue(const char* path);

  // Records a renderer process crash.
  void LogRendererCrash();

  // Records an extension renderer process crash.
  void LogExtensionRendererCrash();

  // Records a renderer process hang.
  void LogRendererHang();

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
  void LogChildProcessChange(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details);

  // Logs keywords specific metrics. Keyword metrics are recorded in the
  // profile specific metrics.
  void LogKeywords(const TemplateURLModel* url_model);

  // Saves plugin-related updates from the in-object buffer to Local State
  // for retrieval next time we send a Profile log (generally next launch).
  void RecordPluginChanges(PrefService* pref);

  // Records state that should be periodically saved, like uptime and
  // buffered plugin stability statistics.
  void RecordCurrentState(PrefService* pref);

  // Logs the initiation of a page load
  void LogLoadStarted();

  // Records a page load notification.
  void LogLoadComplete(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Checks whether a notification can be logged.
  bool CanLogNotification(NotificationType type,
                          const NotificationSource& source,
                          const NotificationDetails& details);

  // Sets the value of the specified path in prefs and schedules a save.
  void RecordBooleanPrefValue(const char* path, bool value);

  NotificationRegistrar registrar_;

  // Indicate whether recording and reporting are currently happening.
  // These should not be set directly, but by calling SetRecording and
  // SetReporting.
  bool recording_active_;
  bool reporting_active_;

  // Coincides with the check box in options window that lets the user control
  // whether to upload.
  bool user_permits_upload_;

  // The variable server_permits_upload_ is set true when the response
  // data forbids uploading.  This should coinside with the "die roll"
  // with probability in the upload tag of the response data came out
  // affirmative.
  bool server_permits_upload_;

  // The progession of states made by the browser are recorded in the following
  // state.
  State state_;

  // Chrome OS hardware class (e.g., hardware qualification ID). This
  // class identifies the configured system components such as CPU,
  // WiFi adapter, etc.  For non Chrome OS hosts, this will be an
  // empty string.
  std::string hardware_class_;

  // The list of plugins which was retrieved on the file thread.
  std::vector<webkit::npapi::WebPluginInfo> plugins_;

  // The outstanding transmission appears as a URL Fetch operation.
  scoped_ptr<URLFetcher> current_fetch_;

  // The URL for the metrics server.
  std::wstring server_url_;

  // The identifier that's sent to the server with the log reports.
  std::string client_id_;

  // Whether the MetricsService object has received any notifications since
  // the last time a transmission was sent.
  bool idle_since_last_transmission_;

  // A number that identifies the how many times the app has been launched.
  int session_id_;

  // When logs were not sent during a previous session they are queued to be
  // sent instead of currently accumulating logs.  We give preference to sending
  // our inital log first, then unsent intial logs, then unsent ongoing logs.
  // Unsent logs are gathered at shutdown, and save in a persistent pref, one
  // log in each string in the following arrays.
  // Note that the vector has the oldest logs listed first (early in the
  // vector), and we'll discard old logs if we have gathered too many logs.
  std::vector<std::string> unsent_initial_logs_;
  std::vector<std::string> unsent_ongoing_logs_;

  // Maps NavigationControllers (corresponding to tabs) or Browser
  // (corresponding to Windows) to a unique integer that we will use to identify
  // it. |next_window_id_| is used to track which IDs we have used so far.
  typedef std::map<uintptr_t, int> WindowMap;
  WindowMap window_map_;
  int next_window_id_;

  // Buffer of child process notifications for quick access.  See
  // ChildProcessStats documentation above for more details.
  struct ChildProcessStats;
  std::map<std::wstring, ChildProcessStats> child_process_stats_buffer_;

  ScopedRunnableMethodFactory<MetricsService> log_sender_factory_;
  ScopedRunnableMethodFactory<MetricsService> state_saver_factory_;

  // Dictionary containing all the profile specific metrics. This is set
  // at creation time from the prefs.
  scoped_ptr<DictionaryValue> profile_dictionary_;

  // The interval between consecutive log transmissions (to avoid hogging the
  // outbound network link).  This is usually also the duration for which we
  // build up a log, but if other unsent-logs from previous sessions exist, we
  // quickly transmit those unsent logs while we continue to build a log.
  base::TimeDelta interlog_duration_;

  // The maximum number of events which get transmitted in a log.  This defaults
  // to a constant and otherwise is provided by the UMA server in the server
  // response data.
  int log_event_limit_;

  // Indicate that a timer for sending the next log has already been queued.
  bool timer_pending_;

#if defined(OS_CHROMEOS)
  // The external metric service is used to log ChromeOS UMA events.
  scoped_refptr<chromeos::ExternalMetrics> external_metrics_;
#endif

  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, EmptyLogList);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, SingleElementLogList);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, OverLimitLogList);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, SmallRecoveredListSize);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, RemoveSizeFromLogList);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, CorruptSizeOfLogList);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, CorruptChecksumOfLogList);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, ClientIdGeneratesAllZeroes);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, ClientIdGeneratesCorrectly);
  FRIEND_TEST_ALL_PREFIXES(MetricsServiceTest, ClientIdCorrectlyFormatted);

  DISALLOW_COPY_AND_ASSIGN(MetricsService);
};

#endif  // CHROME_BROWSER_METRICS_METRICS_SERVICE_H_
