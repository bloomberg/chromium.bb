// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//------------------------------------------------------------------------------
// Description of the life cycle of a instance of MetricsService.
//
//  OVERVIEW
//
// A MetricsService instance is typically created at application startup.  It
// is the central controller for the acquisition of log data, and the automatic
// transmission of that log data to an external server.  Its major job is to
// manage logs, grouping them for transmission, and transmitting them.  As part
// of its grouping, MS finalizes logs by including some just-in-time gathered
// memory statistics, snapshotting the current stats of numerous histograms,
// closing the logs, translating to XML text, and compressing the results for
// transmission.  Transmission includes submitting a compressed log as data in a
// URL-post, and retransmitting (or retaining at process termination) if the
// attempted transmission failed.  Retention across process terminations is done
// using the the PrefServices facilities. The retained logs (the ones that never
// got transmitted) are compressed and base64-encoded before being persisted.
//
// Logs fall into one of two categories: "initial logs," and "ongoing logs."
// There is at most one initial log sent for each complete run of the chromium
// product (from startup, to browser shutdown).  An initial log is generally
// transmitted some short time (1 minute?) after startup, and includes stats
// such as recent crash info, the number and types of plugins, etc.  The
// external server's response to the initial log conceptually tells this MS if
// it should continue transmitting logs (during this session). The server
// response can actually be much more detailed, and always includes (at a
// minimum) how often additional ongoing logs should be sent.
//
// After the above initial log, a series of ongoing logs will be transmitted.
// The first ongoing log actually begins to accumulate information stating when
// the MS was first constructed.  Note that even though the initial log is
// commonly sent a full minute after startup, the initial log does not include
// much in the way of user stats.   The most common interlog period (delay)
// is 20 minutes. That time period starts when the first user action causes a
// logging event.  This means that if there is no user action, there may be long
// periods without any (ongoing) log transmissions.  Ongoing logs typically
// contain very detailed records of user activities (ex: opened tab, closed
// tab, fetched URL, maximized window, etc.)  In addition, just before an
// ongoing log is closed out, a call is made to gather memory statistics.  Those
// memory statistics are deposited into a histogram, and the log finalization
// code is then called.  In the finalization, a call to a Histogram server
// acquires a list of all local histograms that have been flagged for upload
// to the UMA server.  The finalization also acquires a the most recent number
// of page loads, along with any counts of renderer or plugin crashes.
//
// When the browser shuts down, there will typically be a fragment of an ongoing
// log that has not yet been transmitted.  At shutdown time, that fragment
// is closed (including snapshotting histograms), and persisted, for
// potential transmission during a future run of the product.
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
// stats, based on the State enum specified in the state_ member.  Those states
// are:
//
//    INITIALIZED,            // Constructor was called.
//    INIT_TASK_SCHEDULED,    // Waiting for deferred init tasks to complete.
//    INIT_TASK_DONE,         // Waiting for timer to send initial log.
//    INITIAL_LOG_READY,      // Initial log generated, and waiting for reply.
//    SENDING_OLD_LOGS,       // Sending unsent logs from previous session.
//    SENDING_CURRENT_LOGS,   // Sending standard current logs as they accrue.
//
// In more detail, we have:
//
//    INITIALIZED,            // Constructor was called.
// The MS has been constructed, but has taken no actions to compose the
// initial log.
//
//    INIT_TASK_SCHEDULED,    // Waiting for deferred init tasks to complete.
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
//    INITIAL_LOG_READY,      // Initial log generated, and waiting for reply.
// This state is entered only after an initial log has been composed, and
// prepared for transmission.  It is also the case that any previously unsent
// logs have been loaded into instance variables for possible transmission.
//
//    SENDING_OLD_LOGS,       // Sending unsent logs from previous session.
// This state indicates that the initial log for this session has been
// successfully sent and it is now time to send any logs that were
// saved from previous sessions.  All such logs will be transmitted before
// exiting this state, and proceeding with ongoing logs from the current session
// (see next state).
//
//    SENDING_CURRENT_LOGS,   // Sending standard current logs as they accrue.
// Current logs are being accumulated.  Typically every 20 minutes a log is
// closed and finalized for transmission, at the same time as a new log is
// started.
//
// The progression through the above states is simple, and sequential, in the
// most common use cases.  States proceed from INITIAL to SENDING_CURRENT_LOGS,
// and remain in the latter until shutdown.
//
// The one unusual case is when the user asks that we stop logging.  When that
// happens, any staged (transmission in progress) log is persisted, and any log
// log that is currently accumulating is also finalized and persisted.  We then
// regress back to the SEND_OLD_LOGS state in case the user enables log
// recording again during this session.  This way anything we have persisted
// will be sent automatically if/when we progress back to SENDING_CURRENT_LOG
// state.
//
// Also note that whenever we successfully send an old log, we mirror the list
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

#include "chrome/browser/metrics/metrics_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/md5.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/tracked_objects.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/process_map.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/metrics/histogram_synchronizer.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/metrics/metrics_log_serializer.h"
#include "chrome/browser/metrics/metrics_reporting_scheduler.h"
#include "chrome/browser/metrics/tracking_synchronizer.h"
#include "chrome/browser/net/http_pipelining_compatibility_client.h"
#include "chrome/browser/net/network_stats.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/guid.h"
#include "chrome/common/metrics/metrics_log_manager.h"
#include "chrome/common/net/test_server_locations.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/load_flags.h"
#include "webkit/plugins/webplugininfo.h"

// TODO(port): port browser_distribution.h.
#if !defined(OS_POSIX)
#include "chrome/installer/util/browser_distribution.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#endif

using base::Time;
using content::BrowserThread;
using content::ChildProcessData;
using content::LoadNotificationDetails;
using content::PluginService;

namespace {

// Check to see that we're being called on only one thread.
bool IsSingleThreaded() {
  static base::PlatformThreadId thread_id = 0;
  if (!thread_id)
    thread_id = base::PlatformThread::CurrentId();
  return base::PlatformThread::CurrentId() == thread_id;
}

// The delay, in seconds, after starting recording before doing expensive
// initialization work.
const int kInitializationDelaySeconds = 30;

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

// The maximum number of events in a log uploaded to the UMA server.
const int kEventLimit = 2400;

// If an upload fails, and the transmission was over this byte count, then we
// will discard the log, and not try to retransmit it.  We also don't persist
// the log to the prefs for transmission during the next chrome session if this
// limit is exceeded.
const size_t kUploadLogAvoidRetransmitSize = 50000;

// Interval, in minutes, between state saves.
const int kSaveStateIntervalMinutes = 5;

// Used to indicate that the response code is currently not set at all --
// RESPONSE_CODE_INVALID can sometimes be returned in response to a request if,
// e.g., the server is down.
const int kNoResponseCode = content::URLFetcher::RESPONSE_CODE_INVALID - 1;

enum ResponseStatus {
  UNKNOWN_FAILURE,
  SUCCESS,
  BAD_REQUEST,  // Invalid syntax or log too large.
  NUM_RESPONSE_STATUSES
};

ResponseStatus ResponseCodeToStatus(int response_code) {
  switch (response_code) {
    case 200:
      return SUCCESS;
    case 400:
      return BAD_REQUEST;
    default:
      return UNKNOWN_FAILURE;
  }
}

}

// static
MetricsService::ShutdownCleanliness MetricsService::clean_shutdown_status_ =
    MetricsService::CLEANLY_SHUTDOWN;

// This is used to quickly log stats from child process related notifications in
// MetricsService::child_stats_buffer_.  The buffer's contents are transferred
// out when Local State is periodically saved.  The information is then
// reported to the UMA server on next launch.
struct MetricsService::ChildProcessStats {
 public:
  explicit ChildProcessStats(content::ProcessType type)
      : process_launches(0),
        process_crashes(0),
        instances(0),
        process_type(type) {}

  // This constructor is only used by the map to return some default value for
  // an index for which no value has been assigned.
  ChildProcessStats()
      : process_launches(0),
        process_crashes(0),
        instances(0),
        process_type(content::PROCESS_TYPE_UNKNOWN) {}

  // The number of times that the given child process has been launched
  int process_launches;

  // The number of times that the given child process has crashed
  int process_crashes;

  // The number of instances of this child process that have been created.
  // An instance is a DOM object rendered by this child process during a page
  // load.
  int instances;

  content::ProcessType process_type;
};

// Handles asynchronous fetching of memory details.
// Will run the provided task after finished.
class MetricsMemoryDetails : public MemoryDetails {
 public:
  explicit MetricsMemoryDetails(const base::Closure& callback)
      : callback_(callback) {}

  virtual void OnDetailsAvailable() {
    MessageLoop::current()->PostTask(FROM_HERE, callback_);
  }

 private:
  ~MetricsMemoryDetails() {}

  base::Closure callback_;
  DISALLOW_COPY_AND_ASSIGN(MetricsMemoryDetails);
};

// static
void MetricsService::RegisterPrefs(PrefService* local_state) {
  DCHECK(IsSingleThreaded());
  local_state->RegisterStringPref(prefs::kMetricsClientID, "");
  local_state->RegisterInt64Pref(prefs::kMetricsClientIDTimestamp, 0);
  local_state->RegisterInt64Pref(prefs::kStabilityLaunchTimeSec, 0);
  local_state->RegisterInt64Pref(prefs::kStabilityLastTimestampSec, 0);
  local_state->RegisterStringPref(prefs::kStabilityStatsVersion, "");
  local_state->RegisterInt64Pref(prefs::kStabilityStatsBuildTime, 0);
  local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, true);
  local_state->RegisterBooleanPref(prefs::kStabilitySessionEndCompleted, true);
  local_state->RegisterIntegerPref(prefs::kMetricsSessionID, -1);
  local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityCrashCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityIncompleteSessionEndCount,
                                   0);
  local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityExtensionRendererCrashCount,
                                   0);
  local_state->RegisterIntegerPref(prefs::kStabilityRendererHangCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityChildProcessCrashCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityBreakpadRegistrationFail,
                                   0);
  local_state->RegisterIntegerPref(prefs::kStabilityBreakpadRegistrationSuccess,
                                   0);
  local_state->RegisterIntegerPref(prefs::kStabilityDebuggerPresent, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityDebuggerNotPresent, 0);
#if defined(OS_CHROMEOS)
  local_state->RegisterIntegerPref(prefs::kStabilityOtherUserCrashCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilityKernelCrashCount, 0);
  local_state->RegisterIntegerPref(prefs::kStabilitySystemUncleanShutdownCount,
                                   0);
#endif  // OS_CHROMEOS

  local_state->RegisterDictionaryPref(prefs::kProfileMetrics);
  local_state->RegisterIntegerPref(prefs::kNumBookmarksOnBookmarkBar, 0);
  local_state->RegisterIntegerPref(prefs::kNumFoldersOnBookmarkBar, 0);
  local_state->RegisterIntegerPref(prefs::kNumBookmarksInOtherBookmarkFolder,
                                   0);
  local_state->RegisterIntegerPref(prefs::kNumFoldersInOtherBookmarkFolder, 0);
  local_state->RegisterIntegerPref(prefs::kNumKeywords, 0);
  local_state->RegisterListPref(prefs::kMetricsInitialLogsXml);
  local_state->RegisterListPref(prefs::kMetricsOngoingLogsXml);
  local_state->RegisterListPref(prefs::kMetricsInitialLogsProto);
  local_state->RegisterListPref(prefs::kMetricsOngoingLogsProto);

  local_state->RegisterInt64Pref(prefs::kUninstallMetricsPageLoadCount, 0);
  local_state->RegisterInt64Pref(prefs::kUninstallLaunchCount, 0);
  local_state->RegisterInt64Pref(prefs::kUninstallMetricsInstallDate, 0);
  local_state->RegisterInt64Pref(prefs::kUninstallMetricsUptimeSec, 0);
  local_state->RegisterInt64Pref(prefs::kUninstallLastLaunchTimeSec, 0);
  local_state->RegisterInt64Pref(prefs::kUninstallLastObservedRunTimeSec, 0);
}

// static
void MetricsService::DiscardOldStabilityStats(PrefService* local_state) {
  local_state->SetBoolean(prefs::kStabilityExitedCleanly, true);
  local_state->SetBoolean(prefs::kStabilitySessionEndCompleted, true);

  local_state->SetInteger(prefs::kStabilityIncompleteSessionEndCount, 0);
  local_state->SetInteger(prefs::kStabilityBreakpadRegistrationSuccess, 0);
  local_state->SetInteger(prefs::kStabilityBreakpadRegistrationFail, 0);
  local_state->SetInteger(prefs::kStabilityDebuggerPresent, 0);
  local_state->SetInteger(prefs::kStabilityDebuggerNotPresent, 0);

  local_state->SetInteger(prefs::kStabilityLaunchCount, 0);
  local_state->SetInteger(prefs::kStabilityCrashCount, 0);

  local_state->SetInteger(prefs::kStabilityPageLoadCount, 0);
  local_state->SetInteger(prefs::kStabilityRendererCrashCount, 0);
  local_state->SetInteger(prefs::kStabilityRendererHangCount, 0);

  local_state->SetInt64(prefs::kStabilityLaunchTimeSec, 0);
  local_state->SetInt64(prefs::kStabilityLastTimestampSec, 0);

  local_state->ClearPref(prefs::kStabilityPluginStats);

  local_state->ClearPref(prefs::kMetricsInitialLogsXml);
  local_state->ClearPref(prefs::kMetricsOngoingLogsXml);
  local_state->ClearPref(prefs::kMetricsInitialLogsProto);
  local_state->ClearPref(prefs::kMetricsOngoingLogsProto);
}

MetricsService::MetricsService()
    : recording_active_(false),
      reporting_active_(false),
      state_(INITIALIZED),
      idle_since_last_transmission_(false),
      next_window_id_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(self_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(state_saver_factory_(this)),
      waiting_for_asynchronus_reporting_step_(false) {
  DCHECK(IsSingleThreaded());
  InitializeMetricsState();

  base::Closure callback = base::Bind(&MetricsService::StartScheduledUpload,
                                      self_ptr_factory_.GetWeakPtr());
  scheduler_.reset(new MetricsReportingScheduler(callback));
  log_manager_.set_log_serializer(new MetricsLogSerializer());
  log_manager_.set_max_ongoing_log_store_size(kUploadLogAvoidRetransmitSize);
}

MetricsService::~MetricsService() {
  SetRecording(false);
}

void MetricsService::Start() {
  HandleIdleSinceLastTransmission(false);
  SetRecording(true);
  SetReporting(true);
}

void MetricsService::StartRecordingOnly() {
  SetRecording(true);
  SetReporting(false);
}

void MetricsService::Stop() {
  HandleIdleSinceLastTransmission(false);
  SetReporting(false);
  SetRecording(false);
}

std::string MetricsService::GetClientId() {
  return client_id_;
}

void MetricsService::ForceClientIdCreation() {
  if (!client_id_.empty())
    return;
  PrefService* pref = g_browser_process->local_state();
  client_id_ = pref->GetString(prefs::kMetricsClientID);
  if (!client_id_.empty())
    return;

  client_id_ = GenerateClientID();
  pref->SetString(prefs::kMetricsClientID, client_id_);

  // Might as well make a note of how long this ID has existed
  pref->SetString(prefs::kMetricsClientIDTimestamp,
                  base::Int64ToString(Time::Now().ToTimeT()));
}

void MetricsService::SetRecording(bool enabled) {
  DCHECK(IsSingleThreaded());

  if (enabled == recording_active_)
    return;

  if (enabled) {
    ForceClientIdCreation();
    child_process_logging::SetClientId(client_id_);
    StartRecording();

    SetUpNotifications(&registrar_, this);
  } else {
    registrar_.RemoveAll();
    PushPendingLogsToPersistentStorage();
    DCHECK(!log_manager_.has_staged_log());
    if (state_ > INITIAL_LOG_READY && log_manager_.has_unsent_logs())
      state_ = SENDING_OLD_LOGS;
  }
  recording_active_ = enabled;
}

bool MetricsService::recording_active() const {
  DCHECK(IsSingleThreaded());
  return recording_active_;
}

void MetricsService::SetReporting(bool enable) {
  if (reporting_active_ != enable) {
    reporting_active_ = enable;
    if (reporting_active_)
      StartSchedulerIfNecessary();
  }
}

bool MetricsService::reporting_active() const {
  DCHECK(IsSingleThreaded());
  return reporting_active_;
}

// static
void MetricsService::SetUpNotifications(
    content::NotificationRegistrar* registrar,
    content::NotificationObserver* observer) {
  registrar->Add(observer, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar->Add(observer, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_USER_ACTION,
                 content::NotificationService::AllSources());
  registrar->Add(observer, chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllSources());
  registrar->Add(observer, chrome::NOTIFICATION_TAB_CLOSING,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_RENDERER_PROCESS_HANG,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_CHILD_INSTANCE_CREATED,
                 content::NotificationService::AllSources());
  registrar->Add(observer, content::NOTIFICATION_CHILD_PROCESS_CRASHED,
                 content::NotificationService::AllSources());
  registrar->Add(observer, chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED,
                 content::NotificationService::AllSources());
  registrar->Add(observer, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                 content::NotificationService::AllSources());
  registrar->Add(observer, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

void MetricsService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(log_manager_.current_log());
  DCHECK(IsSingleThreaded());

  if (!CanLogNotification(type, source, details))
    return;

  switch (type) {
    case content::NOTIFICATION_USER_ACTION:
         log_manager_.current_log()->RecordUserAction(
             *content::Details<const char*>(details).ptr());
      break;

    case chrome::NOTIFICATION_BROWSER_OPENED:
    case chrome::NOTIFICATION_BROWSER_CLOSED:
      LogWindowChange(type, source, details);
      break;

    case chrome::NOTIFICATION_TAB_PARENTED:
    case chrome::NOTIFICATION_TAB_CLOSING:
      LogWindowChange(type, source, details);
      break;

    case content::NOTIFICATION_LOAD_STOP:
      LogLoadComplete(type, source, details);
      break;

    case content::NOTIFICATION_LOAD_START:
      LogLoadStarted();
      break;

    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
        content::RenderProcessHost::RendererClosedDetails* process_details =
            content::Details<
                content::RenderProcessHost::RendererClosedDetails>(
                    details).ptr();
        content::RenderProcessHost* host =
            content::Source<content::RenderProcessHost>(source).ptr();
        LogRendererCrash(
            host, process_details->status, process_details->was_alive);
      }
      break;

    case content::NOTIFICATION_RENDERER_PROCESS_HANG:
      LogRendererHang();
      break;

    case content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED:
    case content::NOTIFICATION_CHILD_PROCESS_CRASHED:
    case content::NOTIFICATION_CHILD_INSTANCE_CREATED:
      LogChildProcessChange(type, source, details);
      break;

    case chrome::NOTIFICATION_TEMPLATE_URL_SERVICE_LOADED:
      LogKeywordCount(content::Source<TemplateURLService>(
          source)->GetTemplateURLs().size());
      break;

    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL: {
      MetricsLog* current_log =
          static_cast<MetricsLog*>(log_manager_.current_log());
      DCHECK(current_log);
      current_log->RecordOmniboxOpenedURL(
          *content::Details<AutocompleteLog>(details).ptr());
      break;
    }

    case chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED: {
      Profile* p = content::Source<Profile>(source).ptr();
      if (p)
        LogBookmarks(p->GetBookmarkModel());
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  HandleIdleSinceLastTransmission(false);

  if (log_manager_.current_log())
    DVLOG(1) << "METRICS: NUMBER OF EVENTS = "
             << log_manager_.current_log()->num_events();
}

void MetricsService::HandleIdleSinceLastTransmission(bool in_idle) {
  // If there wasn't a lot of action, maybe the computer was asleep, in which
  // case, the log transmissions should have stopped.  Here we start them up
  // again.
  if (!in_idle && idle_since_last_transmission_)
    StartSchedulerIfNecessary();
  idle_since_last_transmission_ = in_idle;
}

void MetricsService::RecordStartOfSessionEnd() {
  LogCleanShutdown();
  RecordBooleanPrefValue(prefs::kStabilitySessionEndCompleted, false);
}

void MetricsService::RecordCompletedSessionEnd() {
  LogCleanShutdown();
  RecordBooleanPrefValue(prefs::kStabilitySessionEndCompleted, true);
}

void MetricsService::RecordBreakpadRegistration(bool success) {
  if (!success)
    IncrementPrefValue(prefs::kStabilityBreakpadRegistrationFail);
  else
    IncrementPrefValue(prefs::kStabilityBreakpadRegistrationSuccess);
}

void MetricsService::RecordBreakpadHasDebugger(bool has_debugger) {
  if (!has_debugger)
    IncrementPrefValue(prefs::kStabilityDebuggerNotPresent);
  else
    IncrementPrefValue(prefs::kStabilityDebuggerPresent);
}

//------------------------------------------------------------------------------
// private methods
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Initialization methods

void MetricsService::InitializeMetricsState() {
#if defined(OS_POSIX)
  server_url_xml_ = ASCIIToUTF16(kServerUrlXml);
  server_url_proto_ = ASCIIToUTF16(kServerUrlProto);
  network_stats_server_ = chrome_common_net::kEchoTestServerLocation;
  http_pipelining_test_server_ = chrome_common_net::kPipelineTestServerBaseUrl;
#else
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  server_url_xml_ = dist->GetStatsServerURL();
  server_url_proto_ = ASCIIToUTF16(kServerUrlProto);
  network_stats_server_ = dist->GetNetworkStatsServer();
  http_pipelining_test_server_ = dist->GetHttpPipeliningTestServer();
#endif

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);

  if ((pref->GetInt64(prefs::kStabilityStatsBuildTime)
       != MetricsLog::GetBuildTime()) ||
      (pref->GetString(prefs::kStabilityStatsVersion)
       != MetricsLog::GetVersionString())) {
    // This is a new version, so we don't want to confuse the stats about the
    // old version with info that we upload.
    DiscardOldStabilityStats(pref);
    pref->SetString(prefs::kStabilityStatsVersion,
                    MetricsLog::GetVersionString());
    pref->SetInt64(prefs::kStabilityStatsBuildTime,
                   MetricsLog::GetBuildTime());
  }

  // Update session ID
  session_id_ = pref->GetInteger(prefs::kMetricsSessionID);
  ++session_id_;
  pref->SetInteger(prefs::kMetricsSessionID, session_id_);

  // Stability bookkeeping
  IncrementPrefValue(prefs::kStabilityLaunchCount);

  if (!pref->GetBoolean(prefs::kStabilityExitedCleanly)) {
    IncrementPrefValue(prefs::kStabilityCrashCount);
    // Reset flag, and wait until we call LogNeedForCleanShutdown() before
    // monitoring.
    pref->SetBoolean(prefs::kStabilityExitedCleanly, true);
  }

  if (!pref->GetBoolean(prefs::kStabilitySessionEndCompleted)) {
    IncrementPrefValue(prefs::kStabilityIncompleteSessionEndCount);
    // This is marked false when we get a WM_ENDSESSION.
    pref->SetBoolean(prefs::kStabilitySessionEndCompleted, true);
  }

  // Initialize uptime counters.
  int64 startup_uptime = MetricsLog::GetIncrementalUptime(pref);
  DCHECK_EQ(0, startup_uptime);
  // For backwards compatibility, leave this intact in case Omaha is checking
  // them.  prefs::kStabilityLastTimestampSec may also be useless now.
  // TODO(jar): Delete these if they have no uses.
  pref->SetInt64(prefs::kStabilityLaunchTimeSec, Time::Now().ToTimeT());

  // Bookkeeping for the uninstall metrics.
  IncrementLongPrefsValue(prefs::kUninstallLaunchCount);

  // Save profile metrics.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs) {
    // Remove the current dictionary and store it for use when sending data to
    // server. By removing the value we prune potentially dead profiles
    // (and keys). All valid values are added back once services startup.
    const DictionaryValue* profile_dictionary =
        prefs->GetDictionary(prefs::kProfileMetrics);
    if (profile_dictionary) {
      // Do a deep copy of profile_dictionary since ClearPref will delete it.
      profile_dictionary_.reset(static_cast<DictionaryValue*>(
          profile_dictionary->DeepCopy()));
      prefs->ClearPref(prefs::kProfileMetrics);
    }
  }

  // Get stats on use of command line.
  const CommandLine* command_line(CommandLine::ForCurrentProcess());
  size_t common_commands = 0;
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineDatDirCount", 1);
  }

  if (command_line->HasSwitch(switches::kApp)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineAppModeCount", 1);
  }

  size_t switch_count = command_line->GetSwitches().size();
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineFlagCount", switch_count);
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineUncommonFlagCount",
                           switch_count - common_commands);

  // Kick off the process of saving the state (so the uptime numbers keep
  // getting updated) every n minutes.
  ScheduleNextStateSave();
}

// static
void MetricsService::InitTaskGetHardwareClass(
    base::WeakPtr<MetricsService> self,
    base::MessageLoopProxy* target_loop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string hardware_class;
#if defined(OS_CHROMEOS)
  chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
      "hardware_class", &hardware_class);
#endif  // OS_CHROMEOS

  target_loop->PostTask(FROM_HERE,
      base::Bind(&MetricsService::OnInitTaskGotHardwareClass,
          self, hardware_class));
}

void MetricsService::OnInitTaskGotHardwareClass(
    const std::string& hardware_class) {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);
  hardware_class_ = hardware_class;

  // Start the next part of the init task: loading plugin information.
  PluginService::GetInstance()->GetPlugins(
      base::Bind(&MetricsService::OnInitTaskGotPluginInfo,
          self_ptr_factory_.GetWeakPtr()));
}

void MetricsService::OnInitTaskGotPluginInfo(
    const std::vector<webkit::WebPluginInfo>& plugins) {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);
  plugins_ = plugins;

  // Schedules a task on a blocking pool thread to gather Google Update
  // statistics (requires Registry reads).
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&MetricsService::InitTaskGetGoogleUpdateData,
                 self_ptr_factory_.GetWeakPtr(),
                 MessageLoop::current()->message_loop_proxy()));
}

// static
void MetricsService::InitTaskGetGoogleUpdateData(
    base::WeakPtr<MetricsService> self,
    base::MessageLoopProxy* target_loop) {
  GoogleUpdateMetrics google_update_metrics;

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  const bool system_install = GoogleUpdateSettings::IsSystemInstall();

  google_update_metrics.is_system_install = system_install;
  google_update_metrics.last_started_au =
      GoogleUpdateSettings::GetGoogleUpdateLastStartedAU(system_install);
  google_update_metrics.last_checked =
      GoogleUpdateSettings::GetGoogleUpdateLastChecked(system_install);
  GoogleUpdateSettings::GetUpdateDetailForGoogleUpdate(
      system_install,
      &google_update_metrics.google_update_data);
  GoogleUpdateSettings::GetUpdateDetail(
      system_install,
      &google_update_metrics.product_data);
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)

  target_loop->PostTask(FROM_HERE,
      base::Bind(&MetricsService::OnInitTaskGotGoogleUpdateData,
          self, google_update_metrics));
}

void MetricsService::OnInitTaskGotGoogleUpdateData(
    const GoogleUpdateMetrics& google_update_metrics) {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);

  google_update_metrics_ = google_update_metrics;

  // Start the next part of the init task: fetching performance data.  This will
  // call into |FinishedReceivingProfilerData()| when the task completes.
  chrome_browser_metrics::TrackingSynchronizer::FetchProfilerDataAsynchronously(
      self_ptr_factory_.GetWeakPtr());
}

void MetricsService::ReceivedProfilerData(
    const tracked_objects::ProcessDataSnapshot& process_data,
    content::ProcessType process_type) {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);

  // Upon the first callback, create the initial log so that we can immediately
  // save the profiler data.
  if (!initial_log_.get())
    initial_log_.reset(new MetricsLog(client_id_, session_id_));

  initial_log_->RecordProfilerData(process_data, process_type);
}

void MetricsService::FinishedReceivingProfilerData() {
  DCHECK_EQ(INIT_TASK_SCHEDULED, state_);
    state_ = INIT_TASK_DONE;
}

std::string MetricsService::GenerateClientID() {
  return guid::GenerateGUID();
}

//------------------------------------------------------------------------------
// State save methods

void MetricsService::ScheduleNextStateSave() {
  state_saver_factory_.InvalidateWeakPtrs();

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&MetricsService::SaveLocalState,
                 state_saver_factory_.GetWeakPtr()),
      base::TimeDelta::FromMinutes(kSaveStateIntervalMinutes));
}

void MetricsService::SaveLocalState() {
  PrefService* pref = g_browser_process->local_state();
  if (!pref) {
    NOTREACHED();
    return;
  }

  RecordCurrentState(pref);

  // TODO(jar):110021 Does this run down the batteries????
  ScheduleNextStateSave();
}


//------------------------------------------------------------------------------
// Recording control methods

void MetricsService::StartRecording() {
  if (log_manager_.current_log())
    return;

  log_manager_.BeginLoggingWithLog(new MetricsLog(client_id_, session_id_),
                                   MetricsLogManager::ONGOING_LOG);
  if (state_ == INITIALIZED) {
    // We only need to schedule that run once.
    state_ = INIT_TASK_SCHEDULED;

    // Schedules a task on the file thread for execution of slower
    // initialization steps (such as plugin list generation) necessary
    // for sending the initial log.  This avoids blocking the main UI
    // thread.
    BrowserThread::PostDelayedTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&MetricsService::InitTaskGetHardwareClass,
            self_ptr_factory_.GetWeakPtr(),
            MessageLoop::current()->message_loop_proxy()),
        base::TimeDelta::FromSeconds(kInitializationDelaySeconds));
  }
}

void MetricsService::StopRecording() {
  if (!log_manager_.current_log())
    return;

  // TODO(jar): Integrate bounds on log recording more consistently, so that we
  // can stop recording logs that are too big much sooner.
  if (log_manager_.current_log()->num_events() > kEventLimit) {
    UMA_HISTOGRAM_COUNTS("UMA.Discarded Log Events",
                         log_manager_.current_log()->num_events());
    log_manager_.DiscardCurrentLog();
    StartRecording();  // Start trivial log to hold our histograms.
  }

  // Adds to ongoing logs.
  log_manager_.current_log()->set_hardware_class(hardware_class_);

  // Put incremental data (histogram deltas, and realtime stats deltas) at the
  // end of all log transmissions (initial log handles this separately).
  // RecordIncrementalStabilityElements only exists on the derived
  // MetricsLog class.
  MetricsLog* current_log =
      static_cast<MetricsLog*>(log_manager_.current_log());
  DCHECK(current_log);
  current_log->RecordEnvironmentProto(plugins_, google_update_metrics_);
  current_log->RecordIncrementalStabilityElements(plugins_);
  RecordCurrentHistograms();

  log_manager_.FinishCurrentLog();
}

void MetricsService::PushPendingLogsToPersistentStorage() {
  if (state_ < INITIAL_LOG_READY)
    return;  // We didn't and still don't have time to get plugin list etc.

  if (log_manager_.has_staged_log()) {
    // We may race here, and send second copy of initial log later.
    if (state_ == INITIAL_LOG_READY)
      state_ = SENDING_OLD_LOGS;
    MetricsLogManager::StoreType store_type = current_fetch_xml_.get() ?
        MetricsLogManager::PROVISIONAL_STORE : MetricsLogManager::NORMAL_STORE;
    log_manager_.StoreStagedLogAsUnsent(store_type);
  }
  DCHECK(!log_manager_.has_staged_log());
  StopRecording();
  StoreUnsentLogs();
}

//------------------------------------------------------------------------------
// Transmission of logs methods

void MetricsService::StartSchedulerIfNecessary() {
  if (reporting_active() && recording_active())
    scheduler_->Start();
}

void MetricsService::StartScheduledUpload() {
  // If reporting has been turned off, the scheduler doesn't need to run.
  if (!reporting_active() || !recording_active()) {
    scheduler_->Stop();
    scheduler_->UploadCancelled();
    return;
  }

  StartFinalLogInfoCollection();
}

void MetricsService::StartFinalLogInfoCollection() {
  // Begin the multi-step process of collecting memory usage histograms:
  // First spawn a task to collect the memory details; when that task is
  // finished, it will call OnMemoryDetailCollectionDone. That will in turn
  // call HistogramSynchronization to collect histograms from all renderers and
  // then call OnHistogramSynchronizationDone to continue processing.
  DCHECK(!waiting_for_asynchronus_reporting_step_);
  waiting_for_asynchronus_reporting_step_ = true;

  base::Closure callback =
      base::Bind(&MetricsService::OnMemoryDetailCollectionDone,
                 self_ptr_factory_.GetWeakPtr());

  scoped_refptr<MetricsMemoryDetails> details(
      new MetricsMemoryDetails(callback));
  details->StartFetch(MemoryDetails::UPDATE_USER_METRICS);

  // Collect WebCore cache information to put into a histogram.
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    i.GetCurrentValue()->Send(new ChromeViewMsg_GetCacheResourceStats());
}

void MetricsService::OnMemoryDetailCollectionDone() {
  DCHECK(IsSingleThreaded());
  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_asynchronus_reporting_step_);

  // Create a callback_task for OnHistogramSynchronizationDone.
  base::Closure callback = base::Bind(
      &MetricsService::OnHistogramSynchronizationDone,
      self_ptr_factory_.GetWeakPtr());

  base::StatisticsRecorder::CollectHistogramStats("Browser");

  // Set up the callback to task to call after we receive histograms from all
  // renderer processes. Wait time specifies how long to wait before absolutely
  // calling us back on the task.
  HistogramSynchronizer::FetchRendererHistogramsAsynchronously(
      MessageLoop::current(), callback,
      base::TimeDelta::FromMilliseconds(kMaxHistogramGatheringWaitDuration));
}

void MetricsService::OnHistogramSynchronizationDone() {
  DCHECK(IsSingleThreaded());
  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_asynchronus_reporting_step_);

  waiting_for_asynchronus_reporting_step_ = false;
  OnFinalLogInfoCollectionDone();
}

void MetricsService::OnFinalLogInfoCollectionDone() {
  // If somehow there is a fetch in progress, we return and hope things work
  // out. The scheduler isn't informed since if this happens, the scheduler
  // will get a response from the upload.
  DCHECK(!current_fetch_xml_.get());
  DCHECK(!current_fetch_proto_.get());
  if (current_fetch_xml_.get() || current_fetch_proto_.get())
    return;

  // If we're getting no notifications, then the log won't have much in it, and
  // it's possible the computer is about to go to sleep, so don't upload and
  // stop the scheduler.
  // Similarly, if logs should no longer be uploaded, stop here.
  if (idle_since_last_transmission_ ||
      !recording_active() || !reporting_active()) {
    scheduler_->Stop();
    scheduler_->UploadCancelled();
    return;
  }

  MakeStagedLog();

  // MakeStagedLog should have prepared log text; if it didn't, skip this
  // upload and hope things work out next time.
  if (log_manager_.staged_log_text().empty()) {
    scheduler_->UploadCancelled();
    return;
  }

  SendStagedLog();
}

void MetricsService::MakeStagedLog() {
  if (log_manager_.has_staged_log())
    return;

  switch (state_) {
    case INITIALIZED:
    case INIT_TASK_SCHEDULED:  // We should be further along by now.
      DCHECK(false);
      return;

    case INIT_TASK_DONE:
      // We need to wait for the initial log to be ready before sending
      // anything, because the server will tell us whether it wants to hear
      // from us.
      PrepareInitialLog();
      DCHECK_EQ(INIT_TASK_DONE, state_);
      log_manager_.LoadPersistedUnsentLogs();
      state_ = INITIAL_LOG_READY;
      break;

    case SENDING_OLD_LOGS:
      if (log_manager_.has_unsent_logs()) {
        log_manager_.StageNextLogForUpload();
        break;
      }
      state_ = SENDING_CURRENT_LOGS;
      // Fall through.

    case SENDING_CURRENT_LOGS:
      StopRecording();
      StartRecording();
      log_manager_.StageNextLogForUpload();
      break;

    default:
      NOTREACHED();
      return;
  }

  DCHECK(log_manager_.has_staged_log());
}

void MetricsService::PrepareInitialLog() {
  DCHECK_EQ(INIT_TASK_DONE, state_);

  DCHECK(initial_log_.get());
  initial_log_->set_hardware_class(hardware_class_);
  initial_log_->RecordEnvironment(plugins_, google_update_metrics_,
                                  profile_dictionary_.get());

  // Histograms only get written to the current log, so make the new log current
  // before writing them.
  log_manager_.PauseCurrentLog();
  log_manager_.BeginLoggingWithLog(initial_log_.release(),
                                   MetricsLogManager::INITIAL_LOG);
  RecordCurrentHistograms();
  log_manager_.FinishCurrentLog();
  log_manager_.ResumePausedLog();

  DCHECK(!log_manager_.has_staged_log());
  log_manager_.StageNextLogForUpload();
}

void MetricsService::StoreUnsentLogs() {
  if (state_ < INITIAL_LOG_READY)
    return;  // We never Recalled the prior unsent logs.

  log_manager_.PersistUnsentLogs();
}

void MetricsService::SendStagedLog() {
  DCHECK(log_manager_.has_staged_log());

  PrepareFetchWithStagedLog();

  if (!current_fetch_xml_.get()) {
    DCHECK(!current_fetch_proto_.get());
    // Compression failed, and log discarded :-/.
    log_manager_.DiscardStagedLog();
    scheduler_->UploadCancelled();
    // TODO(jar): If compression failed, we should have created a tiny log and
    // compressed that, so that we can signal that we're losing logs.
    return;
  }
  // Currently, the staged log for the protobuf version of the data is discarded
  // after we create the URL request, so that there is no chance for
  // re-transmission in case the corresponding XML request fails.  We will
  // handle protobuf failures more carefully once that becomes the main
  // pipeline, i.e. once we switch away from the XML pipeline.
  DCHECK(current_fetch_proto_.get() || !log_manager_.has_staged_log_proto());

  DCHECK(!waiting_for_asynchronus_reporting_step_);

  waiting_for_asynchronus_reporting_step_ = true;
  current_fetch_xml_->Start();
  if (current_fetch_proto_.get())
    current_fetch_proto_->Start();

  HandleIdleSinceLastTransmission(true);
}

void MetricsService::PrepareFetchWithStagedLog() {
  DCHECK(!log_manager_.staged_log_text().empty());

  // Prepare the XML version.
  DCHECK(!current_fetch_xml_.get());
  current_fetch_xml_.reset(content::URLFetcher::Create(
      GURL(server_url_xml_), content::URLFetcher::POST, this));
  current_fetch_xml_->SetRequestContext(
      g_browser_process->system_request_context());
  current_fetch_xml_->SetUploadData(kMimeTypeXml,
                                    log_manager_.staged_log_text().xml);
  // We already drop cookies server-side, but we might as well strip them out
  // client-side as well.
  current_fetch_xml_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                                   net::LOAD_DO_NOT_SEND_COOKIES);

  // Prepare the protobuf version.
  DCHECK(!current_fetch_proto_.get());
  if (log_manager_.has_staged_log_proto()) {
    current_fetch_proto_.reset(content::URLFetcher::Create(
        GURL(server_url_proto_), content::URLFetcher::POST, this));
    current_fetch_proto_->SetRequestContext(
        g_browser_process->system_request_context());
    current_fetch_proto_->SetUploadData(kMimeTypeProto,
                                        log_manager_.staged_log_text().proto);
    // We already drop cookies server-side, but we might as well strip them out
    // client-side as well.
    current_fetch_proto_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                                       net::LOAD_DO_NOT_SEND_COOKIES);

    // Discard the protobuf version of the staged log, so that we will avoid
    // re-uploading it even if we need to re-upload the XML version.
    // TODO(isherman): Handle protobuf upload failures more gracefully once we
    // transition away from the XML-based pipeline.
    log_manager_.DiscardStagedLogProto();
  }
}

static const char* StatusToString(const net::URLRequestStatus& status) {
  switch (status.status()) {
    case net::URLRequestStatus::SUCCESS:
      return "SUCCESS";

    case net::URLRequestStatus::IO_PENDING:
      return "IO_PENDING";

    case net::URLRequestStatus::HANDLED_EXTERNALLY:
      return "HANDLED_EXTERNALLY";

    case net::URLRequestStatus::CANCELED:
      return "CANCELED";

    case net::URLRequestStatus::FAILED:
      return "FAILED";

    default:
      NOTREACHED();
      return "Unknown";
  }
}

// We need to wait for two responses: the response to the XML upload, and the
// response to the protobuf upload.  For now, only the XML upload's response
// affects decisions like whether to retry the upload, whether to abandon the
// upload because it is too large, etc.  However, we still need to wait for the
// protobuf upload, as we cannot reset |current_fetch_proto_| until we have
// confirmation that the network request was sent; and the easiest way to do
// that is to wait for the response.  In case the XML upload's response arrives
// first, we cache that response until the protobuf upload's response also
// arrives.
//
// Note that if the XML upload succeeds but the protobuf upload fails, we will
// not retry the protobuf upload.  If the XML upload fails while the protobuf
// upload succeeds, we will still avoid re-uploading the protobuf data because
// we "zap" the data after the first upload attempt.  This means that we might
// lose protobuf uploads when XML ones succeed; but we will never duplicate any
// protobuf uploads.  Protobuf failures should be rare enough to where this
// should be ok while we have the two pipelines running in parallel.
void MetricsService::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(waiting_for_asynchronus_reporting_step_);

  // We're not allowed to re-use the existing |URLFetcher|s, so free them here.
  // Note however that |source| is aliased to one of these, so we should be
  // careful not to delete it too early.
  scoped_ptr<content::URLFetcher> s;
  if (source == current_fetch_xml_.get()) {
    s.reset(current_fetch_xml_.release());

    // Cache the XML responses, in case we still need to wait for the protobuf
    // response.
    response_code_ = source->GetResponseCode();
    response_status_ = StatusToString(source->GetStatus());
    source->GetResponseAsString(&response_data_);

    // Log a histogram to track response success vs. failure rates.
    UMA_HISTOGRAM_ENUMERATION("UMA.UploadResponseStatus.XML",
                              ResponseCodeToStatus(response_code_),
                              NUM_RESPONSE_STATUSES);
  } else if (source == current_fetch_proto_.get()) {
    s.reset(current_fetch_proto_.release());

    // Log a histogram to track response success vs. failure rates.
    UMA_HISTOGRAM_ENUMERATION("UMA.UploadResponseStatus.Protobuf",
                              ResponseCodeToStatus(source->GetResponseCode()),
                              NUM_RESPONSE_STATUSES);
  } else {
    NOTREACHED();
    return;
  }

  // If we're still waiting for one of the responses, keep waiting...
  if (current_fetch_xml_.get() || current_fetch_proto_.get())
    return;

  // We should only be able to reach here once we've received responses to both
  // the XML and the protobuf requests.  We should always have the response code
  // available.
  DCHECK_NE(response_code_, kNoResponseCode);
  waiting_for_asynchronus_reporting_step_ = false;

  // If the upload was provisionally stored, drop it now that the upload is
  // known to have gone through.
  log_manager_.DiscardLastProvisionalStore();

  // Confirm send so that we can move on.
  VLOG(1) << "Metrics response code: " << response_code_
          << " status=" << response_status_;

  bool upload_succeeded = response_code_ == 200;

  // Provide boolean for error recovery (allow us to ignore response_code).
  bool discard_log = false;

  if (!upload_succeeded &&
      log_manager_.staged_log_text().xml.length() >
          kUploadLogAvoidRetransmitSize) {
    UMA_HISTOGRAM_COUNTS(
        "UMA.Large Rejected Log was Discarded",
        static_cast<int>(log_manager_.staged_log_text().xml.length()));
    discard_log = true;
  } else if (response_code_ == 400) {
    // Bad syntax.  Retransmission won't work.
    discard_log = true;
  }

  if (!upload_succeeded && !discard_log) {
    VLOG(1) << "Metrics: transmission attempt returned a failure code: "
            << response_code_ << ". Verify network connectivity";
    LogBadResponseCode();
  } else {  // Successful receipt (or we are discarding log).
    VLOG(1) << "Metrics response data: " << response_data_;
    switch (state_) {
      case INITIAL_LOG_READY:
        state_ = SENDING_OLD_LOGS;
        break;

      case SENDING_OLD_LOGS:
        // Store the updated list to disk now that the removed log is uploaded.
        StoreUnsentLogs();
        break;

      case SENDING_CURRENT_LOGS:
        break;

      default:
        NOTREACHED();
        break;
    }

    log_manager_.DiscardStagedLog();

    if (log_manager_.has_unsent_logs())
      DCHECK_LT(state_, SENDING_CURRENT_LOGS);
  }

  // Error 400 indicates a problem with the log, not with the server, so
  // don't consider that a sign that the server is in trouble.
  bool server_is_healthy = upload_succeeded || response_code_ == 400;

  scheduler_->UploadFinished(server_is_healthy,
                             log_manager_.has_unsent_logs());

  // Collect network stats if UMA upload succeeded.
  IOThread* io_thread = g_browser_process->io_thread();
  if (server_is_healthy && io_thread) {
    chrome_browser_net::CollectNetworkStats(network_stats_server_, io_thread);
    chrome_browser_net::CollectPipeliningCapabilityStatsOnUIThread(
        http_pipelining_test_server_, io_thread);
  }

  // Reset the cached response data.
  response_code_ = kNoResponseCode;
  response_data_ = std::string();
  response_status_ = std::string();
}

void MetricsService::LogBadResponseCode() {
  VLOG(1) << "Verify your metrics logs are formatted correctly.  Verify server "
             "is active at " << server_url_xml_;
  if (!log_manager_.has_staged_log()) {
    VLOG(1) << "METRICS: Recorder shutdown during log transmission.";
  } else {
    VLOG(1) << "METRICS: transmission retry being scheduled for "
            << log_manager_.staged_log_text().xml;
  }
}

void MetricsService::LogWindowChange(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  int controller_id = -1;
  uintptr_t window_or_tab = source.map_key();
  MetricsLog::WindowEventType window_type;

  // Note: since we stop all logging when a single OTR session is active, it is
  // possible that we start getting notifications about a window that we don't
  // know about.
  if (window_map_.find(window_or_tab) == window_map_.end()) {
    controller_id = next_window_id_++;
    window_map_[window_or_tab] = controller_id;
  } else {
    controller_id = window_map_[window_or_tab];
  }
  DCHECK_NE(controller_id, -1);

  switch (type) {
    case chrome::NOTIFICATION_TAB_PARENTED:
    case chrome::NOTIFICATION_BROWSER_OPENED:
      window_type = MetricsLog::WINDOW_CREATE;
      break;

    case chrome::NOTIFICATION_TAB_CLOSING:
    case chrome::NOTIFICATION_BROWSER_CLOSED:
      window_map_.erase(window_map_.find(window_or_tab));
      window_type = MetricsLog::WINDOW_DESTROY;
      break;

    default:
      NOTREACHED();
      return;
  }

  // TODO(brettw) we should have some kind of ID for the parent.
  log_manager_.current_log()->RecordWindowEvent(window_type, controller_id, 0);
}

void MetricsService::LogLoadComplete(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (details == content::NotificationService::NoDetails())
    return;

  // TODO(jar): There is a bug causing this to be called too many times, and
  // the log overflows.  For now, we won't record these events.
  UMA_HISTOGRAM_COUNTS("UMA.LogLoadComplete called", 1);
  return;

  const content::Details<LoadNotificationDetails> load_details(details);
  int controller_id = window_map_[details.map_key()];
  log_manager_.current_log()->RecordLoadEvent(controller_id,
                                              load_details->url,
                                              load_details->origin,
                                              load_details->session_index,
                                              load_details->load_time);
}

void MetricsService::IncrementPrefValue(const char* path) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int value = pref->GetInteger(path);
  pref->SetInteger(path, value + 1);
}

void MetricsService::IncrementLongPrefsValue(const char* path) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  int64 value = pref->GetInt64(path);
  pref->SetInt64(path, value + 1);
}

void MetricsService::LogLoadStarted() {
  HISTOGRAM_ENUMERATION("Chrome.UmaPageloadCounter", 1, 2);
  IncrementPrefValue(prefs::kStabilityPageLoadCount);
  IncrementLongPrefsValue(prefs::kUninstallMetricsPageLoadCount);
  // We need to save the prefs, as page load count is a critical stat, and it
  // might be lost due to a crash :-(.
}

void MetricsService::LogRendererCrash(content::RenderProcessHost* host,
                                      base::TerminationStatus status,
                                      bool was_alive) {
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  ExtensionService* service = profile->GetExtensionService();
  bool was_extension_process =
      service && service->process_map()->Contains(host->GetID());
  if (status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
      status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
    if (was_extension_process)
      IncrementPrefValue(prefs::kStabilityExtensionRendererCrashCount);
    else
      IncrementPrefValue(prefs::kStabilityRendererCrashCount);

    UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.ChildCrashes",
                             was_extension_process ? 2 : 1);
    if (was_alive) {
      UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.ChildCrashesWasAlive",
                               was_extension_process ? 2 : 1);
    }
  } else if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.ChildKills",
                             was_extension_process ? 2 : 1);
    if (was_alive) {
      UMA_HISTOGRAM_PERCENTAGE("BrowserRenderProcessHost.ChildKillsWasAlive",
                               was_extension_process ? 2 : 1);
    }
  }
}

void MetricsService::LogRendererHang() {
  IncrementPrefValue(prefs::kStabilityRendererHangCount);
}

void MetricsService::LogNeedForCleanShutdown() {
  PrefService* pref = g_browser_process->local_state();
  pref->SetBoolean(prefs::kStabilityExitedCleanly, false);
  // Redundant setting to be sure we call for a clean shutdown.
  clean_shutdown_status_ = NEED_TO_SHUTDOWN;
}

bool MetricsService::UmaMetricsProperlyShutdown() {
  CHECK(clean_shutdown_status_ == CLEANLY_SHUTDOWN ||
        clean_shutdown_status_ == NEED_TO_SHUTDOWN);
  return clean_shutdown_status_ == CLEANLY_SHUTDOWN;
}

// For use in hack in LogCleanShutdown.
static void Signal(base::WaitableEvent* event) {
  event->Signal();
}

void MetricsService::LogCleanShutdown() {
  // Redundant hack to write pref ASAP.
  PrefService* pref = g_browser_process->local_state();
  pref->SetBoolean(prefs::kStabilityExitedCleanly, true);
  pref->CommitPendingWrite();
  // Hack: TBD: Remove this wait.
  // We are so concerned that the pref gets written, we are now willing to stall
  // the UI thread until we get assurance that a pref-writing task has
  // completed.
  base::WaitableEvent done_writing(false, false);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(Signal, &done_writing));
  // http://crbug.com/124954
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  done_writing.TimedWait(base::TimeDelta::FromHours(1));

  // Redundant setting to assure that we always reset this value at shutdown
  // (and that we don't use some alternate path, and not call LogCleanShutdown).
  clean_shutdown_status_ = CLEANLY_SHUTDOWN;

  RecordBooleanPrefValue(prefs::kStabilityExitedCleanly, true);
}

#if defined(OS_CHROMEOS)
void MetricsService::LogChromeOSCrash(const std::string &crash_type) {
  if (crash_type == "user")
    IncrementPrefValue(prefs::kStabilityOtherUserCrashCount);
  else if (crash_type == "kernel")
    IncrementPrefValue(prefs::kStabilityKernelCrashCount);
  else if (crash_type == "uncleanshutdown")
    IncrementPrefValue(prefs::kStabilitySystemUncleanShutdownCount);
  else
    NOTREACHED() << "Unexpected Chrome OS crash type " << crash_type;
  // Wake up metrics logs sending if necessary now that new
  // log data is available.
  HandleIdleSinceLastTransmission(false);
}
#endif  // OS_CHROMEOS

void MetricsService::LogChildProcessChange(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  content::Details<ChildProcessData> child_details(details);
  const string16& child_name = child_details->name;

  if (child_process_stats_buffer_.find(child_name) ==
      child_process_stats_buffer_.end()) {
    child_process_stats_buffer_[child_name] =
        ChildProcessStats(child_details->type);
  }

  ChildProcessStats& stats = child_process_stats_buffer_[child_name];
  switch (type) {
    case content::NOTIFICATION_CHILD_PROCESS_HOST_CONNECTED:
      stats.process_launches++;
      break;

    case content::NOTIFICATION_CHILD_INSTANCE_CREATED:
      stats.instances++;
      break;

    case content::NOTIFICATION_CHILD_PROCESS_CRASHED:
      stats.process_crashes++;
      // Exclude plugin crashes from the count below because we report them via
      // a separate UMA metric.
      if (!IsPluginProcess(child_details->type)) {
        IncrementPrefValue(prefs::kStabilityChildProcessCrashCount);
      }
      break;

    default:
      NOTREACHED() << "Unexpected notification type " << type;
      return;
  }
}

// Recursively counts the number of bookmarks and folders in node.
static void CountBookmarks(const BookmarkNode* node,
                           int* bookmarks,
                           int* folders) {
  if (node->is_url())
    (*bookmarks)++;
  else
    (*folders)++;
  for (int i = 0; i < node->child_count(); ++i)
    CountBookmarks(node->GetChild(i), bookmarks, folders);
}

void MetricsService::LogBookmarks(const BookmarkNode* node,
                                  const char* num_bookmarks_key,
                                  const char* num_folders_key) {
  DCHECK(node);
  int num_bookmarks = 0;
  int num_folders = 0;
  CountBookmarks(node, &num_bookmarks, &num_folders);
  num_folders--;  // Don't include the root folder in the count.

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  pref->SetInteger(num_bookmarks_key, num_bookmarks);
  pref->SetInteger(num_folders_key, num_folders);
}

void MetricsService::LogBookmarks(BookmarkModel* model) {
  DCHECK(model);
  LogBookmarks(model->bookmark_bar_node(),
               prefs::kNumBookmarksOnBookmarkBar,
               prefs::kNumFoldersOnBookmarkBar);
  LogBookmarks(model->other_node(),
               prefs::kNumBookmarksInOtherBookmarkFolder,
               prefs::kNumFoldersInOtherBookmarkFolder);
  ScheduleNextStateSave();
}

void MetricsService::LogKeywordCount(size_t keyword_count) {
  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);
  pref->SetInteger(prefs::kNumKeywords, static_cast<int>(keyword_count));
  ScheduleNextStateSave();
}

void MetricsService::RecordPluginChanges(PrefService* pref) {
  ListPrefUpdate update(pref, prefs::kStabilityPluginStats);
  ListValue* plugins = update.Get();
  DCHECK(plugins);

  for (ListValue::iterator value_iter = plugins->begin();
       value_iter != plugins->end(); ++value_iter) {
    if (!(*value_iter)->IsType(Value::TYPE_DICTIONARY)) {
      NOTREACHED();
      continue;
    }

    DictionaryValue* plugin_dict = static_cast<DictionaryValue*>(*value_iter);
    std::string plugin_name;
    plugin_dict->GetString(prefs::kStabilityPluginName, &plugin_name);
    if (plugin_name.empty()) {
      NOTREACHED();
      continue;
    }

    // TODO(viettrungluu): remove conversions
    string16 name16 = UTF8ToUTF16(plugin_name);
    if (child_process_stats_buffer_.find(name16) ==
        child_process_stats_buffer_.end()) {
      continue;
    }

    ChildProcessStats stats = child_process_stats_buffer_[name16];
    if (stats.process_launches) {
      int launches = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginLaunches, &launches);
      launches += stats.process_launches;
      plugin_dict->SetInteger(prefs::kStabilityPluginLaunches, launches);
    }
    if (stats.process_crashes) {
      int crashes = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginCrashes, &crashes);
      crashes += stats.process_crashes;
      plugin_dict->SetInteger(prefs::kStabilityPluginCrashes, crashes);
    }
    if (stats.instances) {
      int instances = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginInstances, &instances);
      instances += stats.instances;
      plugin_dict->SetInteger(prefs::kStabilityPluginInstances, instances);
    }

    child_process_stats_buffer_.erase(name16);
  }

  // Now go through and add dictionaries for plugins that didn't already have
  // reports in Local State.
  for (std::map<string16, ChildProcessStats>::iterator cache_iter =
           child_process_stats_buffer_.begin();
       cache_iter != child_process_stats_buffer_.end(); ++cache_iter) {
    ChildProcessStats stats = cache_iter->second;

    // Insert only plugins information into the plugins list.
    if (!IsPluginProcess(stats.process_type))
      continue;

    // TODO(viettrungluu): remove conversion
    std::string plugin_name = UTF16ToUTF8(cache_iter->first);

    DictionaryValue* plugin_dict = new DictionaryValue;

    plugin_dict->SetString(prefs::kStabilityPluginName, plugin_name);
    plugin_dict->SetInteger(prefs::kStabilityPluginLaunches,
                            stats.process_launches);
    plugin_dict->SetInteger(prefs::kStabilityPluginCrashes,
                            stats.process_crashes);
    plugin_dict->SetInteger(prefs::kStabilityPluginInstances,
                            stats.instances);
    plugins->Append(plugin_dict);
  }
  child_process_stats_buffer_.clear();
}

bool MetricsService::CanLogNotification(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // We simply don't log anything to UMA if there is a single incognito
  // session visible. The problem is that we always notify using the orginal
  // profile in order to simplify notification processing.
  return !BrowserList::IsOffTheRecordSessionActive();
}

void MetricsService::RecordBooleanPrefValue(const char* path, bool value) {
  DCHECK(IsSingleThreaded());

  PrefService* pref = g_browser_process->local_state();
  DCHECK(pref);

  pref->SetBoolean(path, value);
  RecordCurrentState(pref);
}

void MetricsService::RecordCurrentState(PrefService* pref) {
  pref->SetInt64(prefs::kStabilityLastTimestampSec, Time::Now().ToTimeT());

  RecordPluginChanges(pref);
}

// static
bool MetricsService::IsPluginProcess(content::ProcessType type) {
  return (type == content::PROCESS_TYPE_PLUGIN||
          type == content::PROCESS_TYPE_PPAPI_PLUGIN);
}

#if defined(OS_CHROMEOS)
void MetricsService::StartExternalMetrics() {
  external_metrics_ = new chromeos::ExternalMetrics;
  external_metrics_->Start();
}
#endif

// static
bool MetricsServiceHelper::IsMetricsReportingEnabled() {
  bool result = false;
  const PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    const PrefService::Preference* uma_pref =
        local_state->FindPreference(prefs::kMetricsReportingEnabled);
    if (uma_pref) {
      bool success = uma_pref->GetValue()->GetAsBoolean(&result);
      DCHECK(success);
    }
  }
  return result;
}
