// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/demographic_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/persistent_histograms.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/metrics/version_utils.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_device_info/device_count_metrics_provider.h"
#include "components/ukm/ukm_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/google/google_brand.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/metrics/chrome_browser_state_client.h"
#include "ios/chrome/browser/metrics/ios_chrome_stability_metrics_provider.h"
#include "ios/chrome/browser/metrics/mobile_session_shutdown_metrics_provider.h"
#include "ios/chrome/browser/signin/ios_chrome_signin_status_metrics_provider_delegate.h"
#include "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/tabs/tab_parenting_global_observer.h"
#include "ios/chrome/browser/translate/translate_ranker_metrics_provider.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/thread/web_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum amount of local storage for storing persistent histograms.
const int kMaxHistogramStorageKiB = 50 << 10;  // 50 MiB

void GetNetworkConnectionTrackerAsync(
    base::OnceCallback<void(network::NetworkConnectionTracker*)> callback) {
  std::move(callback).Run(
      GetApplicationContext()->GetNetworkConnectionTracker());
}

std::unique_ptr<metrics::FileMetricsProvider> CreateFileMetricsProvider(
    bool metrics_reporting_enabled) {
  // Create an object to monitor files of metrics and include them in reports.
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics_provider(
      new metrics::FileMetricsProvider(
          GetApplicationContext()->GetLocalState()));

  base::FilePath user_data_dir;
  if (base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir)) {
    base::FilePath browser_metrics_upload_dir =
        user_data_dir.AppendASCII(kBrowserMetricsName);
    if (metrics_reporting_enabled) {
      metrics::FileMetricsProvider::Params browser_metrics_params(
          browser_metrics_upload_dir,
          metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          metrics::FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE,
          kBrowserMetricsName);
      browser_metrics_params.max_dir_kib = kMaxHistogramStorageKiB;
      browser_metrics_params.filter = base::BindRepeating(
          &IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles);
      file_metrics_provider->RegisterSource(browser_metrics_params);
    } else {
      // When metrics reporting is not enabled, any existing files should be
      // deleted in order to preserve user privacy.
      base::PostTask(FROM_HERE,
                     {base::ThreadPool(), base::MayBlock(),
                      base::TaskPriority::BEST_EFFORT,
                      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                     base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                    std::move(browser_metrics_upload_dir),
                                    /*recursive=*/true));
    }
  }
  return file_metrics_provider;
}

}  // namespace

IOSChromeMetricsServiceClient::IOSChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
      stability_metrics_provider_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  notification_listeners_active_ = RegisterForNotifications();
}

IOSChromeMetricsServiceClient::~IOSChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
std::unique_ptr<IOSChromeMetricsServiceClient>
IOSChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  std::unique_ptr<IOSChromeMetricsServiceClient> client(
      new IOSChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void IOSChromeMetricsServiceClient::RegisterPrefs(
    PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
  metrics::FileMetricsProvider::RegisterPrefs(registry, kBrowserMetricsName);
  metrics::RegisterMetricsReportingStatePrefs(registry);
  ukm::UkmService::RegisterPrefs(registry);
}

metrics::MetricsService* IOSChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

ukm::UkmService* IOSChromeMetricsServiceClient::GetUkmService() {
  return ukm_service_.get();
}

void IOSChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
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

void IOSChromeMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = done_callback;
  CollectFinalHistograms();
}

std::unique_ptr<metrics::MetricsLogUploader>
IOSChromeMetricsServiceClient::CreateUploader(
    const GURL& server_url,
    const GURL& insecure_server_url,
    base::StringPiece mime_type,
    metrics::MetricsLogUploader::MetricServiceType service_type,
    const metrics::MetricsLogUploader::UploadCallback& on_upload_complete) {
  return std::make_unique<metrics::NetMetricsLogUploader>(
      GetApplicationContext()->GetSharedURLLoaderFactory(), server_url,
      insecure_server_url, mime_type, service_type, on_upload_complete);
}

base::TimeDelta IOSChromeMetricsServiceClient::GetStandardUploadInterval() {
  return metrics::GetUploadInterval(metrics::ShouldUseCellularUploadInterval());
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
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  metrics_service_ = std::make_unique<metrics::MetricsService>(
      metrics_state_manager_, this, local_state);

  if (IsMetricsReportingForceEnabled() ||
      base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    // We only need to restrict to whitelisted Entries if metrics reporting
    // is not forced.
    bool restrict_to_whitelist_entries = !IsMetricsReportingForceEnabled();
    ukm_service_ = std::make_unique<ukm::UkmService>(
        local_state, this, restrict_to_whitelist_entries,
        std::make_unique<metrics::DemographicMetricsProvider>(
            std::make_unique<metrics::ChromeBrowserStateClient>(),
            metrics::MetricsLogUploader::MetricServiceType::UKM));
  }

  // Register metrics providers.
  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::NetworkMetricsProvider>(
          base::BindRepeating(&GetNetworkConnectionTrackerAsync)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<OmniboxMetricsProvider>());

  {
    auto stability_metrics_provider =
        std::make_unique<IOSChromeStabilityMetricsProvider>(
            GetApplicationContext()->GetLocalState());
    stability_metrics_provider_ = stability_metrics_provider.get();
    metrics_service_->RegisterMetricsProvider(
        std::move(stability_metrics_provider));
  }

  // NOTE: metrics_state_manager_->IsMetricsReportingEnabled() returns false
  // during local testing. To test locally, modify
  // MetricsServiceAccessor::IsMetricsReportingEnabled() to return true.
  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      metrics_state_manager_->IsMetricsReportingEnabled()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::ScreenInfoMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DriveMetricsProvider>(ios::FILE_LOCAL_STATE));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::CallStackProfileMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      SigninStatusMetricsProvider::CreateInstance(
          std::make_unique<IOSChromeSigninStatusMetricsProviderDelegate>()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<MobileSessionShutdownMetricsProvider>(
          metrics_service_.get()));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<syncer::DeviceCountMetricsProvider>(base::BindRepeating(
          &DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers)));

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<translate::TranslateRankerMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      std::make_unique<metrics::DemographicMetricsProvider>(
          std::make_unique<metrics::ChromeBrowserStateClient>(),
          metrics::MetricsLogUploader::MetricServiceType::UMA));
}

void IOSChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(ios): Try to extract the flow below into a utility function that is
  // shared between the iOS port's usage and
  // ChromeMetricsServiceClient::CollectFinalHistograms()'s usage of
  // MetricsMemoryDetails.
  task_vm_info task_info_data;
  mach_msg_type_number_t count = sizeof(task_vm_info) / sizeof(natural_t);
  kern_return_t kr =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_info_data), &count);
  if (kr == KERN_SUCCESS) {
    UMA_HISTOGRAM_MEMORY_KB(
        "Memory.Browser",
        (task_info_data.resident_size - task_info_data.reusable) / 1024);
  }

  collect_final_metrics_done_callback_.Run();
}

bool IOSChromeMetricsServiceClient::RegisterForNotifications() {
  tab_parented_subscription_ =
      TabParentingGlobalObserver::GetInstance()->RegisterCallback(
          base::Bind(&IOSChromeMetricsServiceClient::OnTabParented,
                     base::Unretained(this)));
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));

  std::vector<ios::ChromeBrowserState*> loaded_browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  bool all_profiles_succeeded = true;
  for (ios::ChromeBrowserState* browser_state : loaded_browser_states) {
    if (!RegisterForBrowserStateEvents(browser_state)) {
      all_profiles_succeeded = false;
    }
  }
  return all_profiles_succeeded;
}

bool IOSChromeMetricsServiceClient::RegisterForBrowserStateEvents(
    ios::ChromeBrowserState* browser_state) {
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::IMPLICIT_ACCESS);
  ObserveServiceForDeletions(history_service);
  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForBrowserState(
          browser_state);
  ObserveServiceForSyncDisables(sync, browser_state->GetPrefs());
  return (history_service != nullptr && sync != nullptr);
}

void IOSChromeMetricsServiceClient::OnTabParented(web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

// static
metrics::FileMetricsProvider::FilterAction
IOSChromeMetricsServiceClient::FilterBrowserMetricsFiles(
    const base::FilePath& path) {
  // Do not process the file if it corresponds to the current process id.
  base::ProcessId pid;
  bool parse_success = base::GlobalHistogramAllocator::ParseFilePath(
      path, nullptr, nullptr, &pid);
  if (!parse_success)
    return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
  if (pid == base::GetCurrentProcId())
    return metrics::FileMetricsProvider::FILTER_ACTIVE_THIS_PID;
  // No need to test whether |pid| is a different active process. This isn't
  // applicable to iOS because there cannot be two copies of Chrome running.
  return metrics::FileMetricsProvider::FILTER_PROCESS_FILE;
}

metrics::EnableMetricsDefault
IOSChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      GetApplicationContext()->GetLocalState());
}

void IOSChromeMetricsServiceClient::OnHistoryDeleted() {
  if (ukm_service_)
    ukm_service_->Purge();
}

void IOSChromeMetricsServiceClient::OnSyncPrefsChanged(bool must_purge) {
  if (!ukm_service_)
    return;
  if (must_purge) {
    ukm_service_->Purge();
    ukm_service_->ResetClientState(ukm::ResetReason::kOnSyncPrefsChanged);
  }
  // Signal service manager to enable/disable UKM based on new state.
  UpdateRunningServices();
}

void IOSChromeMetricsServiceClient::OnIncognitoWebStateAdded() {
  // Signal service manager to enable/disable UKM based on new state.
  UpdateRunningServices();
}

void IOSChromeMetricsServiceClient::OnIncognitoWebStateRemoved() {
  // Signal service manager to enable/disable UKM based on new state.
  UpdateRunningServices();
}

bool IOSChromeMetricsServiceClient::SyncStateAllowsUkm() {
  return SyncDisableObserver::SyncStateAllowsUkm();
}

bool IOSChromeMetricsServiceClient::
    AreNotificationListenersEnabledOnAllProfiles() {
  return notification_listeners_active_;
}

std::string IOSChromeMetricsServiceClient::GetUploadSigningKey() {
  std::string decoded_key;
  base::Base64Decode(google_apis::GetMetricsKey(), &decoded_key);
  return decoded_key;
}
