// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"
#include "chrome/browser/metrics/https_engagement_metrics_provider.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/metrics/sampling_metrics_provider.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/metrics/time_ticks_experiment_win.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/util_constants.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/file_metrics_provider.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/net/cellular_logic_helper.h"
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
#include "components/sync/device_info/device_count_metrics_provider.h"
#include "components/translate/core/browser/translate_ranker_metrics_provider.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"
#include "extensions/features/features.h"
#include "ppapi/features/features.h"
#include "printing/features/features.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/metrics/android_metrics_provider.h"
#include "chrome/browser/metrics/page_load_metrics_provider.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/service_process/service_process_control.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/metrics/extensions_metrics_provider.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/metrics/plugin_metrics_provider.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/signin/signin_status_metrics_provider_chromeos.h"
#endif

#if defined(OS_WIN)
#include <windows.h>

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#include "chrome/browser/metrics/google_update_metrics_provider_win.h"
#include "chrome/common/metrics_constants_util_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/browser_watcher/stability_debugging_win.h"
#include "components/browser_watcher/watcher_metrics_provider_win.h"
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/chrome_signin_status_metrics_provider_delegate.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#endif  // !defined(OS_CHROMEOS)

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

// Needs to be kept in sync with the writer in
// third_party/crashpad/crashpad/handler/handler_main.cc.
const char kCrashpadHistogramAllocatorName[] = "CrashpadMetrics";

#if defined(OS_WIN) || defined(OS_MACOSX)
// The stream type assigned to the minidump stream that holds the serialized
// system profile proto.
const uint32_t kSystemProfileMinidumpStreamType = 0x4B6B0003;

// A serialized environment (SystemProfileProto) that was registered with the
// crash reporter, or the empty string if no environment was registered yet.
// Ownership must be maintained after registration as the crash reporter does
// not assume it.
// TODO(manzagop): revisit this if the Crashpad API evolves.
base::LazyInstance<std::string>::Leaky g_environment_for_crash_reporter;
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

void RegisterFileMetricsPreferences(PrefRegistrySimple* registry) {
  metrics::FileMetricsProvider::RegisterPrefs(
      registry, ChromeMetricsServiceClient::kBrowserMetricsName);

  metrics::FileMetricsProvider::RegisterPrefs(registry,
                                              kCrashpadHistogramAllocatorName);

#if defined(OS_WIN)
  metrics::FileMetricsProvider::RegisterPrefs(
      registry, installer::kSetupHistogramAllocatorName);
#endif
}

// Constructs the name of a persistent metrics file from a directory and metrics
// name, and either registers that file as associated with a previous run if
// metrics reporting is enabled, or deletes it if not.
void RegisterOrRemovePreviousRunMetricsFile(
    bool metrics_reporting_enabled,
    const base::FilePath& dir,
    base::StringPiece metrics_name,
    scoped_refptr<base::TaskRunner> task_runner,
    metrics::FileMetricsProvider* file_metrics_provider) {
  base::FilePath metrics_file;
  base::GlobalHistogramAllocator::ConstructFilePaths(dir, metrics_name,
                                                     &metrics_file, nullptr);

  if (metrics_reporting_enabled) {
    // Enable reading any existing saved metrics.
    file_metrics_provider->RegisterSource(
        metrics_file,
        metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
        metrics::FileMetricsProvider::ASSOCIATE_PREVIOUS_RUN, metrics_name);
  } else {
    // When metrics reporting is not enabled, any existing file should be
    // deleted in order to preserve user privacy.
    task_runner->PostTask(FROM_HERE,
                          base::Bind(base::IgnoreResult(&base::DeleteFile),
                                     metrics_file, /*recursive=*/false));
  }
}

std::unique_ptr<metrics::FileMetricsProvider> CreateFileMetricsProvider(
    bool metrics_reporting_enabled) {
  // Fetch a worker-pool for performing I/O tasks that are not allowed on
  // the main UI thread.
  scoped_refptr<base::TaskRunner> task_runner =
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  // Create an object to monitor files of metrics and include them in reports.
  std::unique_ptr<metrics::FileMetricsProvider> file_metrics_provider(
      new metrics::FileMetricsProvider(task_runner,
                                       g_browser_process->local_state()));

  // Register the file holding browser metrics.
  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    RegisterOrRemovePreviousRunMetricsFile(
        metrics_reporting_enabled, user_data_dir,
        ChromeMetricsServiceClient::kBrowserMetricsName, task_runner,
        file_metrics_provider.get());
  }

  // Read the Crashpad metrics files.
  base::FilePath default_user_data_dir;
  if (chrome::GetDefaultUserDataDirectory(&default_user_data_dir)) {
    // Register the data from the previous run if crashpad_handler didn't exit
    // cleanly.
    RegisterOrRemovePreviousRunMetricsFile(
        metrics_reporting_enabled, default_user_data_dir,
        kCrashpadHistogramAllocatorName, task_runner,
        file_metrics_provider.get());
    if (metrics_reporting_enabled) {
      base::FilePath active_path;
      base::GlobalHistogramAllocator::ConstructFilePaths(
          default_user_data_dir, kCrashpadHistogramAllocatorName, nullptr,
          &active_path);
      // Register data that will be populated for the current run.
      file_metrics_provider->RegisterSource(
          active_path,
          metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
          kCrashpadHistogramAllocatorName);
    }
  }

#if defined(OS_WIN)
  // Read metrics file from setup.exe.
  base::FilePath program_dir;
  base::PathService::Get(base::DIR_EXE, &program_dir);
  file_metrics_provider->RegisterSource(
      program_dir.AppendASCII(installer::kSetupHistogramAllocatorName),
      metrics::FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      metrics::FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
      installer::kSetupHistogramAllocatorName);
#endif

  return file_metrics_provider;
}

#if defined(OS_WIN)
void GetExecutableVersionDetails(base::string16* product_name,
                                 base::string16* version_number,
                                 base::string16* channel_name) {
  DCHECK_NE(nullptr, product_name);
  DCHECK_NE(nullptr, version_number);
  DCHECK_NE(nullptr, channel_name);

  wchar_t exe_file[MAX_PATH] = {};
  CHECK(::GetModuleFileName(nullptr, exe_file, arraysize(exe_file)));

  base::string16 unused_special_build;
  install_static::GetExecutableVersionDetails(
      exe_file, product_name, version_number, &unused_special_build,
      channel_name);
}
#endif  // OS_WIN

}  // namespace

const char ChromeMetricsServiceClient::kBrowserMetricsName[] = "BrowserMetrics";

ChromeMetricsServiceClient::ChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
#if defined(OS_CHROMEOS)
      chromeos_metrics_provider_(nullptr),
#endif
      waiting_for_collect_final_metrics_step_(false),
      num_async_histogram_fetches_in_progress_(0),
      profiler_metrics_provider_(nullptr),
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_metrics_provider_(nullptr),
#endif
#if defined(OS_WIN)
      google_update_metrics_provider_(nullptr),
      watcher_metrics_provider_(nullptr),
      antivirus_metrics_provider_(nullptr),
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
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (allocator) {
    // A normal shutdown is almost complete so there is no benefit in keeping a
    // file with no new data to be processed during the next startup sequence.
    // Deleting the file during shutdown adds an extra disk-access or two to
    // shutdown but eliminates the unnecessary processing of the contents during
    // startup only to find nothing.
    allocator->DeletePersistentLocation();
  }
}

// static
std::unique_ptr<ChromeMetricsServiceClient> ChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager) {
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

  RegisterFileMetricsPreferences(registry);

  metrics::RegisterMetricsReportingStatePrefs(registry);

#if BUILDFLAG(ANDROID_JAVA_UI)
  AndroidMetricsProvider::RegisterPrefs(registry);
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginMetricsProvider::RegisterPrefs(registry);
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

metrics::MetricsService* ChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

void ChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
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

void ChromeMetricsServiceClient::OnEnvironmentUpdate(std::string* environment) {
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK(environment);

  // Register the environment with the crash reporter. Note this only registers
  // the first environment, meaning ulterior updates to the environment are not
  // reflected in crash report environments (e.g. fieldtrial information). This
  // approach is due to the Crashpad API at time of implementation (registered
  // data cannot be updated). It would however be unwise to rely on such a
  // mechanism to retrieve the value of the dynamic fields due to the
  // environment update lag. Also note there is a window from startup to this
  // point during which crash reports will not have an environment set.
  if (!g_environment_for_crash_reporter.Get().empty())
    return;

  g_environment_for_crash_reporter.Get() = std::move(*environment);

  crashpad::CrashpadInfo::GetCrashpadInfo()->AddUserDataMinidumpStream(
      kSystemProfileMinidumpStreamType,
      reinterpret_cast<const void*>(
          g_environment_for_crash_reporter.Get().data()),
      g_environment_for_crash_reporter.Get().size());
#endif  // OS_WIN || OS_MACOSX
}

void ChromeMetricsServiceClient::OnLogUploadComplete() {
  // Collect time ticks stats after each UMA upload.
#if defined(OS_WIN)
  chrome::CollectTimeTicksStats();
#endif
}

void ChromeMetricsServiceClient::OnLogCleanShutdown() {
#if defined(OS_WIN)
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    // TODO(manzagop): add a metric.
    return;
  }
  browser_watcher::MarkStabilityFileForDeletion(user_data_dir);
#endif  // OS_WIN
}

void ChromeMetricsServiceClient::InitializeSystemProfileMetrics(
    const base::Closure& done_callback) {
  DCHECK(initialize_task_queue_.empty());

  // Each provider's initializer takes its own "done_callback" to enable
  // asynchronously chaining. We bind |next_task| to this callback, so
  // OnInitNextTask() gets called after each initializer, and we can go through
  // successive tasks in |initialize_task_queue_|. Note that |next_task| is
  // copied by value when passed into base::Bind().
  base::Closure next_task =
      base::Bind(&ChromeMetricsServiceClient::OnInitNextTask,
                 weak_ptr_factory_.GetWeakPtr());

  // The providers below can be bound using base::Unretained(), because task
  // execution occurs at OnInitNextTask() via |next_task|, which is guarded by
  // weak pointer usage.

#if defined(OS_CHROMEOS)
  // Load hardware class information.
  initialize_task_queue_.push_back(
      base::Bind(&ChromeOSMetricsProvider::InitTaskGetHardwareClass,
                 base::Unretained(chromeos_metrics_provider_), next_task));
  // Get a Bluetooth Adapter.
  initialize_task_queue_.push_back(
      base::Bind(&ChromeOSMetricsProvider::InitTaskGetBluetoothAdapter,
                 base::Unretained(chromeos_metrics_provider_), next_task));
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
  // Load plugin information.
  initialize_task_queue_.push_back(
      base::Bind(&PluginMetricsProvider::GetPluginInformation,
                 base::Unretained(plugin_metrics_provider_), next_task));
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  // Launch task to gather Google Update statistics.
  initialize_task_queue_.push_back(
      base::Bind(&GoogleUpdateMetricsProviderWin::GetGoogleUpdateData,
                 base::Unretained(google_update_metrics_provider_), next_task));
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)

#if defined(OS_WIN)
  // Load AntiVirus metrics.
  initialize_task_queue_.push_back(
      base::Bind(&AntiVirusMetricsProvider::GetAntiVirusMetrics,
                 base::Unretained(antivirus_metrics_provider_), next_task));
#endif  // defined(OS_WIN)

  // Load drive metrics.
  initialize_task_queue_.push_back(
      base::Bind(&metrics::DriveMetricsProvider::GetDriveMetrics,
                 base::Unretained(drive_metrics_provider_), next_task));

#if defined(OS_WIN)
  // Optionally collect postmortem reports.
  initialize_task_queue_.push_back(base::Bind(
      &browser_watcher::WatcherMetricsProviderWin::CollectPostmortemReports,
      base::Unretained(watcher_metrics_provider_), next_task));
#endif  // defined(OS_WIN)

  // Finally, call |done_callback| (which skips |next_task|).
  initialize_task_queue_.push_back(done_callback);

  // Do not add more items to |initialize_task_queue_| here; |done_callback|
  // should be the last!

  OnInitNextTask();
}

void ChromeMetricsServiceClient::OnInitNextTask() {
  if (initialize_task_queue_.empty())
    return;

  auto task = initialize_task_queue_.front();
  // |task.Run()| can be asynchronous or synchronous. For the latter case, we
  // may recurse back to this function. Therefore we must pop_front() first.
  initialize_task_queue_.pop_front();
  // Assumes this causes |OnInitNextTask()| to be called again for all but the
  // last task in |initialize_task_queue_|.
  task.Run();
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
  return metrics::GetUploadInterval();
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
#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_metrics_provider_->LogPluginLoadingError(plugin_path);
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

bool ChromeMetricsServiceClient::IsReportingPolicyManaged() {
  return IsMetricsReportingPolicyManaged();
}

metrics::EnableMetricsDefault
ChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      g_browser_process->local_state());
}

void ChromeMetricsServiceClient::Initialize() {
  PrefService* local_state = g_browser_process->local_state();

  // Clear deprecated metrics preference for Android.
  // TODO(gayane): Cleanup this code after M60 when the pref would be cleared
  // from clients.
#if BUILDFLAG(ANDROID_JAVA_UI)
  local_state->ClearPref(prefs::kCrashReportingEnabled);
#endif

  metrics_service_.reset(
      new metrics::MetricsService(metrics_state_manager_, this, local_state));

  // Gets access to persistent metrics shared by sub-processes.
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new SubprocessMetricsProvider()));

  // Register metrics providers.
#if BUILDFLAG(ENABLE_EXTENSIONS)
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
          base::Bind(&chrome::IsIncognitoSessionActive))));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new ChromeStabilityMetricsProvider(local_state)));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::GPUMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::ScreenInfoMetricsProvider));

  metrics_service_->RegisterMetricsProvider(CreateFileMetricsProvider(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()));

  drive_metrics_provider_ = new metrics::DriveMetricsProvider(
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::FILE),
      chrome::FILE_LOCAL_STATE);
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(drive_metrics_provider_));

  profiler_metrics_provider_ = new metrics::ProfilerMetricsProvider();
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(profiler_metrics_provider_));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::CallStackProfileMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new metrics::SamplingMetricsProvider));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new translate::TranslateRankerMetricsProvider()));

#if BUILDFLAG(ANDROID_JAVA_UI)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new AndroidMetricsProvider(local_state)));
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(new PageLoadMetricsProvider()));
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

#if defined(OS_WIN)
  google_update_metrics_provider_ = new GoogleUpdateMetricsProviderWin;
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          google_update_metrics_provider_));

  base::FilePath user_data_dir;
  base::FilePath crash_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir) ||
      !base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir)) {
    // If either call fails, then clear both.
    user_data_dir = base::FilePath();
    crash_dir = base::FilePath();
  }
  watcher_metrics_provider_ = new browser_watcher::WatcherMetricsProviderWin(
      chrome::GetBrowserExitCodesRegistryPath(), user_data_dir, crash_dir,
      base::Bind(&GetExecutableVersionDetails),
      content::BrowserThread::GetBlockingPool());
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(watcher_metrics_provider_));

  antivirus_metrics_provider_ = new AntiVirusMetricsProvider(
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(antivirus_metrics_provider_));
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_PLUGINS)
  plugin_metrics_provider_ = new PluginMetricsProvider(local_state);
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(plugin_metrics_provider_));
#endif  // BUILDFLAG(ENABLE_PLUGINS)

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

  // Record default UMA state as opt-out for all Chrome OS users, if not
  // recorded yet.
  if (metrics::GetMetricsReportingDefaultState(local_state) ==
      metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
    metrics::RecordMetricsReportingDefaultState(
        local_state, metrics::EnableMetricsDefault::OPT_OUT);
  }
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          SigninStatusMetricsProvider::CreateInstance(base::WrapUnique(
              new ChromeSigninStatusMetricsProviderDelegate))));
#endif  // !defined(OS_CHROMEOS)

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new syncer::DeviceCountMetricsProvider(base::Bind(
              &browser_sync::ChromeSyncClient::GetDeviceInfoTrackers))));

  metrics_service_->RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(
          new HttpsEngagementMetricsProvider()));
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

void ChromeMetricsServiceClient::MergeHistogramDeltas() {
  DCHECK(GetMetricsService());
  GetMetricsService()->MergeHistogramDeltas();
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

#if !BUILDFLAG(ENABLE_PRINT_PREVIEW)
  num_async_histogram_fetches_in_progress_ = 2;
#else   // !ENABLE_PRINT_PREVIEW
  num_async_histogram_fetches_in_progress_ = 3;
  // Run requests to service and content in parallel.
  if (!ServiceProcessControl::GetInstance()->GetHistograms(callback, timeout)) {
    // Assume |num_async_histogram_fetches_in_progress_| is not changed by
    // |GetHistograms()|.
    DCHECK_EQ(num_async_histogram_fetches_in_progress_, 3);
    // Assign |num_async_histogram_fetches_in_progress_| above and decrement it
    // here to make code work even if |GetHistograms()| fired |callback|.
    --num_async_histogram_fetches_in_progress_;
  }
#endif  // !ENABLE_PRINT_PREVIEW

  // Merge histograms from metrics providers into StatisticsRecorder.
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ChromeMetricsServiceClient::MergeHistogramDeltas,
                 weak_ptr_factory_.GetWeakPtr()),
      callback);

  // Set up the callback task to call after we receive histograms from all
  // child processes. |timeout| specifies how long to wait before absolutely
  // calling us back on the task.
  content::FetchHistogramsAsynchronously(base::ThreadTaskRunnerHandle::Get(),
                                         callback, timeout);
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
  return metrics::IsCellularLogicEnabled();
}
