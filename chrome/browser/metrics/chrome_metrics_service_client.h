// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/memory_details.h"
#include "chrome/browser/metrics/network_stats_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/profiler/tracking_synchronizer_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ChromeOSMetricsProvider;
class GoogleUpdateMetricsProviderWin;
class PluginMetricsProvider;
class PrefRegistrySimple;

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
class SigninStatusMetricsProvider;
#endif

namespace base {
class FilePath;
}

namespace metrics {
class MetricsService;
class MetricsStateManager;
class ProfilerMetricsProvider;
}

// ChromeMetricsServiceClient provides an implementation of MetricsServiceClient
// that depends on chrome/.
class ChromeMetricsServiceClient
    : public metrics::MetricsServiceClient,
      public metrics::TrackingSynchronizerObserver,
      public content::NotificationObserver {
 public:
  virtual ~ChromeMetricsServiceClient();

  // Factory function.
  static scoped_ptr<ChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager,
      PrefService* local_state);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsServiceClient:
  virtual void SetMetricsClientId(const std::string& client_id) OVERRIDE;
  virtual bool IsOffTheRecordSessionActive() OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual bool GetBrand(std::string* brand_code) OVERRIDE;
  virtual metrics::SystemProfileProto::Channel GetChannel() OVERRIDE;
  virtual std::string GetVersionString() OVERRIDE;
  virtual void OnLogUploadComplete() OVERRIDE;
  virtual void StartGatheringMetrics(
      const base::Closure& done_callback) OVERRIDE;
  virtual void CollectFinalMetrics(const base::Closure& done_callback)
      OVERRIDE;
  virtual scoped_ptr<metrics::MetricsLogUploader> CreateUploader(
      const std::string& server_url,
      const std::string& mime_type,
      const base::Callback<void(int)>& on_upload_complete) OVERRIDE;
  virtual base::string16 GetRegistryBackupKey() OVERRIDE;

  metrics::MetricsService* metrics_service() { return metrics_service_.get(); }

  void LogPluginLoadingError(const base::FilePath& plugin_path);

 private:
  explicit ChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager);

  // Completes the two-phase initialization of ChromeMetricsServiceClient.
  void Initialize();

  // Callback that continues the init task by loading plugin information.
  void OnInitTaskGotHardwareClass();

  // Called after the Plugin init task has been completed that continues the
  // init task by launching a task to gather Google Update statistics.
  void OnInitTaskGotPluginInfo();

  // Called after GoogleUpdate init task has been completed that continues the
  // init task by loading profiler data.
  void OnInitTaskGotGoogleUpdateData();

  // TrackingSynchronizerObserver:
  virtual void ReceivedProfilerData(
      const tracked_objects::ProcessDataSnapshot& process_data,
      int process_type) OVERRIDE;
  virtual void FinishedReceivingProfilerData() OVERRIDE;

  // Callbacks for various stages of final log info collection. Do not call
  // these directly.
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
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

#if defined(OS_WIN)
  // Counts (and removes) the browser crash dump attempt signals left behind by
  // any previous browser processes which generated a crash dump.
  void CountBrowserCrashDumpAttempts();
#endif  // OS_WIN

  base::ThreadChecker thread_checker_;

  // Weak pointer to the MetricsStateManager.
  metrics::MetricsStateManager* metrics_state_manager_;

  // The MetricsService that |this| is a client of.
  scoped_ptr<metrics::MetricsService> metrics_service_;

  content::NotificationRegistrar registrar_;

  // On ChromeOS, holds a weak pointer to the ChromeOSMetricsProvider instance
  // that has been registered with MetricsService. On other platforms, is NULL.
  ChromeOSMetricsProvider* chromeos_metrics_provider_;

  NetworkStatsUploader network_stats_uploader_;

  // Saved callback received from CollectFinalMetrics().
  base::Closure collect_final_metrics_done_callback_;

  // Indicates that collect final metrics step is running.
  bool waiting_for_collect_final_metrics_step_;

  // Number of async histogram fetch requests in progress.
  int num_async_histogram_fetches_in_progress_;

  // The ProfilerMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  metrics::ProfilerMetricsProvider* profiler_metrics_provider_;

#if defined(ENABLE_PLUGINS)
  // The PluginMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  PluginMetricsProvider* plugin_metrics_provider_;
#endif

#if defined(OS_WIN)
  // The GoogleUpdateMetricsProviderWin instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  GoogleUpdateMetricsProviderWin* google_update_metrics_provider_;
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
  // The SigninStatusMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  SigninStatusMetricsProvider* signin_status_metrics_provider_;
#endif

  // Callback that is called when initial metrics gathering is complete.
  base::Closure finished_gathering_initial_metrics_callback_;

  // The MemoryGrowthTracker instance that tracks memory usage growth in
  // MemoryDetails.
  MemoryGrowthTracker memory_growth_tracker_;

  base::WeakPtrFactory<ChromeMetricsServiceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMetricsServiceClient);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_CLIENT_H_
