// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_CLIENT_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/profiler/tracking_synchronizer_observer.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/ukm/observers/history_delete_observer.h"
#include "components/ukm/observers/sync_disable_observer.h"
#include "ios/web/public/web_state/global_web_state_observer.h"

class IOSChromeStabilityMetricsProvider;
class PrefRegistrySimple;

namespace ios {
class ChromeBrowserState;
}

namespace metrics {
class DriveMetricsProvider;
class MetricsService;
class MetricsStateManager;
class ProfilerMetricsProvider;
}  // namespace metrics

namespace ukm {
class UkmService;
}

// IOSChromeMetricsServiceClient provides an implementation of
// MetricsServiceClient that depends on //ios/chrome/.
class IOSChromeMetricsServiceClient
    : public metrics::MetricsServiceClient,
      public metrics::TrackingSynchronizerObserver,
      public ukm::HistoryDeleteObserver,
      public ukm::SyncDisableObserver,
      public web::GlobalWebStateObserver {
 public:
  ~IOSChromeMetricsServiceClient() override;

  // Factory function.
  static std::unique_ptr<IOSChromeMetricsServiceClient> Create(
      metrics::MetricsStateManager* state_manager);

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsServiceClient:
  metrics::MetricsService* GetMetricsService() override;
  ukm::UkmService* GetUkmService() override;
  void SetMetricsClientId(const std::string& client_id) override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  metrics::SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void InitializeSystemProfileMetrics(
      const base::Closure& done_callback) override;
  void CollectFinalMetricsForLog(const base::Closure& done_callback) override;
  std::unique_ptr<metrics::MetricsLogUploader> CreateUploader(
      const std::string& server_url,
      const std::string& mime_type,
      metrics::MetricsLogUploader::MetricServiceType service_type,
      const base::Callback<void(int)>& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;
  base::string16 GetRegistryBackupKey() override;
  void OnRendererProcessCrash() override;
  bool IsHistorySyncEnabledOnAllProfiles() override;

  // ukm::HistoryDeleteObserver:
  void OnHistoryDeleted() override;

  // ukm::SyncDisableObserver:
  void OnSyncPrefsChanged(bool must_purge) override;

  // web::GlobalWebStateObserver:
  void WebStateDidStartLoading(web::WebState* web_state) override;
  void WebStateDidStopLoading(web::WebState* web_state) override;

  metrics::EnableMetricsDefault GetMetricsReportingDefaultState() override;

 private:
  explicit IOSChromeMetricsServiceClient(
      metrics::MetricsStateManager* state_manager);

  // Completes the two-phase initialization of IOSChromeMetricsServiceClient.
  void Initialize();

  // Called after the drive metrics init task has been completed that continues
  // the init task by loading profiler data.
  void OnInitTaskGotDriveMetrics();

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

  // Registers |this| as an observer for notifications which indicate that a
  // user is performing work. This is useful to allow some features to sleep,
  // until the machine becomes active, such as precluding UMA uploads unless
  // there was recent activity.
  void RegisterForNotifications();

  // Register to observe events on a browser state's services.
  void RegisterForBrowserStateEvents(ios::ChromeBrowserState* browser_state);

  // Called when a tab is parented.
  void OnTabParented(web::WebState* web_state);

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  base::ThreadChecker thread_checker_;

  // Weak pointer to the MetricsStateManager.
  metrics::MetricsStateManager* metrics_state_manager_;

  // The MetricsService that |this| is a client of.
  std::unique_ptr<metrics::MetricsService> metrics_service_;

  // The UkmService that |this| is a client of.
  std::unique_ptr<ukm::UkmService> ukm_service_;

  // The IOSChromeStabilityMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  IOSChromeStabilityMetricsProvider* stability_metrics_provider_;

  // Saved callback received from CollectFinalMetricsForLog().
  base::Closure collect_final_metrics_done_callback_;

  // The ProfilerMetricsProvider instance that was registered with
  // MetricsService. Has the same lifetime as |metrics_service_|.
  metrics::ProfilerMetricsProvider* profiler_metrics_provider_;

  // The DriveMetricsProvider instance that was registered with MetricsService.
  // Has the same lifetime as |metrics_service_|.
  metrics::DriveMetricsProvider* drive_metrics_provider_;

  // Callback that is called when initial metrics gathering is complete.
  base::Closure finished_init_task_callback_;

  // Time of this object's creation.
  const base::TimeTicks start_time_;

  // Subscription for receiving callbacks that a tab was parented.
  std::unique_ptr<base::CallbackList<void(web::WebState*)>::Subscription>
      tab_parented_subscription_;

  // Subscription for receiving callbacks that a URL was opened from the
  // omnibox.
  std::unique_ptr<base::CallbackList<void(OmniboxLog*)>::Subscription>
      omnibox_url_opened_subscription_;

  // Whether this client has already uploaded profiler data during this session.
  // Profiler data is uploaded at most once per session.
  bool has_uploaded_profiler_data_;

  base::WeakPtrFactory<IOSChromeMetricsServiceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeMetricsServiceClient);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_METRICS_SERVICE_CLIENT_H_
