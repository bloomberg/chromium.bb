// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_impl.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/remote_debugging_server.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/gpu/gl_string_manager.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/metrics_services_manager.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/crl_set_fetcher.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/omaha_query_params/chrome_omaha_query_params_delegate.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/apps/chrome_app_window_client.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/switch_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/component_updater/component_updater_service.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/metrics/metrics_service.h"
#include "components/network_time/network_time_tracker.h"
#include "components/omaha_query_params/omaha_query_params.h"
#include "components/policy/core/common/policy_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_l10n_util.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/views/focus/view_storage.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_main_mac.h"
#endif

#if defined(OS_ANDROID)
#include "components/gcm_driver/gcm_driver_android.h"
#else
#include "chrome/browser/chrome_device_client.h"
#include "chrome/browser/services/gcm/gcm_desktop_utils.h"
#include "components/gcm_driver/gcm_client_factory.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "components/policy/core/browser/browser_policy_connector.h"
#else
#include "components/policy/core/common/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "components/storage_monitor/storage_monitor.h"
#endif

#if !defined(DISABLE_NACL)
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#endif

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugins_resource_service.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "chrome/browser/media/webrtc_log_uploader.h"
#endif

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
// How often to check if the persistent instance of Chrome needs to restart
// to install an update.
static const int kUpdateCheckIntervalHours = 6;
#endif

#if defined(USE_X11) || defined(OS_WIN)
// How long to wait for the File thread to complete during EndSession, on Linux
// and Windows. We have a timeout here because we're unable to run the UI
// messageloop and there's some deadlock risk. Our only option is to exit
// anyway.
static const int kEndSessionTimeoutSeconds = 10;
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::PluginService;
using content::ResourceDispatcherHost;

BrowserProcessImpl::BrowserProcessImpl(
    base::SequencedTaskRunner* local_state_task_runner,
    const CommandLine& command_line)
    : created_watchdog_thread_(false),
      created_browser_policy_connector_(false),
      created_profile_manager_(false),
      created_local_state_(false),
      created_icon_manager_(false),
      created_notification_ui_manager_(false),
      created_safe_browsing_service_(false),
      module_ref_count_(0),
      did_start_(false),
      download_status_updater_(new DownloadStatusUpdater),
      local_state_task_runner_(local_state_task_runner) {
  g_browser_process = this;
  platform_part_.reset(new BrowserProcessPlatformPart());

#if defined(ENABLE_PRINTING)
  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager);
#endif

  net_log_.reset(new ChromeNetLog);

  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      extensions::kExtensionScheme);
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      extensions::kExtensionResourceScheme);
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      chrome::kChromeSearchScheme);

#if defined(OS_MACOSX)
  InitIdleMonitor();
#endif

#if !defined(OS_ANDROID)
  device_client_.reset(new ChromeDeviceClient);
#endif

#if defined(ENABLE_EXTENSIONS)
#if !defined(USE_ATHENA)
  // Athena sets its own instance during Athena's init process.
  extensions::AppWindowClient::Set(ChromeAppWindowClient::GetInstance());
#endif

  extension_event_router_forwarder_ = new extensions::EventRouterForwarder;
  ExtensionRendererState::GetInstance()->Init();

  extensions::ExtensionsClient::Set(
      extensions::ChromeExtensionsClient::GetInstance());

  extensions_browser_client_.reset(
      new extensions::ChromeExtensionsBrowserClient);
  extensions::ExtensionsBrowserClient::Set(extensions_browser_client_.get());
#endif

  message_center::MessageCenter::Initialize();

  omaha_query_params::OmahaQueryParams::SetDelegate(
      ChromeOmahaQueryParamsDelegate::GetInstance());
}

BrowserProcessImpl::~BrowserProcessImpl() {
  tracked_objects::ThreadData::EnsureCleanupWasCalled(4);

  g_browser_process = NULL;
}

void BrowserProcessImpl::StartTearDown() {
    TRACE_EVENT0("shutdown", "BrowserProcessImpl::StartTearDown");
  // We need to destroy the MetricsServicesManager, IntranetRedirectDetector,
  // PromoResourceService, and SafeBrowsing ClientSideDetectionService (owned by
  // the SafeBrowsingService) before the io_thread_ gets destroyed, since their
  // destructors can call the URLFetcher destructor, which does a
  // PostDelayedTask operation on the IO thread. (The IO thread will handle that
  // URLFetcher operation before going away.)
  metrics_services_manager_.reset();
  intranet_redirect_detector_.reset();
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  if (safe_browsing_service_.get())
    safe_browsing_service()->ShutDown();
#endif

  // Need to clear the desktop notification balloons before the io_thread_ and
  // before the profiles, since if there are any still showing we will access
  // those things during teardown.
  notification_ui_manager_.reset();

  // Need to clear profiles (download managers) before the io_thread_.
  {
    TRACE_EVENT0("shutdown",
                 "BrowserProcessImpl::StartTearDown:ProfileManager");
    // The desktop User Manager needs to be closed before the guest profile
    // can be destroyed.
    if (switches::IsNewAvatarMenu())
      UserManager::Hide();
    profile_manager_.reset();
  }

#if !defined(OS_ANDROID)
  // Debugger must be cleaned up before IO thread and NotificationService.
  remote_debugging_server_.reset();
#endif

#if defined(ENABLE_EXTENSIONS)
  ExtensionRendererState::GetInstance()->Shutdown();

  media_file_system_registry_.reset();
  // Remove the global instance of the Storage Monitor now. Otherwise the
  // FILE thread would be gone when we try to release it in the dtor and
  // Valgrind would report a leak on almost every single browser_test.
  // TODO(gbillock): Make this unnecessary.
  storage_monitor::StorageMonitor::Destroy();
#endif

  message_center::MessageCenter::Shutdown();

#if defined(ENABLE_CONFIGURATION_POLICY)
  // The policy providers managed by |browser_policy_connector_| need to shut
  // down while the IO and FILE threads are still alive.
  if (browser_policy_connector_)
    browser_policy_connector_->Shutdown();
#endif

  // The |gcm_driver_| must shut down while the IO thread is still alive.
  if (gcm_driver_)
    gcm_driver_->Shutdown();

  // Stop the watchdog thread before stopping other threads.
  watchdog_thread_.reset();

#if defined(USE_AURA)
  // Delete aura after the metrics service has been deleted as it accesses
  // monitor information.
  aura::Env::DeleteInstance();
#endif

  platform_part()->StartTearDown();

#if defined(ENABLE_WEBRTC)
  // Cancel any uploads to release the system url request context references.
  if (webrtc_log_uploader_)
    webrtc_log_uploader_->StartShutdown();
#endif

  if (local_state())
    local_state()->CommitPendingWrite();
}

void BrowserProcessImpl::PostDestroyThreads() {
  // With the file_thread_ flushed, we can release any icon resources.
  icon_manager_.reset();

#if defined(ENABLE_WEBRTC)
  // Must outlive the file thread.
  webrtc_log_uploader_.reset();
#endif

  // Reset associated state right after actual thread is stopped,
  // as io_thread_.global_ cleanup happens in CleanUp on the IO
  // thread, i.e. as the thread exits its message loop.
  //
  // This is important also because in various places, the
  // IOThread object being NULL is considered synonymous with the
  // IO thread having stopped.
  io_thread_.reset();
}

unsigned int BrowserProcessImpl::AddRefModule() {
  DCHECK(CalledOnValidThread());

  // CHECK(!IsShuttingDown());
  if (IsShuttingDown()) {
    // Copy the stacktrace which released the final reference onto our stack so
    // it will be available in the crash report for inspection.
    base::debug::StackTrace callstack = release_last_reference_callstack_;
    base::debug::Alias(&callstack);
    CHECK(false);
  }

  did_start_ = true;
  module_ref_count_++;
  return module_ref_count_;
}

static void ShutdownServiceWorkerContext(content::StoragePartition* partition) {
  partition->GetServiceWorkerContext()->Terminate();
}

unsigned int BrowserProcessImpl::ReleaseModule() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(0u, module_ref_count_);
  module_ref_count_--;
  if (0 == module_ref_count_) {
    release_last_reference_callstack_ = base::debug::StackTrace();

    // Stop service workers
    ProfileManager* pm = profile_manager();
    std::vector<Profile*> profiles(pm->GetLoadedProfiles());
    for (size_t i = 0; i < profiles.size(); ++i) {
      content::BrowserContext::ForEachStoragePartition(
          profiles[i], base::Bind(ShutdownServiceWorkerContext));
    }

#if defined(ENABLE_PRINTING)
    // Wait for the pending print jobs to finish. Don't do this later, since
    // this might cause a nested message loop to run, and we don't want pending
    // tasks to run once teardown has started.
    print_job_manager_->Shutdown();
#endif

#if defined(LEAK_SANITIZER)
    // Check for memory leaks now, before we start shutting down threads. Doing
    // this early means we won't report any shutdown-only leaks (as they have
    // not yet happened at this point).
    // If leaks are found, this will make the process exit immediately.
    __lsan_do_leak_check();
#endif

    CHECK(base::MessageLoop::current()->is_running());

#if defined(OS_MACOSX)
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(ChromeBrowserMainPartsMac::DidEndMainMessageLoop));
#endif
    base::MessageLoop::current()->Quit();
  }
  return module_ref_count_;
}

namespace {

// Used at the end of session to block the UI thread for completion of sentinel
// tasks on the set of threads used to persist profile data and local state.
// This is done to ensure that the data has been persisted to disk before
// continuing.
class RundownTaskCounter :
    public base::RefCountedThreadSafe<RundownTaskCounter> {
 public:
  RundownTaskCounter();

  // Posts a rundown task to |task_runner|, can be invoked an arbitrary number
  // of times before calling TimedWait.
  void Post(base::SequencedTaskRunner* task_runner);

  // Waits until the count is zero or |max_time| has passed.
  // This can only be called once per instance.
  bool TimedWait(const base::TimeDelta& max_time);

 private:
  friend class base::RefCountedThreadSafe<RundownTaskCounter>;
  ~RundownTaskCounter() {}

  // Decrements the counter and releases the waitable event on transition to
  // zero.
  void Decrement();

  // The count starts at one to defer the possibility of one->zero transitions
  // until TimedWait is called.
  base::AtomicRefCount count_;
  base::WaitableEvent waitable_event_;

  DISALLOW_COPY_AND_ASSIGN(RundownTaskCounter);
};

RundownTaskCounter::RundownTaskCounter()
    : count_(1), waitable_event_(true, false) {
}

void RundownTaskCounter::Post(base::SequencedTaskRunner* task_runner) {
  // As the count starts off at one, it should never get to zero unless
  // TimedWait has been called.
  DCHECK(!base::AtomicRefCountIsZero(&count_));

  base::AtomicRefCountInc(&count_);

  // The task must be non-nestable to guarantee that it runs after all tasks
  // currently scheduled on |task_runner| have completed.
  task_runner->PostNonNestableTask(FROM_HERE,
      base::Bind(&RundownTaskCounter::Decrement, this));
}

void RundownTaskCounter::Decrement() {
  if (!base::AtomicRefCountDec(&count_))
    waitable_event_.Signal();
}

bool RundownTaskCounter::TimedWait(const base::TimeDelta& max_time) {
  // Decrement the excess count from the constructor.
  Decrement();

  return waitable_event_.TimedWait(max_time);
}

bool ExperimentUseBrokenSynchronization() {
  // The logoff behavior used to have a race, whereby it would perform profile
  // IO writes on the blocking thread pool, but would sycnhronize to the FILE
  // thread. Windows feels free to terminate any process that's hidden or
  // destroyed all it's windows, and sometimes Chrome would be terminated
  // with pending profile IO due to this mis-synchronization.
  // Under the "WindowsLogoffRace" experiment group, the broken behavior is
  // emulated, in order to allow measuring what fraction of unclean shutdowns
  // are due to this bug.
  const std::string group_name =
      base::FieldTrialList::FindFullName("WindowsLogoffRace");
  return group_name == "BrokenSynchronization";
}

}  // namespace

void BrowserProcessImpl::EndSession() {
  bool use_broken_synchronization = ExperimentUseBrokenSynchronization();

  // Mark all the profiles as clean.
  ProfileManager* pm = profile_manager();
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  scoped_refptr<RundownTaskCounter> rundown_counter(new RundownTaskCounter());
  for (size_t i = 0; i < profiles.size(); ++i) {
    Profile* profile = profiles[i];
    profile->SetExitType(Profile::EXIT_SESSION_ENDED);

    if (!use_broken_synchronization)
      rundown_counter->Post(profile->GetIOTaskRunner().get());
  }

  // Tell the metrics service it was cleanly shutdown.
  metrics::MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics && local_state()) {
    metrics->RecordStartOfSessionEnd();
#if !defined(OS_CHROMEOS)
    // MetricsService lazily writes to prefs, force it to write now.
    // On ChromeOS, chrome gets killed when hangs, so no need to
    // commit metrics::prefs::kStabilitySessionEndCompleted change immediately.
    local_state()->CommitPendingWrite();

    if (!use_broken_synchronization)
      rundown_counter->Post(local_state_task_runner_.get());
#endif
  }

  // http://crbug.com/125207
  base::ThreadRestrictions::ScopedAllowWait allow_wait;

  // We must write that the profile and metrics service shutdown cleanly,
  // otherwise on startup we'll think we crashed. So we block until done and
  // then proceed with normal shutdown.
  //
  // If you change the condition here, be sure to also change
  // ProfileBrowserTests to match.
#if defined(USE_X11) || defined(OS_WIN)
  if (use_broken_synchronization) {
    rundown_counter->Post(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE).get());
  }

  // Do a best-effort wait on the successful countdown of rundown tasks. Note
  // that if we don't complete "quickly enough", Windows will terminate our
  // process.
  //
  // On Windows, we previously posted a message to FILE and then ran a nested
  // message loop, waiting for that message to be processed until quitting.
  // However, doing so means that other messages will also be processed. In
  // particular, if the GPU process host notices that the GPU has been killed
  // during shutdown, it races exiting the nested loop with the process host
  // blocking the message loop attempting to re-establish a connection to the
  // GPU process synchronously. Because the system may not be allowing
  // processes to launch, this can result in a hang. See
  // http://crbug.com/318527.
  rundown_counter->TimedWait(
      base::TimeDelta::FromSeconds(kEndSessionTimeoutSeconds));
#else
  NOTIMPLEMENTED();
#endif
}

MetricsServicesManager* BrowserProcessImpl::GetMetricsServicesManager() {
  DCHECK(CalledOnValidThread());
  if (!metrics_services_manager_)
    metrics_services_manager_.reset(new MetricsServicesManager(local_state()));
  return metrics_services_manager_.get();
}

metrics::MetricsService* BrowserProcessImpl::metrics_service() {
  DCHECK(CalledOnValidThread());
  return GetMetricsServicesManager()->GetMetricsService();
}

rappor::RapporService* BrowserProcessImpl::rappor_service() {
  DCHECK(CalledOnValidThread());
  return GetMetricsServicesManager()->GetRapporService();
}

IOThread* BrowserProcessImpl::io_thread() {
  DCHECK(CalledOnValidThread());
  DCHECK(io_thread_.get());
  return io_thread_.get();
}

WatchDogThread* BrowserProcessImpl::watchdog_thread() {
  DCHECK(CalledOnValidThread());
  if (!created_watchdog_thread_)
    CreateWatchdogThread();
  DCHECK(watchdog_thread_.get() != NULL);
  return watchdog_thread_.get();
}

ProfileManager* BrowserProcessImpl::profile_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_profile_manager_)
    CreateProfileManager();
  return profile_manager_.get();
}

PrefService* BrowserProcessImpl::local_state() {
  DCHECK(CalledOnValidThread());
  if (!created_local_state_)
    CreateLocalState();
  return local_state_.get();
}

net::URLRequestContextGetter* BrowserProcessImpl::system_request_context() {
  DCHECK(CalledOnValidThread());
  return io_thread()->system_url_request_context_getter();
}

chrome_variations::VariationsService* BrowserProcessImpl::variations_service() {
  DCHECK(CalledOnValidThread());
  return GetMetricsServicesManager()->GetVariationsService();
}

BrowserProcessPlatformPart* BrowserProcessImpl::platform_part() {
  return platform_part_.get();
}

extensions::EventRouterForwarder*
BrowserProcessImpl::extension_event_router_forwarder() {
#if defined(ENABLE_EXTENSIONS)
  return extension_event_router_forwarder_.get();
#else
  return NULL;
#endif
}

NotificationUIManager* BrowserProcessImpl::notification_ui_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_notification_ui_manager_)
    CreateNotificationUIManager();
  return notification_ui_manager_.get();
}

message_center::MessageCenter* BrowserProcessImpl::message_center() {
  DCHECK(CalledOnValidThread());
  return message_center::MessageCenter::Get();
}

policy::BrowserPolicyConnector* BrowserProcessImpl::browser_policy_connector() {
  DCHECK(CalledOnValidThread());
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (!created_browser_policy_connector_) {
    DCHECK(!browser_policy_connector_);
    browser_policy_connector_ = platform_part_->CreateBrowserPolicyConnector();
    created_browser_policy_connector_ = true;
  }
  return browser_policy_connector_.get();
#else
  return NULL;
#endif
}

policy::PolicyService* BrowserProcessImpl::policy_service() {
#if defined(ENABLE_CONFIGURATION_POLICY)
  return browser_policy_connector()->GetPolicyService();
#else
  if (!policy_service_.get())
    policy_service_.reset(new policy::PolicyServiceStub());
  return policy_service_.get();
#endif
}

IconManager* BrowserProcessImpl::icon_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_icon_manager_)
    CreateIconManager();
  return icon_manager_.get();
}

GLStringManager* BrowserProcessImpl::gl_string_manager() {
  DCHECK(CalledOnValidThread());
  if (!gl_string_manager_.get())
    gl_string_manager_.reset(new GLStringManager());
  return gl_string_manager_.get();
}

GpuModeManager* BrowserProcessImpl::gpu_mode_manager() {
  DCHECK(CalledOnValidThread());
  if (!gpu_mode_manager_.get())
    gpu_mode_manager_.reset(new GpuModeManager());
  return gpu_mode_manager_.get();
}

void BrowserProcessImpl::CreateDevToolsHttpProtocolHandler(
    chrome::HostDesktopType host_desktop_type,
    const std::string& ip,
    int port) {
  DCHECK(CalledOnValidThread());
#if !defined(OS_ANDROID)
  // StartupBrowserCreator::LaunchBrowser can be run multiple times when browser
  // is started with several profiles or existing browser process is reused.
  if (!remote_debugging_server_.get()) {
    remote_debugging_server_.reset(
        new RemoteDebuggingServer(host_desktop_type, ip, port));
  }
#endif
}

bool BrowserProcessImpl::IsShuttingDown() {
  DCHECK(CalledOnValidThread());
  return did_start_ && 0 == module_ref_count_;
}

printing::PrintJobManager* BrowserProcessImpl::print_job_manager() {
  DCHECK(CalledOnValidThread());
  return print_job_manager_.get();
}

printing::PrintPreviewDialogController*
    BrowserProcessImpl::print_preview_dialog_controller() {
#if defined(ENABLE_FULL_PRINTING)
  DCHECK(CalledOnValidThread());
  if (!print_preview_dialog_controller_.get())
    CreatePrintPreviewDialogController();
  return print_preview_dialog_controller_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

printing::BackgroundPrintingManager*
    BrowserProcessImpl::background_printing_manager() {
#if defined(ENABLE_FULL_PRINTING)
  DCHECK(CalledOnValidThread());
  if (!background_printing_manager_.get())
    CreateBackgroundPrintingManager();
  return background_printing_manager_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

IntranetRedirectDetector* BrowserProcessImpl::intranet_redirect_detector() {
  DCHECK(CalledOnValidThread());
  if (!intranet_redirect_detector_.get())
    CreateIntranetRedirectDetector();
  return intranet_redirect_detector_.get();
}

const std::string& BrowserProcessImpl::GetApplicationLocale() {
  DCHECK(!locale_.empty());
  return locale_;
}

void BrowserProcessImpl::SetApplicationLocale(const std::string& locale) {
  locale_ = locale;
  extension_l10n_util::SetProcessLocale(locale);
  chrome::ChromeContentBrowserClient::SetApplicationLocale(locale);
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      locale);
}

DownloadStatusUpdater* BrowserProcessImpl::download_status_updater() {
  return download_status_updater_.get();
}

MediaFileSystemRegistry* BrowserProcessImpl::media_file_system_registry() {
#if defined(ENABLE_EXTENSIONS)
  if (!media_file_system_registry_)
    media_file_system_registry_.reset(new MediaFileSystemRegistry());
  return media_file_system_registry_.get();
#else
  return NULL;
#endif
}

bool BrowserProcessImpl::created_local_state() const {
  return created_local_state_;
}

#if defined(ENABLE_WEBRTC)
WebRtcLogUploader* BrowserProcessImpl::webrtc_log_uploader() {
  if (!webrtc_log_uploader_.get())
    webrtc_log_uploader_.reset(new WebRtcLogUploader());
  return webrtc_log_uploader_.get();
}
#endif

network_time::NetworkTimeTracker* BrowserProcessImpl::network_time_tracker() {
  if (!network_time_tracker_) {
    network_time_tracker_.reset(new network_time::NetworkTimeTracker(
        scoped_ptr<base::TickClock>(new base::DefaultTickClock()),
        local_state()));
  }
  return network_time_tracker_.get();
}

gcm::GCMDriver* BrowserProcessImpl::gcm_driver() {
  DCHECK(CalledOnValidThread());
  if (!gcm_driver_)
    CreateGCMDriver();
  return gcm_driver_.get();
}

// static
void BrowserProcessImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDefaultBrowserSettingEnabled,
                                false);
  // This policy needs to be defined before the net subsystem is initialized,
  // so we do it here.
  registry->RegisterIntegerPref(prefs::kMaxConnectionsPerProxy,
                                net::kDefaultMaxSocketsPerProxyServer);

  registry->RegisterBooleanPref(prefs::kAllowCrossOriginAuthPrompt, false);

#if defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_IOS)
  registry->RegisterBooleanPref(prefs::kEulaAccepted, false);
#endif  // defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_IOS)
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN7) {
    registry->RegisterStringPref(prefs::kRelaunchMode,
                                 upgrade_util::kRelaunchModeDefault);
  }
#endif

  // TODO(brettw,*): this comment about ResourceBundle was here since
  // initial commit.  This comment seems unrelated, bit-rotten and
  // a candidate for removal.
  // Initialize ResourceBundle which handles files loaded from external
  // sources. This has to be done before uninstall code path and before prefs
  // are registered.
  registry->RegisterStringPref(prefs::kApplicationLocale, std::string());
#if defined(OS_CHROMEOS)
  registry->RegisterStringPref(prefs::kOwnerLocale, std::string());
  registry->RegisterStringPref(prefs::kHardwareKeyboardLayout,
                               std::string());
#endif  // defined(OS_CHROMEOS)
#if !defined(OS_CHROMEOS)
  registry->RegisterBooleanPref(
      prefs::kMetricsReportingEnabled,
      GoogleUpdateSettings::GetCollectStatsConsent());
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kCrashReportingEnabled, false);
#endif  // defined(OS_ANDROID)
}

DownloadRequestLimiter* BrowserProcessImpl::download_request_limiter() {
  DCHECK(CalledOnValidThread());
  if (!download_request_limiter_.get())
    download_request_limiter_ = new DownloadRequestLimiter();
  return download_request_limiter_.get();
}

BackgroundModeManager* BrowserProcessImpl::background_mode_manager() {
  DCHECK(CalledOnValidThread());
#if defined(ENABLE_BACKGROUND)
  if (!background_mode_manager_.get())
    CreateBackgroundModeManager();
  return background_mode_manager_.get();
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

void BrowserProcessImpl::set_background_mode_manager_for_test(
    scoped_ptr<BackgroundModeManager> manager) {
  background_mode_manager_ = manager.Pass();
}

StatusTray* BrowserProcessImpl::status_tray() {
  DCHECK(CalledOnValidThread());
  if (!status_tray_.get())
    CreateStatusTray();
  return status_tray_.get();
}


SafeBrowsingService* BrowserProcessImpl::safe_browsing_service() {
  DCHECK(CalledOnValidThread());
  if (!created_safe_browsing_service_)
    CreateSafeBrowsingService();
  return safe_browsing_service_.get();
}

safe_browsing::ClientSideDetectionService*
    BrowserProcessImpl::safe_browsing_detection_service() {
  DCHECK(CalledOnValidThread());
  if (safe_browsing_service())
    return safe_browsing_service()->safe_browsing_detection_service();
  return NULL;
}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
void BrowserProcessImpl::StartAutoupdateTimer() {
  autoupdate_timer_.Start(FROM_HERE,
      base::TimeDelta::FromHours(kUpdateCheckIntervalHours),
      this,
      &BrowserProcessImpl::OnAutoupdateTimer);
}
#endif

ChromeNetLog* BrowserProcessImpl::net_log() {
  return net_log_.get();
}

prerender::PrerenderTracker* BrowserProcessImpl::prerender_tracker() {
  if (!prerender_tracker_.get())
    prerender_tracker_.reset(new prerender::PrerenderTracker);

  return prerender_tracker_.get();
}

component_updater::ComponentUpdateService*
BrowserProcessImpl::component_updater() {
  if (!component_updater_.get()) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
      return NULL;
    component_updater::Configurator* configurator =
        component_updater::MakeChromeComponentUpdaterConfigurator(
            CommandLine::ForCurrentProcess(),
            io_thread()->system_url_request_context_getter());
    // Creating the component updater does not do anything, components
    // need to be registered and Start() needs to be called.
    component_updater_.reset(ComponentUpdateServiceFactory(configurator));
  }
  return component_updater_.get();
}

CRLSetFetcher* BrowserProcessImpl::crl_set_fetcher() {
  if (!crl_set_fetcher_.get())
    crl_set_fetcher_ = new CRLSetFetcher();
  return crl_set_fetcher_.get();
}

component_updater::PnaclComponentInstaller*
BrowserProcessImpl::pnacl_component_installer() {
#if !defined(DISABLE_NACL)
  if (!pnacl_component_installer_.get()) {
    pnacl_component_installer_.reset(
        new component_updater::PnaclComponentInstaller());
  }
  return pnacl_component_installer_.get();
#else
  return NULL;
#endif
}

void BrowserProcessImpl::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new ChromeResourceDispatcherHostDelegate(prerender_tracker()));
  ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());

  pref_change_registrar_.Add(
      prefs::kAllowCrossOriginAuthPrompt,
      base::Bind(&BrowserProcessImpl::ApplyAllowCrossOriginAuthPromptPolicy,
                 base::Unretained(this)));
  ApplyAllowCrossOriginAuthPromptPolicy();
}

void BrowserProcessImpl::CreateWatchdogThread() {
  DCHECK(!created_watchdog_thread_ && watchdog_thread_.get() == NULL);
  created_watchdog_thread_ = true;

  scoped_ptr<WatchDogThread> thread(new WatchDogThread());
  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  if (!thread->StartWithOptions(options))
    return;
  watchdog_thread_.swap(thread);
}

void BrowserProcessImpl::CreateProfileManager() {
  DCHECK(!created_profile_manager_ && profile_manager_.get() == NULL);
  created_profile_manager_ = true;

  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  profile_manager_.reset(new ProfileManager(user_data_dir));
}

void BrowserProcessImpl::CreateLocalState() {
  DCHECK(!created_local_state_ && local_state_.get() == NULL);
  created_local_state_ = true;

  base::FilePath local_state_path;
  CHECK(PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path));
  scoped_refptr<PrefRegistrySimple> pref_registry = new PrefRegistrySimple;

  // Register local state preferences.
  chrome::RegisterLocalState(pref_registry.get());

  local_state_ =
      chrome_prefs::CreateLocalState(local_state_path,
                                     local_state_task_runner_.get(),
                                     policy_service(),
                                     pref_registry,
                                     false).Pass();

  pref_change_registrar_.Init(local_state_.get());

  // Initialize the notification for the default browser setting policy.
  pref_change_registrar_.Add(
      prefs::kDefaultBrowserSettingEnabled,
      base::Bind(&BrowserProcessImpl::ApplyDefaultBrowserPolicy,
                 base::Unretained(this)));

  // This preference must be kept in sync with external values; update them
  // whenever the preference or its controlling policy changes.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  pref_change_registrar_.Add(
      prefs::kMetricsReportingEnabled,
      base::Bind(&BrowserProcessImpl::ApplyMetricsReportingPolicy,
                 base::Unretained(this)));
#endif

  int max_per_proxy = local_state_->GetInteger(prefs::kMaxConnectionsPerProxy);
  net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL,
      std::max(std::min(max_per_proxy, 99),
               net::ClientSocketPoolManager::max_sockets_per_group(
                   net::HttpNetworkSession::NORMAL_SOCKET_POOL)));
}

void BrowserProcessImpl::PreCreateThreads() {
  io_thread_.reset(
      new IOThread(local_state(), policy_service(), net_log_.get(),
                   extension_event_router_forwarder()));
}

void BrowserProcessImpl::PreMainMessageLoopRun() {
#if defined(ENABLE_CONFIGURATION_POLICY)
  // browser_policy_connector() is created very early because local_state()
  // needs policy to be initialized with the managed preference values.
  // However, policy fetches from the network and loading of disk caches
  // requires that threads are running; this Init() call lets the connector
  // resume its initialization now that the loops are spinning and the
  // system request context is available for the fetchers.
  browser_policy_connector()->Init(local_state(), system_request_context());
#endif

  if (local_state_->IsManagedPreference(prefs::kDefaultBrowserSettingEnabled))
    ApplyDefaultBrowserPolicy();

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  ApplyMetricsReportingPolicy();
#endif

#if defined(ENABLE_PLUGINS)
  PluginService* plugin_service = PluginService::GetInstance();
  plugin_service->SetFilter(ChromePluginServiceFilter::GetInstance());
  plugin_service->StartWatchingPlugins();

#if defined(OS_POSIX)
  // Also find plugins in a user-specific plugins dir,
  // e.g. ~/.config/chromium/Plugins.
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (!cmd_line.HasSwitch(switches::kDisablePluginsDiscovery)) {
    base::FilePath user_data_dir;
    if (PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
      plugin_service->AddExtraPluginDir(user_data_dir.Append("Plugins"));
  }
#endif

  // Triggers initialization of the singleton instance on UI thread.
  PluginFinder::GetInstance()->Init();

#if defined(ENABLE_PLUGIN_INSTALLATION)
  DCHECK(!plugins_resource_service_.get());
  plugins_resource_service_ = new PluginsResourceService(local_state());
  plugins_resource_service_->Init();
#endif
#endif  // defined(ENABLE_PLUGINS)

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableWebResources)) {
    DCHECK(!promo_resource_service_.get());
    promo_resource_service_ = new PromoResourceService;
    promo_resource_service_->StartAfterDelay();
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  storage_monitor::StorageMonitor::Create();
#endif

  platform_part_->PreMainMessageLoopRun();
}

void BrowserProcessImpl::CreateIconManager() {
  DCHECK(!created_icon_manager_ && icon_manager_.get() == NULL);
  created_icon_manager_ = true;
  icon_manager_.reset(new IconManager);
}

void BrowserProcessImpl::CreateIntranetRedirectDetector() {
  DCHECK(intranet_redirect_detector_.get() == NULL);
  scoped_ptr<IntranetRedirectDetector> intranet_redirect_detector(
      new IntranetRedirectDetector);
  intranet_redirect_detector_.swap(intranet_redirect_detector);
}

void BrowserProcessImpl::CreateNotificationUIManager() {
#if defined(ENABLE_NOTIFICATIONS)
  DCHECK(notification_ui_manager_.get() == NULL);
  notification_ui_manager_.reset(NotificationUIManager::Create(local_state()));
  created_notification_ui_manager_ = true;
#endif
}

void BrowserProcessImpl::CreateBackgroundModeManager() {
  DCHECK(background_mode_manager_.get() == NULL);
  background_mode_manager_.reset(
      new BackgroundModeManager(CommandLine::ForCurrentProcess(),
                                &profile_manager()->GetProfileInfoCache()));
}

void BrowserProcessImpl::CreateStatusTray() {
  DCHECK(status_tray_.get() == NULL);
  status_tray_.reset(StatusTray::Create());
}

void BrowserProcessImpl::CreatePrintPreviewDialogController() {
#if defined(ENABLE_FULL_PRINTING)
  DCHECK(print_preview_dialog_controller_.get() == NULL);
  print_preview_dialog_controller_ =
      new printing::PrintPreviewDialogController();
#else
  NOTIMPLEMENTED();
#endif
}

void BrowserProcessImpl::CreateBackgroundPrintingManager() {
#if defined(ENABLE_FULL_PRINTING)
  DCHECK(background_printing_manager_.get() == NULL);
  background_printing_manager_.reset(new printing::BackgroundPrintingManager());
#else
  NOTIMPLEMENTED();
#endif
}

void BrowserProcessImpl::CreateSafeBrowsingService() {
  DCHECK(safe_browsing_service_.get() == NULL);
  // Set this flag to true so that we don't retry indefinitely to
  // create the service class if there was an error.
  created_safe_browsing_service_ = true;
#if defined(FULL_SAFE_BROWSING) || defined(MOBILE_SAFE_BROWSING)
  safe_browsing_service_ = SafeBrowsingService::CreateSafeBrowsingService();
  safe_browsing_service_->Initialize();
#endif
}

void BrowserProcessImpl::CreateGCMDriver() {
  DCHECK(!gcm_driver_);

#if defined(OS_ANDROID)
  gcm_driver_.reset(new gcm::GCMDriverAndroid);
#else
  base::FilePath store_path;
  CHECK(PathService::Get(chrome::DIR_GLOBAL_GCM_STORE, &store_path));
  gcm_driver_ = gcm::CreateGCMDriverDesktop(
      make_scoped_ptr(new gcm::GCMClientFactory),
      local_state(),
      store_path,
      system_request_context());
  // Sign-in is not required for device-level GCM usage. So we just call
  // OnSignedIn to assume always signed-in. Note that GCM will not be started
  // at this point since no one has asked for it yet.
  // TODO(jianli): To be removed when sign-in enforcement is dropped.
  gcm_driver_->OnSignedIn();
#endif  // defined(OS_ANDROID)
}

void BrowserProcessImpl::ApplyDefaultBrowserPolicy() {
  if (local_state()->GetBoolean(prefs::kDefaultBrowserSettingEnabled)) {
    scoped_refptr<ShellIntegration::DefaultWebClientWorker>
        set_browser_worker = new ShellIntegration::DefaultBrowserWorker(NULL);
    set_browser_worker->StartSetAsDefault();
  }
}

void BrowserProcessImpl::ApplyAllowCrossOriginAuthPromptPolicy() {
  bool value = local_state()->GetBoolean(prefs::kAllowCrossOriginAuthPrompt);
  ResourceDispatcherHost::Get()->SetAllowCrossOriginAuthPrompt(value);
}

void BrowserProcessImpl::ApplyMetricsReportingPolicy() {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
  CHECK(BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          base::IgnoreResult(&GoogleUpdateSettings::SetCollectStatsConsent),
          local_state()->GetBoolean(prefs::kMetricsReportingEnabled))));
#endif
}

// Mac is currently not supported.
#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)

bool BrowserProcessImpl::CanAutorestartForUpdate() const {
  // Check if browser is in the background and if it needs to be restarted to
  // apply a pending update.
  return chrome::GetTotalBrowserCount() == 0 && chrome::WillKeepAlive() &&
         upgrade_util::IsUpdatePendingRestart();
}

// Switches to add when auto-restarting Chrome.
const char* const kSwitchesToAddOnAutorestart[] = {
  switches::kNoStartupWindow
};

void BrowserProcessImpl::RestartBackgroundInstance() {
  CommandLine* old_cl = CommandLine::ForCurrentProcess();
  scoped_ptr<CommandLine> new_cl(new CommandLine(old_cl->GetProgram()));

  std::map<std::string, CommandLine::StringType> switches =
      old_cl->GetSwitches();

  switches::RemoveSwitchesForAutostart(&switches);

  // Append the rest of the switches (along with their values, if any)
  // to the new command line
  for (std::map<std::string, CommandLine::StringType>::const_iterator i =
      switches.begin(); i != switches.end(); ++i) {
      CommandLine::StringType switch_value = i->second;
      if (switch_value.length() > 0) {
        new_cl->AppendSwitchNative(i->first, i->second);
      } else {
        new_cl->AppendSwitch(i->first);
      }
  }

  // Ensure that our desired switches are set on the new process.
  for (size_t i = 0; i < arraysize(kSwitchesToAddOnAutorestart); ++i) {
    if (!new_cl->HasSwitch(kSwitchesToAddOnAutorestart[i]))
      new_cl->AppendSwitch(kSwitchesToAddOnAutorestart[i]);
  }

  DLOG(WARNING) << "Shutting down current instance of the browser.";
  chrome::AttemptExit();

  // Transfer ownership to Upgrade.
  upgrade_util::SetNewCommandLine(new_cl.release());
}

void BrowserProcessImpl::OnAutoupdateTimer() {
  if (CanAutorestartForUpdate()) {
    DLOG(WARNING) << "Detected update.  Restarting browser.";
    RestartBackgroundInstance();
  }
}

#endif  // (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
