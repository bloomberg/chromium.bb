// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/profiler/tracking_synchronizer_observer.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ppapi/features/features.h"

class AntiVirusMetricsProvider;
class ChromeOSMetricsProvider;
class GoogleUpdateMetricsProviderWin;
class PluginMetricsProvider;
class PrefRegistrySimple;
class PrefService;

#if !defined(OS_CHROMEOS)
class SigninStatusMetricsProvider;
#endif

namespace browser_watcher {
class WatcherMetricsProviderWin;
}  // namespace browser_watcher

namespace metrics {
class DriveMetricsProvider;
class MetricsService;
class MetricsStateManager;
class ProfilerMetricsProvider;
}  // namespace metrics

// ChromeMetricsServiceClient provides an implementation of MetricsServiceClient
// that depends on chrome/.
class ChromeMetricsServiceClient
    : public metrics::MetricsServiceClient,
      public metrics::TrackingSynchronizerObserver,
      public content::NotificationObserver {
 public:
  ~ChromeMetricsServiceClient() override;

  // Factory function.
  static std::unique_ptr<ChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsServiceClient:
  metrics::MetricsService* GetMetricsService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void OnEnvironmentUpdate(std::string* serialized_environment) override;
  void OnLogUploadComplete() override;
  void OnLogCleanShutdown() override;
  void InitializeSystemProfileMetrics(
      const base::Closure& done_callback) override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const base::Callback<void(int)>& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;
  base::string16 GetRegistryBackupKey() override;
  void OnPluginLoadingError(const base::FilePath& plugin_path) override;
  bool IsReportingPolicyManaged() override;
  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;
  bool IsUMACellularUploadLogicEnabled() override;

  // Persistent browser metrics need to be persisted somewhere. This constant
  // provides a known string to be used for both the allocator's internal name
  // and for a file on disk (relative to chrome::DIR_USER_DATA) to which they
  // can be saved.
  static const char kBrowserMetricsName[];

 private:
  explicit ChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager);

  // Completes the two-phase initialization of ChromeMetricsServiceClient.
  void Initialize();

  // Callback to chain init tasks: Pops and executes the next init task from
  // |initialize_task_queue_|, then passes itself as callback for each init task
  // to call upon completion.
  void OnInitNextTask();

  // Returns true iff profiler data should be included in the next metrics log.
  // NOTE: This method is probabilistic and also updates internal state as a
  // side-effect when called, so it should only be called once per log.
  bool ShouldIncludeProfilerDataInLog();

  // TrackingSynchronizerObserver:
  void ReceivedProfilerData(
      const metrics::ProfilerDataAttributes& attributes,
      const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
      const metrics::ProfilerEvents& past_profiler_events) override;
  void FinishedReceivingProfilerData() override;

  // Callbacks for various stages of final log info collection. Do not call
  // these directly.
  void CollectFinalHistograms();
  void MergeHistogramDeltas();
  void OnMemoryDetailCollectionDone();
  void OnHistogramSynchronizationDone();

  // Records metrics about the switches present on the command line.
  void RecordCommandLineMetrics();

  // Registers |this| as an observer for notifications which indicate that a
  // user is performing work. This is useful to allow some features to sleep,
  // until the machine becomes active, such as precluding UMA uploads unless
  // there was recent activity.
  void RegisterForNotifications();

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

#if defined(OS_WIN)
  // Counts (and removes) the browser crash dump attempt signals left behind by
  // any previous browser processes which generated a crash dump.
  void CountBrowserCrashDumpAttempts();
#endif  // OS_WIN

  base::ThreadChecker thread_checker_;

  // Weak pointer to the MetricsStateManager.
  metrics::MetricsStateManager* metrics_state_manager_;

  // The MetricsService that |this| is a client of.
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  content::NotificationRegistrar registrar_;

#if defined(OS_CHROMEOS)
  // On ChromeOS, holds a weak pointer to the ChromeOSMetricsProvider instance
  // that has been registered with MetricsService. On other platforms, is NULL.
  ChromeOSMetricsProvider* chromeos_metrics_provider_;
#endif

  // A queue of tasks for initial metrics gathering. These may be asynchronous
  // or synchronous.
  std::deque<base::Closure> initialize_task_queue_;

  // Saved callback received from CollectFinalMetricsForLog().
  base::Closure collect_final_metrics_done_callback_;

  // Indicates that collect final metrics step is running.
  bool waiting_for_collect_final_metrics_step_;

  // Number of async histogram fetch requests in progress.
  int num_async_histogram_fetches_in_progress_;

  // The ProfilerMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  metrics::ProfilerMetricsProvider* profiler_metrics_provider_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // The PluginMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  PluginMetricsProvider* plugin_metrics_provider_;
#endif

#if defined(OS_WIN)
  // The GoogleUpdateMetricsProviderWin instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  GoogleUpdateMetricsProviderWin* google_update_metrics_provider_;

  // The WatcherMetricsProviderWin instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  browser_watcher::WatcherMetricsProviderWin* watcher_metrics_provider_;

  // The AntiVirusMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  AntiVirusMetricsProvider* antivirus_metrics_provider_;
#endif

  // The DriveMetricsProvider instance that was registered with MetricsService.
  // Has the same lifetime as |metrics_service_|.
  metrics::DriveMetricsProvider* drive_metrics_provider_;

  // The MemoryGrowthTracker instance that tracks memory usage growth in
  // MemoryDetails.
  MemoryGrowthTracker memory_growth_tracker_;

  // Callback to determine whether or not a cellular network is currently being
  // used.
  base::Callback<void(bool*)> cellular_callback_;

  // Time of this object's creation.
  const base::TimeTicks start_time_;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  std::unique_ptr<base::CallbackList<void(OmniboxLog*)>::Subscription>
      omnibox_url_opened_subscription_;

  // Whether this client has already uploaded profiler data during this session.
  // Profiler data is uploaded at most once per session.
  bool has_uploaded_profiler_data_;

  base::WeakPtrFactory<ChromeMetricsServiceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServiceClient);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
