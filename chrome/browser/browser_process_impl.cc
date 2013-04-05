// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_impl.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/component_updater_configurator.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/remote_debugging_server.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/gpu/gl_string_manager.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/crl_set_fetcher.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/policy/policy_service.h"
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
#include "chrome/browser/thumbnails/render_widget_snapshot_taker.h"
#include "chrome/browser/ui/bookmarks/bookmark_prompt_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/web_resource/promo_resource_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/switch_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "extensions/common/constants.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/policy/browser_policy_connector.h"
#else
#include "chrome/browser/policy/policy_service_stub.h"
#endif  // defined(ENABLE_CONFIGURATION_POLICY)

#if defined(ENABLE_MESSAGE_CENTER)
#include "ui/message_center/message_center.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/views/focus/view_storage.h"
#if defined(USE_AURA)
#include "chrome/browser/metro_viewer/metro_viewer_process_host_win.h"
#endif
#elif defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_main_mac.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/memory/oom_priority_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugins_resource_service.h"
#endif

#if defined(OS_MACOSX)
#include "apps/app_shim/app_shim_host_manager_mac.h"
#endif

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
// How often to check if the persistent instance of Chrome needs to restart
// to install an update.
static const int kUpdateCheckIntervalHours = 6;
#endif

#if defined(OS_WIN)
// Attest to the fact that the call to the file thread to save preferences has
// run, and it is safe to terminate.  This avoids the potential of some other
// task prematurely terminating our waiting message loop by posting a
// QuitTask().
static bool g_end_session_file_thread_has_completed = false;
#endif

#if defined(USE_X11)
// How long to wait for the File thread to complete during EndSession, on
// Linux. We have a timeout here because we're unable to run the UI messageloop
// and there's some deadlock risk. Our only option is to exit anyway.
static const int kEndSessionTimeoutSeconds = 10;
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::PluginService;
using content::ResourceDispatcherHost;

BrowserProcessImpl::BrowserProcessImpl(
    base::SequencedTaskRunner* local_state_task_runner,
    const CommandLine& command_line)
    : created_metrics_service_(false),
      created_watchdog_thread_(false),
      created_browser_policy_connector_(false),
      created_profile_manager_(false),
      created_local_state_(false),
      created_icon_manager_(false),
      created_notification_ui_manager_(false),
      created_safe_browsing_service_(false),
      module_ref_count_(0),
      did_start_(false),
      checked_for_new_frames_(false),
      using_new_frames_(false),
      render_widget_snapshot_taker_(new RenderWidgetSnapshotTaker),
      download_status_updater_(new DownloadStatusUpdater),
      local_state_task_runner_(local_state_task_runner) {
  g_browser_process = this;

#if defined(ENABLE_PRINTING)
  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager);
#endif

  net_log_.reset(new ChromeNetLog);

  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      extensions::kExtensionScheme);
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      chrome::kExtensionResourceScheme);
  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      chrome::kChromeSearchScheme);

#if defined(OS_MACOSX)
  InitIdleMonitor();
#endif

  extensions::PermissionsInfo::GetInstance()->InitializeWithDelegate(
      extensions::ChromeAPIPermissions());
  extension_event_router_forwarder_ = new extensions::EventRouterForwarder;
  ExtensionRendererState::GetInstance()->Init();

#if defined(ENABLE_MESSAGE_CENTER)
  message_center::MessageCenter::Initialize();
#endif
}

BrowserProcessImpl::~BrowserProcessImpl() {
  tracked_objects::ThreadData::EnsureCleanupWasCalled(4);

  g_browser_process = NULL;
}

void BrowserProcessImpl::StartTearDown() {
#if defined(ENABLE_AUTOMATION)
  // Delete the AutomationProviderList before NotificationService,
  // since it may try to unregister notifications
  // Both NotificationService and AutomationProvider are singleton instances in
  // the BrowserProcess. Since AutomationProvider may have some active
  // notification observers, it is essential that it gets destroyed before the
  // NotificationService. NotificationService won't be destroyed until after
  // this destructor is run.
  automation_provider_list_.reset();
#endif

  // We need to shutdown the SdchDictionaryFetcher as it regularly holds
  // a pointer to a URLFetcher, and that URLFetcher (upon destruction) will do
  // a PostDelayedTask onto the IO thread.  This shutdown call will both discard
  // any pending URLFetchers, and avoid creating any more.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SdchDictionaryFetcher::Shutdown));

  // We need to destroy the MetricsService, VariationsService,
  // IntranetRedirectDetector, PromoResourceService, and SafeBrowsing
  // ClientSideDetectionService (owned by the SafeBrowsingService) before the
  // io_thread_ gets destroyed, since their destructors can call the URLFetcher
  // destructor, which does a PostDelayedTask operation on the IO thread. (The
  // IO thread will handle that URLFetcher operation before going away.)
  metrics_service_.reset();
  variations_service_.reset();
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
  profile_manager_.reset();

#if !defined(OS_ANDROID)
  // Debugger must be cleaned up before IO thread and NotificationService.
  remote_debugging_server_.reset();
#endif

  ExtensionRendererState::GetInstance()->Shutdown();

#if defined(ENABLE_MESSAGE_CENTER)
  message_center::MessageCenter::Shutdown();
#endif

#if defined(ENABLE_CONFIGURATION_POLICY)
  // The policy providers managed by |browser_policy_connector_| need to shut
  // down while the IO and FILE threads are still alive.
  if (browser_policy_connector_)
    browser_policy_connector_->Shutdown();
#endif

  // Stop the watchdog thread before stopping other threads.
  watchdog_thread_.reset();

#if defined(USE_AURA)
  // Delete aura after the metrics service has been deleted as it accesses
  // monitor information.
  aura::Env::DeleteInstance();
#endif

#if defined(OS_MACOSX)
  app_shim_host_manager_.reset();
#endif
}

void BrowserProcessImpl::PostDestroyThreads() {
  // With the file_thread_ flushed, we can release any icon resources.
  icon_manager_.reset();

  // Reset associated state right after actual thread is stopped,
  // as io_thread_.global_ cleanup happens in CleanUp on the IO
  // thread, i.e. as the thread exits its message loop.
  //
  // This is important also because in various places, the
  // IOThread object being NULL is considered synonymous with the
  // IO thread having stopped.
  io_thread_.reset();
}

#if defined(OS_WIN)
// Send a QuitTask to the given MessageLoop when the (file) thread has processed
// our (other) recent requests (to save preferences).
// Change the boolean so that the receiving thread will know that we did indeed
// send the QuitTask that terminated the message loop.
static void PostQuit(MessageLoop* message_loop) {
  g_end_session_file_thread_has_completed = true;
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}
#elif defined(USE_X11)
static void Signal(base::WaitableEvent* event) {
  event->Signal();
}
#endif

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

unsigned int BrowserProcessImpl::ReleaseModule() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(0u, module_ref_count_);
  module_ref_count_--;
  if (0 == module_ref_count_) {
    release_last_reference_callstack_ = base::debug::StackTrace();

#if defined(ENABLE_PRINTING)
    // Wait for the pending print jobs to finish. Don't do this later, since
    // this might cause a nested message loop to run, and we don't want pending
    // tasks to run once teardown has started.
    print_job_manager_->OnQuit();
    print_job_manager_.reset();
#endif

    CHECK(MessageLoop::current()->is_running());

#if defined(OS_MACOSX)
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(ChromeBrowserMainPartsMac::DidEndMainMessageLoop));
#endif
    MessageLoop::current()->Quit();
  }
  return module_ref_count_;
}

void BrowserProcessImpl::EndSession() {
  // Mark all the profiles as clean.
  ProfileManager* pm = profile_manager();
  std::vector<Profile*> profiles(pm->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    profiles[i]->SetExitType(Profile::EXIT_SESSION_ENDED);

  // Tell the metrics service it was cleanly shutdown.
  MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics && local_state()) {
    metrics->RecordStartOfSessionEnd();

    // MetricsService lazily writes to prefs, force it to write now.
    local_state()->CommitPendingWrite();
  }

  // http://crbug.com/125207
  base::ThreadRestrictions::ScopedAllowWait allow_wait;

  // We must write that the profile and metrics service shutdown cleanly,
  // otherwise on startup we'll think we crashed. So we block until done and
  // then proceed with normal shutdown.
#if defined(USE_X11)
  //  Can't run a local loop on linux. Instead create a waitable event.
  scoped_ptr<base::WaitableEvent> done_writing(
      new base::WaitableEvent(false, false));
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(Signal, done_writing.get()));
  // If all file writes haven't cleared in the timeout, leak the WaitableEvent
  // so that there's no race to reference it in Signal().
  if (!done_writing->TimedWait(
      base::TimeDelta::FromSeconds(kEndSessionTimeoutSeconds))) {
    ignore_result(done_writing.release());
  }

#elif defined(OS_WIN)
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(PostQuit, MessageLoop::current()));
  int quits_received = 0;
  do {
    MessageLoop::current()->Run();
    ++quits_received;
  } while (!g_end_session_file_thread_has_completed);
  // If we did get extra quits, then we should re-post them to the message loop.
  while (--quits_received > 0)
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
#else
  NOTIMPLEMENTED();
#endif
}

MetricsService* BrowserProcessImpl::metrics_service() {
  DCHECK(CalledOnValidThread());
  if (!created_metrics_service_)
    CreateMetricsService();
  return metrics_service_.get();
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
  if (!variations_service_.get()) {
    variations_service_.reset(
        chrome_variations::VariationsService::Create(local_state()));
  }
  return variations_service_.get();
}

#if defined(OS_CHROMEOS)
chromeos::OomPriorityManager* BrowserProcessImpl::oom_priority_manager() {
  DCHECK(CalledOnValidThread());
  if (!oom_priority_manager_.get())
    oom_priority_manager_.reset(new chromeos::OomPriorityManager());
  return oom_priority_manager_.get();
}
#endif  // defined(OS_CHROMEOS)

extensions::EventRouterForwarder*
BrowserProcessImpl::extension_event_router_forwarder() {
  return extension_event_router_forwarder_.get();
}

NotificationUIManager* BrowserProcessImpl::notification_ui_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_notification_ui_manager_)
    CreateNotificationUIManager();
  return notification_ui_manager_.get();
}

#if defined(ENABLE_MESSAGE_CENTER)
message_center::MessageCenter* BrowserProcessImpl::message_center() {
  DCHECK(CalledOnValidThread());
  return message_center::MessageCenter::Get();
}
#endif

policy::BrowserPolicyConnector* BrowserProcessImpl::browser_policy_connector() {
  DCHECK(CalledOnValidThread());
#if defined(ENABLE_CONFIGURATION_POLICY)
  if (!created_browser_policy_connector_) {
    DCHECK(!browser_policy_connector_);
    browser_policy_connector_.reset(new policy::BrowserPolicyConnector());
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

RenderWidgetSnapshotTaker* BrowserProcessImpl::GetRenderWidgetSnapshotTaker() {
  return render_widget_snapshot_taker_.get();
}

AutomationProviderList* BrowserProcessImpl::GetAutomationProviderList() {
  DCHECK(CalledOnValidThread());
#if defined(ENABLE_AUTOMATION)
  if (automation_provider_list_.get() == NULL)
    automation_provider_list_.reset(new AutomationProviderList());
  return automation_provider_list_.get();
#else
  return NULL;
#endif
}

void BrowserProcessImpl::CreateDevToolsHttpProtocolHandler(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type,
    const std::string& ip,
    int port,
    const std::string& frontend_url) {
  DCHECK(CalledOnValidThread());
#if !defined(OS_ANDROID)
  // StartupBrowserCreator::LaunchBrowser can be run multiple times when browser
  // is started with several profiles or existing browser process is reused.
  if (!remote_debugging_server_.get()) {
    remote_debugging_server_.reset(
        new RemoteDebuggingServer(profile, host_desktop_type, ip, port,
                                  frontend_url));
  }
#endif
}

bool BrowserProcessImpl::IsShuttingDown() {
  DCHECK(CalledOnValidThread());
  return did_start_ && 0 == module_ref_count_;
}

printing::PrintJobManager* BrowserProcessImpl::print_job_manager() {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  // http://code.google.com/p/chromium/issues/detail?id=6828
  // print_job_manager_ is initialized in the constructor and destroyed in the
  // destructor, so it should always be valid.
  DCHECK(print_job_manager_.get());
  return print_job_manager_.get();
}

printing::PrintPreviewDialogController*
    BrowserProcessImpl::print_preview_dialog_controller() {
#if defined(ENABLE_PRINTING)
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
#if defined(ENABLE_PRINTING)
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
  static_cast<chrome::ChromeContentBrowserClient*>(
      content::GetContentClient()->browser())->SetApplicationLocale(locale);
}

DownloadStatusUpdater* BrowserProcessImpl::download_status_updater() {
  return download_status_updater_.get();
}

BookmarkPromptController* BrowserProcessImpl::bookmark_prompt_controller() {
#if defined(OS_ANDROID)
  return NULL;
#else
  return bookmark_prompt_controller_.get();
#endif
}

chrome::MediaFileSystemRegistry*
BrowserProcessImpl::media_file_system_registry() {
  if (!media_file_system_registry_)
    media_file_system_registry_.reset(new chrome::MediaFileSystemRegistry());
  return media_file_system_registry_.get();
}

#if !defined(OS_WIN)
void BrowserProcessImpl::PlatformSpecificCommandLineProcessing(
    const CommandLine& command_line) {
}
#endif

bool BrowserProcessImpl::created_local_state() const {
    return created_local_state_;
}

// static
void BrowserProcessImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDefaultBrowserSettingEnabled,
                                false);
  // This policy needs to be defined before the net subsystem is initialized,
  // so we do it here.
  registry->RegisterIntegerPref(prefs::kMaxConnectionsPerProxy,
                                net::kDefaultMaxSocketsPerProxyServer);

  // This is observed by ChildProcessSecurityPolicy, which lives in content/
  // though, so it can't register itself.
  registry->RegisterListPref(prefs::kDisabledSchemes);

  registry->RegisterBooleanPref(prefs::kAllowCrossOriginAuthPrompt, false);

#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    registry->RegisterBooleanPref(prefs::kRestartSwitchMode, false);
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
}

DownloadRequestLimiter* BrowserProcessImpl::download_request_limiter() {
  DCHECK(CalledOnValidThread());
  if (!download_request_limiter_)
    download_request_limiter_ = new DownloadRequestLimiter();
  return download_request_limiter_;
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

ComponentUpdateService* BrowserProcessImpl::component_updater() {
#if defined(OS_CHROMEOS)
  return NULL;
#else
  if (!component_updater_.get()) {
    ComponentUpdateService::Configurator* configurator =
        MakeChromeComponentUpdaterConfigurator(
            CommandLine::ForCurrentProcess(),
            io_thread()->system_url_request_context_getter());
    // Creating the component updater does not do anything, components
    // need to be registered and Start() needs to be called.
    component_updater_.reset(ComponentUpdateServiceFactory(configurator));
  }
  return component_updater_.get();
#endif
}

CRLSetFetcher* BrowserProcessImpl::crl_set_fetcher() {
#if defined(OS_CHROMEOS)
  // There's no component updater on ChromeOS so there can't be a CRLSetFetcher
  // either.
  return NULL;
#else
  if (!crl_set_fetcher_.get())
    crl_set_fetcher_ = new CRLSetFetcher();
  return crl_set_fetcher_.get();
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

void BrowserProcessImpl::CreateMetricsService() {
  DCHECK(!created_metrics_service_ && metrics_service_.get() == NULL);
  created_metrics_service_ = true;

  metrics_service_.reset(new MetricsService);
}

void BrowserProcessImpl::CreateWatchdogThread() {
  DCHECK(!created_watchdog_thread_ && watchdog_thread_.get() == NULL);
  created_watchdog_thread_ = true;

  scoped_ptr<WatchDogThread> thread(new WatchDogThread());
  if (!thread->Start())
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
  chrome::RegisterLocalState(pref_registry);

  local_state_.reset(
      chrome_prefs::CreateLocalState(local_state_path,
                                     local_state_task_runner_,
                                     policy_service(),
                                     NULL,
                                     pref_registry,
                                     false));

  pref_change_registrar_.Init(local_state_.get());

  // Initialize the notification for the default browser setting policy.
  pref_change_registrar_.Add(
      prefs::kDefaultBrowserSettingEnabled,
      base::Bind(&BrowserProcessImpl::ApplyDefaultBrowserPolicy,
                 base::Unretained(this)));

  int max_per_proxy = local_state_->GetInteger(prefs::kMaxConnectionsPerProxy);
  net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL,
      std::max(std::min(max_per_proxy, 99),
               net::ClientSocketPoolManager::max_sockets_per_group(
                   net::HttpNetworkSession::NORMAL_SOCKET_POOL)));

  pref_change_registrar_.Add(
      prefs::kDisabledSchemes,
      base::Bind(&BrowserProcessImpl::ApplyDisabledSchemesPolicy,
                 base::Unretained(this)));
  ApplyDisabledSchemesPolicy();
}

void BrowserProcessImpl::PreCreateThreads() {
  io_thread_.reset(new IOThread(local_state(), policy_service(), net_log_.get(),
                                extension_event_router_forwarder_.get()));
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

#if defined(ENABLE_PLUGINS)
  PluginService* plugin_service = PluginService::GetInstance();
  plugin_service->SetFilter(ChromePluginServiceFilter::GetInstance());
  plugin_service->StartWatchingPlugins();

#if defined(OS_POSIX)
  // Also find plugins in a user-specific plugins dir,
  // e.g. ~/.config/chromium/Plugins.
  base::FilePath user_data_dir;
  if (PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    plugin_service->AddExtraPluginDir(user_data_dir.Append("Plugins"));
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

#if !defined(OS_ANDROID)
  if (browser_defaults::bookmarks_enabled &&
      BookmarkPromptController::IsEnabled()) {
    bookmark_prompt_controller_.reset(new BookmarkPromptController());
  }
#endif

#if defined(OS_MACOSX)
  app_shim_host_manager_.reset(new AppShimHostManager);
#endif
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
#if defined(ENABLE_PRINTING)
  DCHECK(print_preview_dialog_controller_.get() == NULL);
  print_preview_dialog_controller_ =
      new printing::PrintPreviewDialogController();
#else
  NOTIMPLEMENTED();
#endif
}

void BrowserProcessImpl::CreateBackgroundPrintingManager() {
#if defined(ENABLE_PRINTING)
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

void BrowserProcessImpl::ApplyDisabledSchemesPolicy() {
  std::set<std::string> schemes;
  const ListValue* scheme_list =
      local_state()->GetList(prefs::kDisabledSchemes);
  for (ListValue::const_iterator iter = scheme_list->begin();
       iter != scheme_list->end(); ++iter) {
    std::string scheme;
    if ((*iter)->GetAsString(&scheme))
      schemes.insert(scheme);
  }
  ChildProcessSecurityPolicy::GetInstance()->RegisterDisabledSchemes(schemes);
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
