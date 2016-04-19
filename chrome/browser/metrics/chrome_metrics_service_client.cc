// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/metrics/time_ticks_experiment_win.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/features.h"
#include "chrome/installer/util/util_constants.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/net/version_utils.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/device_count_metrics_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/metrics/android_metrics_provider.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/service_process/service_process_control.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/metrics/extensions_metrics_provider.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "chrome/browser/metrics/plugin_metrics_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/signin/signin_status_metrics_provider_chromeos.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include "chrome/browser/metrics/google_update_metrics_provider_win.h"
#include "chrome/common/metrics_constants_util_win.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/browser_watcher/watcher_metrics_provider_win.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/chrome_signin_status_metrics_provider_delegate.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#endif  // !defined(OS_CHROMEOS)

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

// Standard interval between log uploads, in seconds.
#if defined(OS_ANDROID)
const int kStandardUploadIntervalSeconds = 5 * 60;  // Five minutes.
const int kStandardUploadIntervalCellularSeconds = 15 * 60;  // Fifteen minutes.
#else
const int kStandardUploadIntervalSeconds = 30 * 60;  // Thirty minutes.
#endif

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

// Checks whether it is the first time that cellular uploads logic should be
// enabled based on whether the the preference for that logic is initialized.
// This should happen only once as the used preference will be initialized
// afterwards in |UmaSessionStats.java|.
bool ShouldClearSavedMetrics() {
#if BUILDFLAG(ANDROID_JAVA_UI)
  PrefService* local_state = g_browser_process->local_state();
  return !local_state->HasPrefPath(metrics::prefs::kMetricsReportingEnabled) &&
         variations::GetVariationParamValue("UMA_EnableCellularLogUpload",
                                            "Enabled") == "true";
#else
  return false;
#endif
}

void RegisterInstallerFileMetricsPreferences(PrefRegistrySimple* registry) {
#if defined(OS_WIN)
  metrics::FileMetricsProvider::RegisterPrefs(
      registry, installer::kSetupHistogramAllocatorName);
#endif
}

void RegisterInstallerFileMetricsProvider(
    metrics::MetricsService* metrics_service) {
#if defined(OS_WIN)
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics(
      new metrics::FileMetricsProvider(
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN),
          g_browser_process->local_state()));
  base::FilePath program_dir;
  base::PathService::Get(base::DIR_EXE, &program_dir);
  file_metrics->RegisterFile(
      program_dir.AppendASCII(installer::kSetupHistogramAllocatorName)
          .AddExtension(L".pma"),
      metrics::FileMetricsProvider::FILE_HISTOGRAMS_ATOMIC,
      installer::kSetupHistogramAllocatorName);
  metrics_service->RegisterMetricsProvider(std::move(file_metrics));
#endif
}

}  // namespace


ChromeMetricsServiceClient::ChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
#if defined(OS_CHROMEOS)
      chromeos_metrics_provider_(nullptr),
#endif
      waiting_for_collect_final_metrics_step_(false),
      num_async_histogram_fetches_in_progress_(0),
      profiler_metrics_provider_(nullptr),
#if defined(ENABLE_PLUGINS)
      plugin_metrics_provider_(nullptr),
#endif
#if defined(OS_WIN)
      google_update_metrics_provider_(nullptr),
#endif
      drive_metrics_provider_(nullptr),
      start_time_(base::TimeTicks::Now()),
      has_uploaded_profiler_data_(false),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordCommandLineMetrics();
  RegisterForNotifications();
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
std::unique_ptr<ChromeMetricsServiceClient> ChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager,
    PrefService* local_state) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  std::unique_ptr<ChromeMetricsServiceClient> client(
      new ChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void ChromeMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);

  RegisterInstallerFileMetricsPreferences(registry);

  RegisterMetricsReportingStatePrefs(registry);

#if BUILDFLAG(ANDROID_JAVA_UI)
  AndroidMetricsProvider::RegisterPrefs(registry);
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if defined(ENABLE_PLUGINS)
  PluginMetricsProvider::RegisterPrefs(registry);
#endif  // defined(ENABLE_PLUGINS)
}

metrics::MetricsService* ChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

void ChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

void ChromeMetricsServiceClient::OnRecordingDisabled() {
  // If we're shutting down, don't drop the metrics_client_id, so that late
  // crashes won't lose it.
  if (!g_browser_process->IsShuttingDown())
    crash_keys::ClearMetricsClientId();
}

bool ChromeMetricsServiceClient::IsOffTheRecordSessionActive() {
  return chrome::IsOffTheRecordSessionActive();
}

int32_t ChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string ChromeMetricsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(chrome::GetChannel());
}

std::string ChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void ChromeMetricsServiceClient::OnLogUploadComplete() {
  // Collect time ticks stats after each UMA upload.
#if defined(OS_WIN)
  chrome::CollectTimeTicksStats();
#endif
}

void ChromeMetricsServiceClient::InitializeSystemProfileMetrics(
    const base::Closure& done_callback) {
  finished_init_task_callback_ = done_callback;
  base::Closure got_hardware_class_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotHardwareClass,
                 weak_ptr_factory_.GetWeakPtr());
#if defined(OS_CHROMEOS)
  chromeos_metrics_provider_->InitTaskGetHardwareClass(
      got_hardware_class_callback);
#else
  got_hardware_class_callback.Run();
#endif  // defined(OS_CHROMEOS)
}

void ChromeMetricsServiceClient::CollectFinalMetricsForLog(
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

std::unique_ptr<metrics::MetricsLogUploader>
ChromeMetricsServiceClient::CreateUploader(
    const base::Callback<void(int)>& on_upload_complete) {
  return std::unique_ptr<metrics::MetricsLogUploader>(
      new metrics::NetMetricsLogUploader(
          g_browser_process->system_request_context(),
          metrics::kDefaultMetricsServerUrl, metrics::kDefaultMetricsMimeType,
          on_upload_complete));
}

base::TimeDelta ChromeMetricsServiceClient::GetStandardUploadInterval() {
#if defined(OS_ANDROID)
  if (IsCellularLogicEnabled())
    return base::TimeDelta::FromSeconds(kStandardUploadIntervalCellularSeconds);
#endif
  return base::TimeDelta::FromSeconds(kStandardUploadIntervalSeconds);
}

base::string16 ChromeMetricsServiceClient::GetRegistryBackupKey() {
#if defined(OS_WIN)
  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  return distribution->GetRegistryPath().append(L"\\StabilityMetrics");
#else
  return base::string16();
#endif
}

void ChromeMetricsServiceClient::OnPluginLoadingError(
    const base::FilePath& plugin_path) {
#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_->LogPluginLoadingError(plugin_path);
#else
  NOTREACHED();
#endif  // defined(ENABLE_PLUGINS)
}

bool ChromeMetricsServiceClient::IsReportingPolicyManaged() {
  return IsMetricsReportingPolicyManaged();
}

metrics::MetricsServiceClient::EnableMetricsDefault
ChromeMetricsServiceClient::GetDefaultOptIn() {
  return GetMetricsReportingDefaultOptIn(g_browser_process->local_state());
}

void ChromeMetricsServiceClient::Initialize() {
  // Clear metrics reports if it is the first time cellular upload logic should
  // apply to avoid sudden bulk uploads. It needs to be done before initializing
  // metrics service so that metrics log manager is initialized correctly.
  if (ShouldClearSavedMetrics()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->ClearPref(metrics::prefs::kMetricsInitialLogs);
    local_state->ClearPref(metrics::prefs::kMetricsOngoingLogs);
  }

  metrics_service_.reset(new metrics::MetricsService(
      metrics_state_manager_, this, g_browser_process->local_state()));

  // Gets access to persistent metrics shared by sub-processes.
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new SubprocessMetricsProvider()));

  // Register metrics providers.
#if defined(ENABLE_EXTENSIONS)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new ExtensionsMetricsProvider(metrics_state_manager_)));
#endif
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::NetworkMetricsProvider(
              content::BrowserThread::GetBlockingPool())));

  // Currently, we configure OmniboxMetricsProvider to not log events to UMA
  // if there is a single incognito session visible. In the future, it may
  // be worth revisiting this to still log events from non-incognito sessions.
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(new OmniboxMetricsProvider(
          base::Bind(&chrome::IsOffTheRecordSessionActive))));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new ChromeStabilityMetricsProvider(
              g_browser_process->local_state())));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::GPUMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::ScreenInfoMetricsProvider));

  RegisterInstallerFileMetricsProvider(metrics_service_.get());

  drive_metrics_provider_ = new metrics::DriveMetricsProvider(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE),
      chrome::FILE_LOCAL_STATE);
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(drive_metrics_provider_));

  profiler_metrics_provider_ =
      new metrics::ProfilerMetricsProvider(base::Bind(&IsCellularLogicEnabled));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(profiler_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));

#if BUILDFLAG(ANDROID_JAVA_UI)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new AndroidMetricsProvider(g_browser_process->local_state())));
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if defined(OS_WIN)
  google_update_metrics_provider_ = new GoogleUpdateMetricsProviderWin;
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          google_update_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new browser_watcher::WatcherMetricsProviderWin(
              chrome::GetBrowserExitCodesRegistryPath(),
              content::BrowserThread::GetBlockingPool())));
#endif  // defined(OS_WIN)

#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_ =
      new PluginMetricsProvider(g_browser_process->local_state());
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(plugin_metrics_provider_));
#endif  // defined(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
  ChromeOSMetricsProvider* chromeos_metrics_provider =
      new ChromeOSMetricsProvider;
  chromeos_metrics_provider_ = chromeos_metrics_provider;
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(chromeos_metrics_provider));

  SigninStatusMetricsProviderChromeOS* signin_metrics_provider_cros =
      new SigninStatusMetricsProviderChromeOS;
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(signin_metrics_provider_cros));
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          SigninStatusMetricsProvider::CreateInstance(base::WrapUnique(
              new ChromeSigninStatusMetricsProviderDelegate))));
#endif  // !defined(OS_CHROMEOS)

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new sync_driver::DeviceCountMetricsProvider(base::Bind(
              &browser_sync::ChromeSyncClient::GetDeviceInfoTrackers))));

  // Clear stability metrics if it is the first time cellular upload logic
  // should apply to avoid sudden bulk uploads. It needs to be done after all
  // providers are registered.
  if (ShouldClearSavedMetrics())
    metrics_service_->ClearSavedStabilityMetrics();
}

void ChromeMetricsServiceClient::OnInitTaskGotHardwareClass() {
  const base::Closure got_bluetooth_adapter_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotBluetoothAdapter,
                 weak_ptr_factory_.GetWeakPtr());
#if defined(OS_CHROMEOS)
  chromeos_metrics_provider_->InitTaskGetBluetoothAdapter(
      got_bluetooth_adapter_callback);
#else
  got_bluetooth_adapter_callback.Run();
#endif  // defined(OS_CHROMEOS)
}

void ChromeMetricsServiceClient::OnInitTaskGotBluetoothAdapter() {
  const base::Closure got_plugin_info_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotPluginInfo,
                 weak_ptr_factory_.GetWeakPtr());

#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_->GetPluginInformation(got_plugin_info_callback);
#else
  got_plugin_info_callback.Run();
#endif  // defined(ENABLE_PLUGINS)
}

void ChromeMetricsServiceClient::OnInitTaskGotPluginInfo() {
  const base::Closure got_metrics_callback =
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotGoogleUpdateData,
                 weak_ptr_factory_.GetWeakPtr());

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  google_update_metrics_provider_->GetGoogleUpdateData(got_metrics_callback);
#else
  got_metrics_callback.Run();
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
}

void ChromeMetricsServiceClient::OnInitTaskGotGoogleUpdateData() {
  drive_metrics_provider_->GetDriveMetrics(
      base::Bind(&ChromeMetricsServiceClient::OnInitTaskGotDriveMetrics,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ChromeMetricsServiceClient::OnInitTaskGotDriveMetrics() {
  finished_init_task_callback_.Run();
}

bool ChromeMetricsServiceClient::ShouldIncludeProfilerDataInLog() {
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

void ChromeMetricsServiceClient::ReceivedProfilerData(
    const metrics::ProfilerDataAttributes& attributes,
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    const metrics::ProfilerEvents& past_events) {
  profiler_metrics_provider_->RecordProfilerData(
      process_data_phase, attributes.process_id, attributes.process_type,
      attributes.profiling_phase, attributes.phase_start - start_time_,
      attributes.phase_finish - start_time_, past_events);
}

void ChromeMetricsServiceClient::FinishedReceivingProfilerData() {
  CollectFinalHistograms();
}

void ChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Begin the multi-step process of collecting memory usage histograms:
  // First spawn a task to collect the memory details; when that task is
  // finished, it will call OnMemoryDetailCollectionDone. That will in turn
  // call HistogramSynchronization to collect histograms from all renderers and
  // then call OnHistogramSynchronizationDone to continue processing.
  DCHECK(!waiting_for_collect_final_metrics_step_);
  waiting_for_collect_final_metrics_step_ = true;

  base::Closure callback =
      base::Bind(&ChromeMetricsServiceClient::OnMemoryDetailCollectionDone,
                 weak_ptr_factory_.GetWeakPtr());

  scoped_refptr<MetricsMemoryDetails> details(
      new MetricsMemoryDetails(callback, &memory_growth_tracker_));
  details->StartFetch();
}

void ChromeMetricsServiceClient::OnMemoryDetailCollectionDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);

  // Create a callback_task for OnHistogramSynchronizationDone.
  base::Closure callback = base::Bind(
      &ChromeMetricsServiceClient::OnHistogramSynchronizationDone,
      weak_ptr_factory_.GetWeakPtr());

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(kMaxHistogramGatheringWaitDuration);

  DCHECK_EQ(num_async_histogram_fetches_in_progress_, 0);

#if !defined(ENABLE_PRINT_PREVIEW)
  num_async_histogram_fetches_in_progress_ = 1;
#else   // !ENABLE_PRINT_PREVIEW
  num_async_histogram_fetches_in_progress_ = 2;
  // Run requests to service and content in parallel.
  if (!ServiceProcessControl::GetInstance()->GetHistograms(callback, timeout)) {
    // Assume |num_async_histogram_fetches_in_progress_| is not changed by
    // |GetHistograms()|.
    DCHECK_EQ(num_async_histogram_fetches_in_progress_, 2);
    // Assign |num_async_histogram_fetches_in_progress_| above and decrement it
    // here to make code work even if |GetHistograms()| fired |callback|.
    --num_async_histogram_fetches_in_progress_;
  }
#endif  // !ENABLE_PRINT_PREVIEW

  // Set up the callback task to call after we receive histograms from all
  // child processes. |timeout| specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(base::MessageLoop::current(), callback,
                                         timeout);
}

void ChromeMetricsServiceClient::OnHistogramSynchronizationDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This function should only be called as the callback from an ansynchronous
  // step.
  DCHECK(waiting_for_collect_final_metrics_step_);
  DCHECK_GT(num_async_histogram_fetches_in_progress_, 0);

  // Check if all expected requests finished.
  if (--num_async_histogram_fetches_in_progress_ > 0)
    return;

  waiting_for_collect_final_metrics_step_ = false;
  collect_final_metrics_done_callback_.Run();
}

void ChromeMetricsServiceClient::RecordCommandLineMetrics() {
  // Get stats on use of command line.
  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  size_t common_commands = 0;
  if (command_line->HasSwitch(switches::kUserDataDir)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineDatDirCount", 1);
  }

  if (command_line->HasSwitch(switches::kApp)) {
    ++common_commands;
    UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineAppModeCount", 1);
  }

  // TODO(rohitrao): Should these be logged on iOS as well?
  // http://crbug.com/375794
  size_t switch_count = command_line->GetSwitches().size();
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineFlagCount", switch_count);
  UMA_HISTOGRAM_COUNTS_100("Chrome.CommandLineUncommonFlagCount",
                           switch_count - common_commands);
}

void ChromeMetricsServiceClient::RegisterForNotifications() {
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_PARENTED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
                 content::NotificationService::AllSources());

  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&ChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));
}

void ChromeMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED:
    case chrome::NOTIFICATION_BROWSER_CLOSED:
    case chrome::NOTIFICATION_TAB_PARENTED:
    case chrome::NOTIFICATION_TAB_CLOSING:
    case content::NOTIFICATION_LOAD_STOP:
    case content::NOTIFICATION_LOAD_START:
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
      metrics_service_->OnApplicationNotIdle();
      break;

    default:
      NOTREACHED();
  }
}

void ChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

bool ChromeMetricsServiceClient::IsUMACellularUploadLogicEnabled() {
  return IsCellularLogicEnabled();
}
