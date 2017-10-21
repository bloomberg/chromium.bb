// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/ukm/observers/history_delete_observer.h"
#include "components/ukm/observers/sync_disable_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ppapi/features/features.h"
#include "third_party/metrics_proto/system_profile.pb.h"

class PluginMetricsProvider;
class Profile;
class PrefRegistrySimple;

#if defined(OS_ANDROID)
class TabModelListObserver;
#endif  // defined(OS_ANDROID)


namespace metrics {
class MetricsService;
class MetricsStateManager;
}  // namespace metrics

// ChromeMetricsServiceClient provides an implementation of MetricsServiceClient
// that depends on chrome/.
class ChromeMetricsServiceClient : public metrics::MetricsServiceClient,
                                   public content::NotificationObserver,
                                   public ukm::HistoryDeleteObserver,
                                   public ukm::SyncDisableObserver {
 public:
  ~ChromeMetricsServiceClient() override;

  // Factory function.
  static std::unique_ptr<ChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Checks if the user has forced metrics collection on via the override flag.
  static bool IsMetricsReportingForceEnabled();

  // metrics::MetricsServiceClient:
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmService* GetUkmService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void OnEnvironmentUpdate(std::string* serialized_environment) override;
  void OnLogCleanShutdown() override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      base::StringPiece server_url,
      base::StringPiece mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const metrics::MetricsLogUploader::UploadCallback& on_upload_complete)
      override;
  base::TimeDelta GetStandardUploadInterval() override;
  void OnPluginLoadingError(const base::FilePath& plugin_path) override;
  bool IsReportingPolicyManaged() override;
  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;
  bool IsUMACellularUploadLogicEnabled() override;
  bool IsHistorySyncEnabledOnAllProfiles() override;

  // ukm::HistoryDeleteObserver:
  void OnHistoryDeleted() override;

  // ukm::SyncDisableObserver:
  void OnSyncPrefsChanged(bool must_purge) override;

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

  // Registers providers to the MetricsService. These provide data from
  // alternate sources.
  void RegisterMetricsServiceProviders();

  // Registers providers to the UkmService. These provide data from alternate
  // sources.
  void RegisterUKMProviders();

  // Returns true iff profiler data should be included in the next metrics log.
  // NOTE: This method is probabilistic and also updates internal state as a
  // side-effect when called, so it should only be called once per log.
  bool ShouldIncludeProfilerDataInLog();

  // Callbacks for various stages of final log info collection. Do not call
  // these directly.
  void CollectFinalHistograms();
  void OnMemoryDetailCollectionDone();
  void OnHistogramSynchronizationDone();

  // Records metrics about the switches present on the command line.
  void RecordCommandLineMetrics();

  // Registers |this| as an observer for notifications which indicate that a
  // user is performing work. This is useful to allow some features to sleep,
  // until the machine becomes active, such as precluding UMA uploads unless
  // there was recent activity.
  void RegisterForNotifications();

  // Call to listen for events on the selected profile's services.
  void RegisterForProfileEvents(Profile* profile);

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

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer to the MetricsStateManager.
  metrics::MetricsStateManager* metrics_state_manager_;

  // The MetricsService that |this| is a client of.
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // The UkmService that |this| is a client of.
  std::unique_ptr<ukm::UkmService> ukm_service_;

  content::NotificationRegistrar registrar_;

#if defined(OS_ANDROID)
  // Listener for changes in incognito activity.
  // Desktop platform use BrowserList, and can listen for
  // chrome::NOTIFICATION_BROWSER_OPENED instead.
  std::unique_ptr<TabModelListObserver> incognito_observer_;
#endif  // defined(OS_ANDROID)

  // A queue of tasks for initial metrics gathering. These may be asynchronous
  // or synchronous.
  base::circular_deque<base::Closure> initialize_task_queue_;

  // Saved callback received from CollectFinalMetricsForLog().
  base::Closure collect_final_metrics_done_callback_;

  // Indicates that collect final metrics step is running.
  bool waiting_for_collect_final_metrics_step_;

  // Number of async histogram fetch requests in progress.
  int num_async_histogram_fetches_in_progress_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // The PluginMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  PluginMetricsProvider* plugin_metrics_provider_;
#endif

  // The MemoryGrowthTracker instance that tracks memory usage growth in
  // MemoryDetails.
  MemoryGrowthTracker memory_growth_tracker_;

  // Callback to determine whether or not a cellular network is currently being
  // used.
  base::Callback<void(bool*)> cellular_callback_;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  std::unique_ptr<base::CallbackList<void(OmniboxLog*)>::Subscription>
      omnibox_url_opened_subscription_;

  base::WeakPtrFactory<ChromeMetricsServiceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServiceClient);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
