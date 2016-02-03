// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/net/version_utils.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/google/google_brand.h"
#include "ios/chrome/browser/metrics/ios_chrome_stability_metrics_provider.h"
#include "ios/chrome/browser/metrics/mobile_session_shutdown_metrics_provider.h"
#include "ios/chrome/browser/signin/ios_chrome_signin_status_metrics_provider_delegate.h"
#include "ios/chrome/browser/tab_parenting_global_observer.h"
#include "ios/chrome/browser/ui/browser_otr_state.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_thread.h"

namespace {

// Standard interval between log uploads, in seconds.
const int kStandardUploadIntervalSeconds = 5 * 60;           // Five minutes.
const int kStandardUploadIntervalCellularSeconds = 15 * 60;  // Fifteen minutes.

// Returns true if current connection type is cellular and user is assigned to
// experimental group for enabled cellular uploads.
bool IsCellularLogicEnabled() {
  if (variations::GetVariationParamValue("UMA_EnableCellularLogUpload",
                                         "Enabled") != "true" ||
      variations::GetVariationParamValue("UMA_EnableCellularLogUpload",
                                         "Optimize") == "false") {
    return false;
  }

  return net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
}

}  // namespace

IOSChromeMetricsServiceClient::IOSChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
      stability_metrics_provider_(nullptr),
      profiler_metrics_provider_(nullptr),
      drive_metrics_provider_(nullptr),
      start_time_(base::TimeTicks::Now()),
      has_uploaded_profiler_data_(false),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RegisterForNotifications();
}

IOSChromeMetricsServiceClient::~IOSChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
scoped_ptr<IOSChromeMetricsServiceClient> IOSChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager,
    PrefService* local_state) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  scoped_ptr<IOSChromeMetricsServiceClient> client(
      new IOSChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void IOSChromeMetricsServiceClient::RegisterPrefs(
    PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
}

metrics::MetricsService* IOSChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

void IOSChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

void IOSChromeMetricsServiceClient::OnRecordingDisabled() {
  crash_keys::ClearMetricsClientId();
}

bool IOSChromeMetricsServiceClient::IsOffTheRecordSessionActive() {
  return ::IsOffTheRecordSessionActive();
}

int32_t IOSChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string IOSChromeMetricsServiceClient::GetApplicationLocale() {
  return GetApplicationContext()->GetApplicationLocale();
}

bool IOSChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return ios::google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel
IOSChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(::GetChannel());
}

std::string IOSChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void IOSChromeMetricsServiceClient::OnLogUploadComplete() {}

void IOSChromeMetricsServiceClient::InitializeSystemProfileMetrics(
    const base::Closure& done_callback) {
  finished_init_task_callback_ = done_callback;
  drive_metrics_provider_->GetDriveMetrics(
      base::Bind(&IOSChromeMetricsServiceClient::OnInitTaskGotDriveMetrics,
                 weak_ptr_factory_.GetWeakPtr()));
}

void IOSChromeMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = done_callback;

  if (ShouldIncludeProfilerDataInLog()) {
    // Fetch profiler data. This will call into
    // |FinishedReceivingProfilerData()| when the task completes.
    metrics::TrackingSynchronizer::FetchProfilerDataAsynchronously(
        weak_ptr_factory_.GetWeakPtr());
  } else {
    CollectFinalHistograms();
  }
}

scoped_ptr<metrics::MetricsLogUploader>
IOSChromeMetricsServiceClient::CreateUploader(
    const base::Callback<void(int)>& on_upload_complete) {
  return scoped_ptr<metrics::MetricsLogUploader>(
      new metrics::NetMetricsLogUploader(
          GetApplicationContext()->GetSystemURLRequestContext(),
          metrics::kDefaultMetricsServerUrl, metrics::kDefaultMetricsMimeType,
          on_upload_complete));
}

base::TimeDelta IOSChromeMetricsServiceClient::GetStandardUploadInterval() {
  if (IsCellularLogicEnabled())
    return base::TimeDelta::FromSeconds(kStandardUploadIntervalCellularSeconds);
  return base::TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
}

base::string16 IOSChromeMetricsServiceClient::GetRegistryBackupKey() {
  return base::string16();
}

void IOSChromeMetricsServiceClient::OnRendererProcessCrash() {
  stability_metrics_provider_->LogRendererCrash();
}

void IOSChromeMetricsServiceClient::WebStateDidStartLoading(
    web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::WebStateDidStopLoading(
    web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::Initialize() {
  metrics_service_.reset(new metrics::MetricsService(
      metrics_state_manager_, this, GetApplicationContext()->GetLocalState()));

  // Register metrics providers.
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new metrics::NetworkMetricsProvider(
          web::WebThread::GetBlockingPool())));

  // Currently, we configure OmniboxMetricsProvider to not log events to UMA
  // if there is a single incognito session visible. In the future, it may
  // be worth revisiting this to still log events from non-incognito sessions.
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new OmniboxMetricsProvider(
          base::Bind(&::IsOffTheRecordSessionActive))));

  stability_metrics_provider_ = new IOSChromeStabilityMetricsProvider(
      GetApplicationContext()->GetLocalState());
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(stability_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new metrics::ScreenInfoMetricsProvider));

  drive_metrics_provider_ = new metrics::DriveMetricsProvider(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE),
      ios::FILE_LOCAL_STATE);
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(drive_metrics_provider_));

  profiler_metrics_provider_ =
      new metrics::ProfilerMetricsProvider(base::Bind(&IsCellularLogicEnabled));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(profiler_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          SigninStatusMetricsProvider::CreateInstance(make_scoped_ptr(
              new IOSChromeSigninStatusMetricsProviderDelegate))));

  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new MobileSessionShutdownMetricsProvider(metrics_service_.get())));
}

void IOSChromeMetricsServiceClient::OnInitTaskGotDriveMetrics() {
  finished_init_task_callback_.Run();
}

bool IOSChromeMetricsServiceClient::ShouldIncludeProfilerDataInLog() {
  // Upload profiler data at most once per session.
  if (has_uploaded_profiler_data_)
    return false;

  // For each log, flip a fair coin. Thus, profiler data is sent with the first
  // log with probability 50%, with the second log with probability 25%, and so
  // on. As a result, uploaded data is biased toward earlier logs.
  // TODO(isherman): Explore other possible algorithms, and choose one that
  // might be more appropriate.  For example, it might be reasonable to include
  // profiler data with some fixed probability, so that a given client might
  // upload profiler data more than once; but on average, clients won't upload
  // too much data.
  if (base::RandDouble() < 0.5)
    return false;

  has_uploaded_profiler_data_ = true;
  return true;
}

void IOSChromeMetricsServiceClient::ReceivedProfilerData(
    const metrics::ProfilerDataAttributes& attributes,
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    const metrics::ProfilerEvents& past_events) {
  profiler_metrics_provider_->RecordProfilerData(
      process_data_phase, attributes.process_id, attributes.process_type,
      attributes.profiling_phase, attributes.phase_start - start_time_,
      attributes.phase_finish - start_time_, past_events);
}

void IOSChromeMetricsServiceClient::FinishedReceivingProfilerData() {
  CollectFinalHistograms();
}

void IOSChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(ios): Try to extract the flow below into a utility function that is
  // shared between the iOS port's usage and
  // ChromeMetricsServiceClient::CollectFinalHistograms()'s usage of
  // MetricsMemoryDetails.
  scoped_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));
  UMA_HISTOGRAM_MEMORY_KB("Memory.Browser",
                          process_metrics->GetWorkingSetSize() / 1024);
  collect_final_metrics_done_callback_.Run();
}

void IOSChromeMetricsServiceClient::RegisterForNotifications() {
  tab_parented_subscription_ =
      TabParentingGlobalObserver::GetInstance()->RegisterCallback(
          base::Bind(&IOSChromeMetricsServiceClient::OnTabParented,
                     base::Unretained(this)));
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));
}

void IOSChromeMetricsServiceClient::OnTabParented(web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}
