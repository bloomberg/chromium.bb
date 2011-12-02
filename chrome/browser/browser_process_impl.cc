// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_impl.h"

#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/browser/component_updater/component_updater_configurator.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/debugger/remote_debugging_server.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/extensions/extension_tab_id_map.h"
#include "chrome/browser/extensions/network_delay_listener.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/crl_set_fetcher.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_tracker.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/tab_closeable_state_watcher.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_resource/gpu_blacklist_updater.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/switch_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/download/download_file_manager.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/net/browser_online_state_observer.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_fetcher.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "ui/views/focus/view_storage.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/chrome_browser_main_mac.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/web_socket_proxy_controller.h"
#include "chrome/browser/oom_priority_manager.h"
#endif  // defined(OS_CHROMEOS)

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

BrowserProcessImpl::BrowserProcessImpl(const CommandLine& command_line)
    : created_resource_dispatcher_host_(false),
      created_metrics_service_(false),
      created_watchdog_thread_(false),
      created_profile_manager_(false),
      created_local_state_(false),
      created_icon_manager_(false),
      created_sidebar_manager_(false),
      created_browser_policy_connector_(false),
      created_notification_ui_manager_(false),
      created_safe_browsing_service_(false),
      module_ref_count_(0),
      did_start_(false),
      checked_for_new_frames_(false),
      using_new_frames_(false),
      thumbnail_generator_(new ThumbnailGenerator),
      download_status_updater_(new DownloadStatusUpdater) {
  g_browser_process = this;
  clipboard_.reset(new ui::Clipboard);

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager);

  net_log_.reset(new ChromeNetLog);

  ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      chrome::kExtensionScheme);

  extension_event_router_forwarder_ = new ExtensionEventRouterForwarder;

  ExtensionTabIdMap::GetInstance()->Init();

  online_state_observer_.reset(new BrowserOnlineStateObserver);
}

BrowserProcessImpl::~BrowserProcessImpl() {
  // See StartTearDown and PreStopThread functions below, this is where
  // most destruction happens so that it can be interleaved with threads
  // going away.

  // Wait for the pending print jobs to finish.
  print_job_manager_->OnQuit();
  print_job_manager_.reset();

  tracked_objects::ThreadData::EnsureCleanupWasCalled(4);

  g_browser_process = NULL;
}

void BrowserProcessImpl::StartTearDown() {
  // Delete the AutomationProviderList before NotificationService,
  // since it may try to unregister notifications
  // Both NotificationService and AutomationProvider are singleton instances in
  // the BrowserProcess. Since AutomationProvider may have some active
  // notification observers, it is essential that it gets destroyed before the
  // NotificationService. NotificationService won't be destroyed until after
  // this destructor is run.
  automation_provider_list_.reset();

  // We need to shutdown the SdchDictionaryFetcher as it regularly holds
  // a pointer to a URLFetcher, and that URLFetcher (upon destruction) will do
  // a PostDelayedTask onto the IO thread.  This shutdown call will both discard
  // any pending URLFetchers, and avoid creating any more.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&SdchDictionaryFetcher::Shutdown));

  // We need to destroy the MetricsService, GoogleURLTracker,
  // IntranetRedirectDetector, and SafeBrowsing ClientSideDetectionService
  // (owned by the SafeBrowsingService) before the io_thread_ gets destroyed,
  // since their destructors can call the URLFetcher destructor, which does a
  // PostDelayedTask operation on the IO thread.
  // (The IO thread will handle that URLFetcher operation before going away.)
  metrics_service_.reset();
  google_url_tracker_.reset();
  intranet_redirect_detector_.reset();
#if defined(ENABLE_SAFE_BROWSING)
  if (safe_browsing_service_.get()) {
    safe_browsing_service()->ShutDown();
  }
#endif

  // Need to clear the desktop notification balloons before the io_thread_ and
  // before the profiles, since if there are any still showing we will access
  // those things during teardown.
  notification_ui_manager_.reset();

  // FIXME - We shouldn't need this, it's because of DefaultRequestContext! :(
  // We need to kill off all URLFetchers using profile related
  // URLRequestContexts. Normally that'd be covered by deleting the Profiles,
  // but we have some URLFetchers using the DefaultRequestContext, so they need
  // to be cancelled too. Remove this when DefaultRequestContext goes away.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&content::URLFetcher::CancelAll));

  // Need to clear profiles (download managers) before the io_thread_.
  profile_manager_.reset();

  // Debugger must be cleaned up before IO thread and NotificationService.
  remote_debugging_server_.reset();

  if (resource_dispatcher_host_.get()) {
    // Cancel pending requests and prevent new requests.
    resource_dispatcher_host()->Shutdown();
  }

  ExtensionTabIdMap::GetInstance()->Shutdown();

  // The policy providers managed by |browser_policy_connector_| need to shut
  // down while the IO and FILE threads are still alive.
  browser_policy_connector_.reset();

  // Destroying the GpuProcessHostUIShims on the UI thread posts a task to
  // delete related objects on the GPU thread. This must be done before
  // stopping the GPU thread. The GPU thread will close IPC channels to renderer
  // processes so this has to happen before stopping the IO thread.
  GpuProcessHostUIShim::DestroyAll();

  // Stop the watchdog thread before stopping other threads.
  watchdog_thread_.reset();
}

void BrowserProcessImpl::PreStartThread(BrowserThread::ID thread_id) {
  switch (thread_id) {
    case BrowserThread::IO:
      CreateIOThreadState();
      break;

    default:
      break;
  }
}

void BrowserProcessImpl::PostStartThread(BrowserThread::ID thread_id) {
}

void BrowserProcessImpl::PreStopThread(BrowserThread::ID thread_id) {
  switch (thread_id) {
#if defined(OS_CHROMEOS)
    case BrowserThread::WEB_SOCKET_PROXY:
      chromeos::WebSocketProxyController::Shutdown();
      break;
#endif
    case BrowserThread::FILE:
      // Clean up state that lives on or uses the file_thread_ before
      // it goes away.
      if (resource_dispatcher_host_.get()) {
        resource_dispatcher_host()->download_file_manager()->Shutdown();
        resource_dispatcher_host()->save_file_manager()->Shutdown();
      }
      break;
    case BrowserThread::WEBKIT:
      // Need to destroy ResourceDispatcherHost before PluginService
      // and SafeBrowsingService, since it caches a pointer to
      // it. This also causes the webkit thread to terminate (which is
      // still the responsibility of the embedder, not of the content
      // framework).
      resource_dispatcher_host_.reset();
      break;
    default:
      break;
  }
}

void BrowserProcessImpl::PostStopThread(BrowserThread::ID thread_id) {
  switch (thread_id) {
    case BrowserThread::FILE:
      // With the file_thread_ flushed, we can release any icon resources.
      icon_manager_.reset();
      break;
    case BrowserThread::IO:
      // Reset associated state right after actual thread is stopped,
      // as io_thread_.global_ cleanup happens in CleanUp on the IO
      // thread, i.e. as the thread exits its message loop.
      io_thread_.reset();
      break;
    default:
      break;
  }
}

#if defined(OS_WIN)
// Send a QuitTask to the given MessageLoop when the (file) thread has processed
// our (other) recent requests (to save preferences).
// Change the boolean so that the receiving thread will know that we did indeed
// send the QuitTask that terminated the message loop.
static void PostQuit(MessageLoop* message_loop) {
  g_end_session_file_thread_has_completed = true;
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}
#elif defined(USE_X11)
static void Signal(base::WaitableEvent* event) {
  event->Signal();
}
#endif

unsigned int BrowserProcessImpl::AddRefModule() {
  DCHECK(CalledOnValidThread());
  CHECK(!IsShuttingDown());
  did_start_ = true;
  module_ref_count_++;
  return module_ref_count_;
}

unsigned int BrowserProcessImpl::ReleaseModule() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(0u, module_ref_count_);
  module_ref_count_--;
  if (0 == module_ref_count_) {
    CHECK(MessageLoop::current()->is_running());
    // Allow UI and IO threads to do blocking IO on shutdown, since we do a lot
    // of it on shutdown for valid reasons.
    base::ThreadRestrictions::SetIOAllowed(true);
    CHECK(!BrowserList::GetLastActive());
    io_thread()->message_loop()->PostTask(
        FROM_HERE,
        base::IgnoreReturn<bool>(
            base::Bind(&base::ThreadRestrictions::SetIOAllowed, true)));

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
    profiles[i]->MarkAsCleanShutdown();

  // Tell the metrics service it was cleanly shutdown.
  MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics && local_state()) {
    metrics->RecordStartOfSessionEnd();

    // MetricsService lazily writes to prefs, force it to write now.
    local_state()->SavePersistentPrefs();
  }

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
      base::TimeDelta::FromSeconds(kEndSessionTimeoutSeconds)))
    ignore_result(done_writing.release());

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
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
#else
  NOTIMPLEMENTED();
#endif
}

ResourceDispatcherHost* BrowserProcessImpl::resource_dispatcher_host() {
  DCHECK(CalledOnValidThread());
  if (!created_resource_dispatcher_host_)
    CreateResourceDispatcherHost();
  return resource_dispatcher_host_.get();
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

base::Thread* BrowserProcessImpl::file_thread() {
  DCHECK(CalledOnValidThread());
  return BrowserThread::UnsafeGetBrowserThread(BrowserThread::FILE);
}

base::Thread* BrowserProcessImpl::db_thread() {
  DCHECK(CalledOnValidThread());
  return BrowserThread::UnsafeGetBrowserThread(BrowserThread::DB);
}

base::Thread* BrowserProcessImpl::process_launcher_thread() {
  DCHECK(CalledOnValidThread());
  return BrowserThread::UnsafeGetBrowserThread(BrowserThread::PROCESS_LAUNCHER);
}

base::Thread* BrowserProcessImpl::cache_thread() {
  DCHECK(CalledOnValidThread());
  return BrowserThread::UnsafeGetBrowserThread(BrowserThread::CACHE);
}

WatchDogThread* BrowserProcessImpl::watchdog_thread() {
  DCHECK(CalledOnValidThread());
  if (!created_watchdog_thread_)
    CreateWatchdogThread();
  DCHECK(watchdog_thread_.get() != NULL);
  return watchdog_thread_.get();
}

#if defined(OS_CHROMEOS)
base::Thread* BrowserProcessImpl::web_socket_proxy_thread() {
  DCHECK(CalledOnValidThread());
  return BrowserThread::UnsafeGetBrowserThread(BrowserThread::WEB_SOCKET_PROXY);
}
#endif

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

SidebarManager* BrowserProcessImpl::sidebar_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_sidebar_manager_)
    CreateSidebarManager();
  return sidebar_manager_.get();
}

ui::Clipboard* BrowserProcessImpl::clipboard() {
  DCHECK(CalledOnValidThread());
  return clipboard_.get();
}

net::URLRequestContextGetter* BrowserProcessImpl::system_request_context() {
  DCHECK(CalledOnValidThread());
  return io_thread()->system_url_request_context_getter();
}

#if defined(OS_CHROMEOS)
browser::OomPriorityManager* BrowserProcessImpl::oom_priority_manager() {
  DCHECK(CalledOnValidThread());
  if (!oom_priority_manager_.get())
    oom_priority_manager_.reset(new browser::OomPriorityManager());
  return oom_priority_manager_.get();
}
#endif  // defined(OS_CHROMEOS)

ExtensionEventRouterForwarder*
BrowserProcessImpl::extension_event_router_forwarder() {
  return extension_event_router_forwarder_.get();
}

NotificationUIManager* BrowserProcessImpl::notification_ui_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_notification_ui_manager_)
    CreateNotificationUIManager();
  return notification_ui_manager_.get();
}

policy::BrowserPolicyConnector* BrowserProcessImpl::browser_policy_connector() {
  DCHECK(CalledOnValidThread());
  if (!created_browser_policy_connector_) {
    DCHECK(browser_policy_connector_.get() == NULL);
    created_browser_policy_connector_ = true;
#if defined(ENABLE_CONFIGURATION_POLICY)
    browser_policy_connector_.reset(new policy::BrowserPolicyConnector());
    browser_policy_connector_->Init();
#endif
  }
  return browser_policy_connector_.get();
}

IconManager* BrowserProcessImpl::icon_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_icon_manager_)
    CreateIconManager();
  return icon_manager_.get();
}

ThumbnailGenerator* BrowserProcessImpl::GetThumbnailGenerator() {
  return thumbnail_generator_.get();
}

AutomationProviderList* BrowserProcessImpl::GetAutomationProviderList() {
  DCHECK(CalledOnValidThread());
  if (automation_provider_list_.get() == NULL)
    automation_provider_list_.reset(new AutomationProviderList());
  return automation_provider_list_.get();
}

void BrowserProcessImpl::InitDevToolsHttpProtocolHandler(
    Profile* profile,
    const std::string& ip,
    int port,
    const std::string& frontend_url) {
  DCHECK(CalledOnValidThread());
  remote_debugging_server_.reset(
      new RemoteDebuggingServer(profile, ip, port, frontend_url));
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

printing::PrintPreviewTabController*
    BrowserProcessImpl::print_preview_tab_controller() {
  DCHECK(CalledOnValidThread());
  if (!print_preview_tab_controller_.get())
    CreatePrintPreviewTabController();
  return print_preview_tab_controller_.get();
}

printing::BackgroundPrintingManager*
    BrowserProcessImpl::background_printing_manager() {
  DCHECK(CalledOnValidThread());
  if (!background_printing_manager_.get())
    CreateBackgroundPrintingManager();
  return background_printing_manager_.get();
}

GoogleURLTracker* BrowserProcessImpl::google_url_tracker() {
  DCHECK(CalledOnValidThread());
  if (!google_url_tracker_.get())
    CreateGoogleURLTracker();
  return google_url_tracker_.get();
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
}

DownloadStatusUpdater* BrowserProcessImpl::download_status_updater() {
  return download_status_updater_.get();
}

DownloadRequestLimiter* BrowserProcessImpl::download_request_limiter() {
  DCHECK(CalledOnValidThread());
  if (!download_request_limiter_)
    download_request_limiter_ = new DownloadRequestLimiter();
  return download_request_limiter_;
}

TabCloseableStateWatcher* BrowserProcessImpl::tab_closeable_state_watcher() {
  DCHECK(CalledOnValidThread());
  if (!tab_closeable_state_watcher_.get())
    CreateTabCloseableStateWatcher();
  return tab_closeable_state_watcher_.get();
}

BackgroundModeManager* BrowserProcessImpl::background_mode_manager() {
  DCHECK(CalledOnValidThread());
  if (!background_mode_manager_.get())
    CreateBackgroundModeManager();
  return background_mode_manager_.get();
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

bool BrowserProcessImpl::plugin_finder_disabled() const {
  if (plugin_finder_disabled_pref_.get())
    return plugin_finder_disabled_pref_->GetValue();
  else
    return false;
}

void BrowserProcessImpl::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref = content::Details<std::string>(details).ptr();
    if (*pref == prefs::kDefaultBrowserSettingEnabled) {
      ApplyDefaultBrowserPolicy();
    } else if (*pref == prefs::kDisabledSchemes) {
      ApplyDisabledSchemesPolicy();
    } else if (*pref == prefs::kAllowCrossOriginAuthPrompt) {
      ApplyAllowCrossOriginAuthPromptPolicy();
    }
  } else {
    NOTREACHED();
  }
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

MHTMLGenerationManager* BrowserProcessImpl::mhtml_generation_manager() {
  if (!mhtml_generation_manager_.get())
    mhtml_generation_manager_ = new MHTMLGenerationManager();

  return mhtml_generation_manager_.get();
}

GpuBlacklistUpdater* BrowserProcessImpl::gpu_blacklist_updater() {
  if (!gpu_blacklist_updater_.get())
    gpu_blacklist_updater_ = new GpuBlacklistUpdater();

  return gpu_blacklist_updater_.get();
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
  if (!crl_set_fetcher_.get()) {
    crl_set_fetcher_ = new CRLSetFetcher();
  }
  return crl_set_fetcher_.get();
#endif
}

void BrowserProcessImpl::CreateResourceDispatcherHost() {
  DCHECK(!created_resource_dispatcher_host_ &&
         resource_dispatcher_host_.get() == NULL);
  created_resource_dispatcher_host_ = true;

  // UserScriptListener and NetworkDelayListener will delete themselves.
  ResourceQueue::DelegateSet resource_queue_delegates;
  resource_queue_delegates.insert(new UserScriptListener());
  resource_queue_delegates.insert(new NetworkDelayListener());

  resource_dispatcher_host_.reset(
      new ResourceDispatcherHost(resource_queue_delegates));
  resource_dispatcher_host_->Initialize();

  resource_dispatcher_host_delegate_.reset(
      new ChromeResourceDispatcherHostDelegate(resource_dispatcher_host_.get(),
                                               prerender_tracker()));
  resource_dispatcher_host_->set_delegate(
      resource_dispatcher_host_delegate_.get());

  pref_change_registrar_.Add(prefs::kAllowCrossOriginAuthPrompt, this);
  ApplyAllowCrossOriginAuthPromptPolicy();
}

void BrowserProcessImpl::CreateMetricsService() {
  DCHECK(!created_metrics_service_ && metrics_service_.get() == NULL);
  created_metrics_service_ = true;

  metrics_service_.reset(new MetricsService);
}

void BrowserProcessImpl::CreateIOThreadState() {
  // Prior to any processing happening on the io thread, we create the
  // plugin service as it is predominantly used from the io thread,
  // but must be created on the main thread. The service ctor is
  // inexpensive and does not invoke the io_thread() accessor.
  PluginService* plugin_service = PluginService::GetInstance();
  plugin_service->Init();
  plugin_service->set_filter(ChromePluginServiceFilter::GetInstance());
  plugin_service->StartWatchingPlugins();

  // Register the internal Flash if available.
  FilePath path;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInternalFlash) &&
      PathService::Get(chrome::FILE_FLASH_PLUGIN, &path)) {
    plugin_service->AddExtraPluginPath(path);
  }

#if defined(OS_POSIX)
  // Also find plugins in a user-specific plugins dir,
  // e.g. ~/.config/chromium/Plugins.
  FilePath user_data_dir;
  if (PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    plugin_service->AddExtraPluginPath(user_data_dir.Append("Plugins"));
  }
#endif

  scoped_ptr<IOThread> thread(new IOThread(
      local_state(), net_log_.get(), extension_event_router_forwarder_.get()));
  io_thread_.swap(thread);
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

  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  profile_manager_.reset(new ProfileManager(user_data_dir));
}

void BrowserProcessImpl::CreateLocalState() {
  DCHECK(!created_local_state_ && local_state_.get() == NULL);
  created_local_state_ = true;

  FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  local_state_.reset(
      PrefService::CreatePrefService(local_state_path, NULL, false));

  // Initialize the prefs of the local state.
  browser::RegisterLocalState(local_state_.get());

  pref_change_registrar_.Init(local_state_.get());

  print_job_manager_->InitOnUIThread(local_state_.get());

  // Initialize the notification for the default browser setting policy.
  local_state_->RegisterBooleanPref(prefs::kDefaultBrowserSettingEnabled,
                                    false);
  if (local_state_->IsManagedPreference(prefs::kDefaultBrowserSettingEnabled))
    ApplyDefaultBrowserPolicy();
  pref_change_registrar_.Add(prefs::kDefaultBrowserSettingEnabled, this);

  // Initialize the preference for the plugin finder policy.
  // This preference is only needed on the IO thread so make it available there.
  local_state_->RegisterBooleanPref(prefs::kDisablePluginFinder, false);
  plugin_finder_disabled_pref_.reset(new BooleanPrefMember);
  plugin_finder_disabled_pref_->Init(prefs::kDisablePluginFinder,
                                   local_state_.get(), NULL);
  plugin_finder_disabled_pref_->MoveToThread(BrowserThread::IO);

  // Another policy that needs to be defined before the net subsystem is
  // initialized is MaxConnectionsPerProxy so we do it here.
  local_state_->RegisterIntegerPref(prefs::kMaxConnectionsPerProxy,
                                    net::kDefaultMaxSocketsPerProxyServer);
  int max_per_proxy = local_state_->GetInteger(prefs::kMaxConnectionsPerProxy);
  net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
      std::max(std::min(max_per_proxy, 99),
               net::ClientSocketPoolManager::max_sockets_per_group()));

  // This is observed by ChildProcessSecurityPolicy, which lives in content/
  // though, so it can't register itself.
  local_state_->RegisterListPref(prefs::kDisabledSchemes);
  pref_change_registrar_.Add(prefs::kDisabledSchemes, this);
  ApplyDisabledSchemesPolicy();

  local_state_->RegisterBooleanPref(prefs::kAllowCrossOriginAuthPrompt, false);
}

void BrowserProcessImpl::CreateIconManager() {
  DCHECK(!created_icon_manager_ && icon_manager_.get() == NULL);
  created_icon_manager_ = true;
  icon_manager_.reset(new IconManager);
}

void BrowserProcessImpl::CreateSidebarManager() {
  DCHECK(sidebar_manager_.get() == NULL);
  created_sidebar_manager_ = true;
  sidebar_manager_ = new SidebarManager();
}

void BrowserProcessImpl::CreateGoogleURLTracker() {
  DCHECK(google_url_tracker_.get() == NULL);
  scoped_ptr<GoogleURLTracker> google_url_tracker(new GoogleURLTracker);
  google_url_tracker_.swap(google_url_tracker);
}

void BrowserProcessImpl::CreateIntranetRedirectDetector() {
  DCHECK(intranet_redirect_detector_.get() == NULL);
  scoped_ptr<IntranetRedirectDetector> intranet_redirect_detector(
      new IntranetRedirectDetector);
  intranet_redirect_detector_.swap(intranet_redirect_detector);
}

void BrowserProcessImpl::CreateNotificationUIManager() {
  DCHECK(notification_ui_manager_.get() == NULL);
  notification_ui_manager_.reset(NotificationUIManager::Create(local_state()));
  created_notification_ui_manager_ = true;
}

void BrowserProcessImpl::CreateTabCloseableStateWatcher() {
  DCHECK(tab_closeable_state_watcher_.get() == NULL);
  tab_closeable_state_watcher_.reset(TabCloseableStateWatcher::Create());
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

void BrowserProcessImpl::CreatePrintPreviewTabController() {
  DCHECK(print_preview_tab_controller_.get() == NULL);
  print_preview_tab_controller_ = new printing::PrintPreviewTabController();
}

void BrowserProcessImpl::CreateBackgroundPrintingManager() {
  DCHECK(background_printing_manager_.get() == NULL);
  background_printing_manager_.reset(new printing::BackgroundPrintingManager());
}

void BrowserProcessImpl::CreateSafeBrowsingService() {
  DCHECK(safe_browsing_service_.get() == NULL);
  // Set this flag to true so that we don't retry indefinitely to
  // create the service class if there was an error.
  created_safe_browsing_service_ = true;
#if defined(ENABLE_SAFE_BROWSING)
  safe_browsing_service_ = SafeBrowsingService::CreateSafeBrowsingService();
  safe_browsing_service_->Initialize();
#endif
}

void BrowserProcessImpl::ApplyDisabledSchemesPolicy() {
  std::set<std::string> schemes;
  const ListValue* scheme_list = local_state_->GetList(prefs::kDisabledSchemes);
  for (ListValue::const_iterator iter = scheme_list->begin();
       iter != scheme_list->end(); ++iter) {
    std::string scheme;
    if ((*iter)->GetAsString(&scheme))
      schemes.insert(scheme);
  }
  ChildProcessSecurityPolicy::GetInstance()->RegisterDisabledSchemes(schemes);
}

void BrowserProcessImpl::ApplyDefaultBrowserPolicy() {
  if (local_state_->GetBoolean(prefs::kDefaultBrowserSettingEnabled)) {
    scoped_refptr<ShellIntegration::DefaultWebClientWorker>
        set_browser_worker = new ShellIntegration::DefaultBrowserWorker(NULL);
    set_browser_worker->StartSetAsDefault();
  }
}

void BrowserProcessImpl::ApplyAllowCrossOriginAuthPromptPolicy() {
  bool value = local_state()->GetBoolean(prefs::kAllowCrossOriginAuthPrompt);
  resource_dispatcher_host()->set_allow_cross_origin_auth_prompt(value);
}

// Mac is currently not supported.
#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)

bool BrowserProcessImpl::CanAutorestartForUpdate() const {
  // Check if browser is in the background and if it needs to be restarted to
  // apply a pending update.
  return BrowserList::size() == 0 && BrowserList::WillKeepAlive() &&
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
  BrowserList::AttemptExit();

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
