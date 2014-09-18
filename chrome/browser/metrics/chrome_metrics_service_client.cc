// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"
#include "chrome/browser/metrics/omnibox_metrics_provider.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "components/metrics/gpu/gpu_metrics_provider.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"

#if defined(OS_ANDROID)
#include "chrome/browser/metrics/android_metrics_provider.h"
#endif

#if defined(ENABLE_FULL_PRINTING)
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
#endif

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/registry.h"
#include "chrome/browser/metrics/google_update_metrics_provider_win.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
#include "chrome/browser/metrics/signin_status_metrics_provider.h"
#endif

namespace {

// This specifies the amount of time to wait for all renderers to send their
// data.
const int kMaxHistogramGatheringWaitDuration = 60000;  // 60 seconds.

metrics::SystemProfileProto::Channel AsProtobufChannel(
    chrome::VersionInfo::Channel channel) {
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return metrics::SystemProfileProto::CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return metrics::SystemProfileProto::CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return metrics::SystemProfileProto::CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return metrics::SystemProfileProto::CHANNEL_STABLE;
  }
  NOTREACHED();
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

// Handles asynchronous fetching of memory details.
// Will run the provided task after finished.
class MetricsMemoryDetails : public MemoryDetails {
 public:
  MetricsMemoryDetails(
      const base::Closure& callback,
      MemoryGrowthTracker* memory_growth_tracker)
      : callback_(callback) {
    SetMemoryGrowthTracker(memory_growth_tracker);
  }

  virtual void OnDetailsAvailable() OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE, callback_);
  }

 private:
  virtual ~MetricsMemoryDetails() {}

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(MetricsMemoryDetails);
};

}  // namespace

ChromeMetricsServiceClient::ChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
      chromeos_metrics_provider_(NULL),
      waiting_for_collect_final_metrics_step_(false),
      num_async_histogram_fetches_in_progress_(0),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RecordCommandLineMetrics();
  RegisterForNotifications();

#if defined(OS_WIN)
  CountBrowserCrashDumpAttempts();
#endif  // defined(OS_WIN)
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
scoped_ptr<ChromeMetricsServiceClient> ChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager,
    PrefService* local_state) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  scoped_ptr<ChromeMetricsServiceClient> client(
      new ChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client.Pass();
}

// static
void ChromeMetricsServiceClient::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kUninstallLastLaunchTimeSec, 0);
  registry->RegisterInt64Pref(prefs::kUninstallLastObservedRunTimeSec, 0);

  metrics::MetricsService::RegisterPrefs(registry);
  ChromeStabilityMetricsProvider::RegisterPrefs(registry);

#if defined(OS_ANDROID)
  AndroidMetricsProvider::RegisterPrefs(registry);
#endif  // defined(OS_ANDROID)

#if defined(ENABLE_PLUGINS)
  PluginMetricsProvider::RegisterPrefs(registry);
#endif  // defined(ENABLE_PLUGINS)
}

void ChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetCrashClientIdFromGUID(client_id);
}

bool ChromeMetricsServiceClient::IsOffTheRecordSessionActive() {
  return chrome::IsOffTheRecordSessionActive();
}

std::string ChromeMetricsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return AsProtobufChannel(chrome::VersionInfo::GetChannel());
}

std::string ChromeMetricsServiceClient::GetVersionString() {
  chrome::VersionInfo version_info;
  if (!version_info.is_valid()) {
    NOTREACHED();
    return std::string();
  }

  std::string version = version_info.Version();
#if defined(ARCH_CPU_64_BITS)
  version += "-64";
#endif  // defined(ARCH_CPU_64_BITS)
  if (!version_info.IsOfficialBuild())
    version.append("-devel");
  return version;
}

void ChromeMetricsServiceClient::OnLogUploadComplete() {
  // Collect network stats after each UMA upload.
  network_stats_uploader_.CollectAndReportNetworkStats();
}

void ChromeMetricsServiceClient::StartGatheringMetrics(
    const base::Closure& done_callback) {
  finished_gathering_initial_metrics_callback_ = done_callback;
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

void ChromeMetricsServiceClient::CollectFinalMetrics(
    const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = done_callback;

  // Begin the multi-step process of collecting memory usage histograms:
  // First spawn a task to collect the memory details; when that task is
  // finished, it will call OnMemoryDetailCollectionDone. That will in turn
  // call HistogramSynchronization to collect histograms from all renderers and
  // then call OnHistogramSynchronizationDone to continue processing.
  DCHECK(!waiting_for_collect_final_metrics_step_);
  waiting_for_collect_final_metrics_step_ = true;

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
  // Record the signin status histogram value.
  signin_status_metrics_provider_->RecordSigninStatusHistogram();
#endif

  base::Closure callback =
      base::Bind(&ChromeMetricsServiceClient::OnMemoryDetailCollectionDone,
                 weak_ptr_factory_.GetWeakPtr());

  scoped_refptr<MetricsMemoryDetails> details(
      new MetricsMemoryDetails(callback, &memory_growth_tracker_));
  details->StartFetch(MemoryDetails::UPDATE_USER_METRICS);

  // Collect WebCore cache information to put into a histogram.
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new ChromeViewMsg_GetCacheResourceStats());
  }
}

scoped_ptr<metrics::MetricsLogUploader>
ChromeMetricsServiceClient::CreateUploader(
    const std::string& server_url,
    const std::string& mime_type,
    const base::Callback<void(int)>& on_upload_complete) {
  return scoped_ptr<metrics::MetricsLogUploader>(
      new metrics::NetMetricsLogUploader(
          g_browser_process->system_request_context(), server_url, mime_type,
          on_upload_complete));
}

base::string16 ChromeMetricsServiceClient::GetRegistryBackupKey() {
#if defined(OS_WIN)
  return L"Software\\" PRODUCT_STRING_PATH L"\\StabilityMetrics";
#else
  return base::string16();
#endif
}

void ChromeMetricsServiceClient::LogPluginLoadingError(
    const base::FilePath& plugin_path) {
#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_->LogPluginLoadingError(plugin_path);
#else
  NOTREACHED();
#endif  // defined(ENABLE_PLUGINS)
}

void ChromeMetricsServiceClient::Initialize() {
  metrics_service_.reset(new metrics::MetricsService(
      metrics_state_manager_, this, g_browser_process->local_state()));

  // Register metrics providers.
#if defined(ENABLE_EXTENSIONS)
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new ExtensionsMetricsProvider(metrics_state_manager_)));
#endif
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new NetworkMetricsProvider(
          content::BrowserThread::GetBlockingPool())));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new OmniboxMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new ChromeStabilityMetricsProvider));
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(new metrics::GPUMetricsProvider()));
  profiler_metrics_provider_ = new metrics::ProfilerMetricsProvider;
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(profiler_metrics_provider_));

#if defined(OS_ANDROID)
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(
          new AndroidMetricsProvider(g_browser_process->local_state())));
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
  google_update_metrics_provider_ = new GoogleUpdateMetricsProviderWin;
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(google_update_metrics_provider_));
#endif  // defined(OS_WIN)

#if defined(ENABLE_PLUGINS)
  plugin_metrics_provider_ =
      new PluginMetricsProvider(g_browser_process->local_state());
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(plugin_metrics_provider_));
#endif  // defined(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
  ChromeOSMetricsProvider* chromeos_metrics_provider =
      new ChromeOSMetricsProvider;
  chromeos_metrics_provider_ = chromeos_metrics_provider;
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(chromeos_metrics_provider));
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS) && !defined(OS_IOS)
  signin_status_metrics_provider_ =
      SigninStatusMetricsProvider::CreateInstance();
  metrics_service_->RegisterMetricsProvider(
      scoped_ptr<metrics::MetricsProvider>(signin_status_metrics_provider_));
#endif
}

void ChromeMetricsServiceClient::OnInitTaskGotHardwareClass() {
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
  // Start the next part of the init task: fetching performance data.  This will
  // call into |FinishedReceivingProfilerData()| when the task completes.
  metrics::TrackingSynchronizer::FetchProfilerDataAsynchronously(
      weak_ptr_factory_.GetWeakPtr());
}

void ChromeMetricsServiceClient::ReceivedProfilerData(
    const tracked_objects::ProcessDataSnapshot& process_data,
    int process_type) {
  profiler_metrics_provider_->RecordProfilerData(process_data, process_type);
}

void ChromeMetricsServiceClient::FinishedReceivingProfilerData() {
  finished_gathering_initial_metrics_callback_.Run();
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

#if !defined(ENABLE_FULL_PRINTING)
  num_async_histogram_fetches_in_progress_ = 1;
#else  // !ENABLE_FULL_PRINTING
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
#endif  // !ENABLE_FULL_PRINTING

  // Set up the callback to task to call after we receive histograms from all
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
  registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                 content::NotificationService::AllSources());
}

void ChromeMetricsServiceClient::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED:
    case chrome::NOTIFICATION_BROWSER_CLOSED:
    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL:
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

#if defined(OS_WIN)
void ChromeMetricsServiceClient::CountBrowserCrashDumpAttempts() {
  // Open the registry key for iteration.
  base::win::RegKey regkey;
  if (regkey.Open(HKEY_CURRENT_USER,
                  chrome::kBrowserCrashDumpAttemptsRegistryPath,
                  KEY_ALL_ACCESS) != ERROR_SUCCESS) {
    return;
  }

  // The values we're interested in counting are all prefixed with the version.
  base::string16 chrome_version(base::ASCIIToUTF16(chrome::kChromeVersion));

  // Track a list of values to delete. We don't modify the registry key while
  // we're iterating over its values.
  typedef std::vector<base::string16> StringVector;
  StringVector to_delete;

  // Iterate over the values in the key counting dumps with and without crashes.
  // We directly walk the values instead of using RegistryValueIterator in order
  // to read all of the values as DWORDS instead of strings.
  base::string16 name;
  DWORD value = 0;
  int dumps_with_crash = 0;
  int dumps_with_no_crash = 0;
  for (int i = regkey.GetValueCount() - 1; i >= 0; --i) {
    if (regkey.GetValueNameAt(i, &name) == ERROR_SUCCESS &&
        StartsWith(name, chrome_version, false) &&
        regkey.ReadValueDW(name.c_str(), &value) == ERROR_SUCCESS) {
      to_delete.push_back(name);
      if (value == 0)
        ++dumps_with_no_crash;
      else
        ++dumps_with_crash;
    }
  }

  // Delete the registry keys we've just counted.
  for (StringVector::iterator i = to_delete.begin(); i != to_delete.end(); ++i)
    regkey.DeleteValue(i->c_str());

  // Capture the histogram samples.
  if (dumps_with_crash != 0)
    UMA_HISTOGRAM_COUNTS("Chrome.BrowserDumpsWithCrash", dumps_with_crash);
  if (dumps_with_no_crash != 0)
    UMA_HISTOGRAM_COUNTS("Chrome.BrowserDumpsWithNoCrash", dumps_with_no_crash);
  int total_dumps = dumps_with_crash + dumps_with_no_crash;
  if (total_dumps != 0)
    UMA_HISTOGRAM_COUNTS("Chrome.BrowserCrashDumpAttempts", total_dumps);
}
#endif  // defined(OS_WIN)
