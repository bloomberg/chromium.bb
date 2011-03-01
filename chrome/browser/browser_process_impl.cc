// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_impl.h"

#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_main.h"
#include "chrome/browser/browser_process_sub_thread.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/debugger/browser_list_tabcontents_provider.h"
#include "chrome/browser/debugger/devtools_http_protocol_handler.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/download/download_file_manager.h"
#include "chrome/browser/download/save_file_manager.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/plugin_data_remover.h"
#include "chrome/browser/plugin_updater.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/tab_closeable_state_watcher.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/switch_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/browser_thread.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "ipc/ipc_logging.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/database/database_tracker.h"

#if defined(OS_WIN)
#include "views/focus/view_storage.h"
#endif

#if defined(IPC_MESSAGE_LOG_ENABLED)
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#endif

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
// How often to check if the persistent instance of Chrome needs to restart
// to install an update.
static const int kUpdateCheckIntervalHours = 6;
#endif

#if defined(USE_X11)
// How long to wait for the File thread to complete during EndSession, on
// Linux. We have a timeout here because we're unable to run the UI messageloop
// and there's some deadlock risk. Our only option is to exit anyway.
static const int kEndSessionTimeoutSeconds = 10;
#endif

BrowserProcessImpl::BrowserProcessImpl(const CommandLine& command_line)
    : created_resource_dispatcher_host_(false),
      created_metrics_service_(false),
      created_io_thread_(false),
      created_file_thread_(false),
      created_db_thread_(false),
      created_process_launcher_thread_(false),
      created_cache_thread_(false),
      created_profile_manager_(false),
      created_local_state_(false),
      created_icon_manager_(false),
      created_devtools_manager_(false),
      created_sidebar_manager_(false),
      created_browser_policy_connector_(false),
      created_notification_ui_manager_(false),
      created_safe_browsing_detection_service_(false),
      module_ref_count_(0),
      did_start_(false),
      checked_for_new_frames_(false),
      using_new_frames_(false),
      have_inspector_files_(true) {
  g_browser_process = this;
  clipboard_.reset(new ui::Clipboard);
  main_notification_service_.reset(new NotificationService);

  notification_registrar_.Add(this,
                              NotificationType::APP_TERMINATING,
                              NotificationService::AllSources());

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager);

  shutdown_event_.reset(new base::WaitableEvent(true, false));

  net_log_.reset(new ChromeNetLog);
}

BrowserProcessImpl::~BrowserProcessImpl() {
  FilePath profile_path;
  bool clear_local_state_on_exit;

  // Store the profile path for clearing local state data on exit.
  clear_local_state_on_exit = ShouldClearLocalState(&profile_path);

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
  SdchDictionaryFetcher::Shutdown();

  // We need to destroy the MetricsService, GoogleURLTracker,
  // IntranetRedirectDetector, and SafeBrowsing ClientSideDetectionService
  // before the io_thread_ gets destroyed, since their destructors can call the
  // URLFetcher destructor, which does a PostDelayedTask operation on the IO
  // thread. (The IO thread will handle that URLFetcher operation before going
  // away.)
  metrics_service_.reset();
  google_url_tracker_.reset();
  intranet_redirect_detector_.reset();
  safe_browsing_detection_service_.reset();

  // Need to clear the desktop notification balloons before the io_thread_ and
  // before the profiles, since if there are any still showing we will access
  // those things during teardown.
  notification_ui_manager_.reset();

  // Need to clear profiles (download managers) before the io_thread_.
  profile_manager_.reset();

  // Debugger must be cleaned up before IO thread and NotificationService.
  if (devtools_http_handler_.get()) {
    devtools_http_handler_->Stop();
    devtools_http_handler_ = NULL;
  }
  if (devtools_legacy_handler_.get()) {
    devtools_legacy_handler_->Stop();
    devtools_legacy_handler_ = NULL;
  }

  if (resource_dispatcher_host_.get()) {
    // Need to tell Safe Browsing Service that the IO thread is going away
    // since it cached a pointer to it.
    if (resource_dispatcher_host()->safe_browsing_service())
      resource_dispatcher_host()->safe_browsing_service()->ShutDown();

    // Cancel pending requests and prevent new requests.
    resource_dispatcher_host()->Shutdown();
  }

  // The policy providers managed by |browser_policy_connector_| need to shut
  // down while the IO and FILE threads are still alive.
  browser_policy_connector_.reset();

#if defined(USE_X11)
  // The IO thread must outlive the BACKGROUND_X11 thread.
  background_x11_thread_.reset();
#endif

  // Wait for removing plugin data to finish before shutting down the IO thread.
  WaitForPluginDataRemoverToFinish();

  // Need to stop io_thread_ before resource_dispatcher_host_, since
  // io_thread_ may still deref ResourceDispatcherHost and handle resource
  // request before going away.
  io_thread_.reset();

  // The IO thread was the only user of this thread.
  cache_thread_.reset();

  // Stop the process launcher thread after the IO thread, in case the IO thread
  // posted a task to terminate a process on the process launcher thread.
  process_launcher_thread_.reset();

  // Clean up state that lives on the file_thread_ before it goes away.
  if (resource_dispatcher_host_.get()) {
    resource_dispatcher_host()->download_file_manager()->Shutdown();
    resource_dispatcher_host()->save_file_manager()->Shutdown();
  }

  // Need to stop the file_thread_ here to force it to process messages in its
  // message loop from the previous call to shutdown the DownloadFileManager,
  // SaveFileManager and SessionService.
  file_thread_.reset();

  // With the file_thread_ flushed, we can release any icon resources.
  icon_manager_.reset();

  // Need to destroy ResourceDispatcherHost before PluginService and
  // SafeBrowsingService, since it caches a pointer to it. This also
  // causes the webkit thread to terminate.
  resource_dispatcher_host_.reset();

  // Wait for the pending print jobs to finish.
  print_job_manager_->OnQuit();
  print_job_manager_.reset();

  // Destroy TabCloseableStateWatcher before NotificationService since the
  // former registers for notifications.
  tab_closeable_state_watcher_.reset();

  // Now OK to destroy NotificationService.
  main_notification_service_.reset();

  // Prior to clearing local state, we want to complete tasks pending
  // on the db thread too.
  db_thread_.reset();

  // At this point, no render process exist and the file, io, db, and
  // webkit threads in this process have all terminated, so it's safe
  // to access local state data such as cookies, database, or local storage.
  if (clear_local_state_on_exit)
    ClearLocalState(profile_path);

  g_browser_process = NULL;
}

#if defined(OS_WIN)
// Send a QuitTask to the given MessageLoop.
static void PostQuit(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}
#elif defined(USE_X11)
static void Signal(base::WaitableEvent* event) {
  event->Signal();
}
#endif

unsigned int BrowserProcessImpl::AddRefModule() {
  DCHECK(CalledOnValidThread());
  did_start_ = true;
  module_ref_count_++;
  return module_ref_count_;
}

unsigned int BrowserProcessImpl::ReleaseModule() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(0u, module_ref_count_);
  module_ref_count_--;
  if (0 == module_ref_count_) {
    // Allow UI and IO threads to do blocking IO on shutdown, since we do a lot
    // of it on shutdown for valid reasons.
    base::ThreadRestrictions::SetIOAllowed(true);
    io_thread()->message_loop()->PostTask(
        FROM_HERE,
        NewRunnableFunction(&base::ThreadRestrictions::SetIOAllowed, true));
    MessageLoop::current()->PostTask(
        FROM_HERE, NewRunnableFunction(DidEndMainMessageLoop));
    MessageLoop::current()->Quit();
  }
  return module_ref_count_;
}

void BrowserProcessImpl::EndSession() {
#if defined(OS_WIN) || defined(USE_X11)
  // Notify we are going away.
  shutdown_event_->Signal();
#endif

  // Mark all the profiles as clean.
  ProfileManager* pm = profile_manager();
  for (ProfileManager::const_iterator i = pm->begin(); i != pm->end(); ++i)
    (*i)->MarkAsCleanShutdown();

  // Tell the metrics service it was cleanly shutdown.
  MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics && local_state()) {
    metrics->RecordCleanShutdown();

    metrics->RecordStartOfSessionEnd();

    // MetricsService lazily writes to prefs, force it to write now.
    local_state()->SavePersistentPrefs();
  }

  // We must write that the profile and metrics service shutdown cleanly,
  // otherwise on startup we'll think we crashed. So we block until done and
  // then proceed with normal shutdown.
#if defined(USE_X11)
  //  Can't run a local loop on linux. Instead create a waitable event.
  base::WaitableEvent done_writing(false, false);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(Signal, &done_writing));
  done_writing.TimedWait(
      base::TimeDelta::FromSeconds(kEndSessionTimeoutSeconds));
#elif defined(OS_WIN)
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableFunction(PostQuit, MessageLoop::current()));
  MessageLoop::current()->Run();
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
  if (!created_io_thread_)
    CreateIOThread();
  return io_thread_.get();
}

base::Thread* BrowserProcessImpl::file_thread() {
  DCHECK(CalledOnValidThread());
  if (!created_file_thread_)
    CreateFileThread();
  return file_thread_.get();
}

base::Thread* BrowserProcessImpl::db_thread() {
  DCHECK(CalledOnValidThread());
  if (!created_db_thread_)
    CreateDBThread();
  return db_thread_.get();
}

base::Thread* BrowserProcessImpl::process_launcher_thread() {
  DCHECK(CalledOnValidThread());
  if (!created_process_launcher_thread_)
    CreateProcessLauncherThread();
  return process_launcher_thread_.get();
}

base::Thread* BrowserProcessImpl::cache_thread() {
  DCHECK(CalledOnValidThread());
  if (!created_cache_thread_)
    CreateCacheThread();
  return cache_thread_.get();
}

#if defined(USE_X11)
base::Thread* BrowserProcessImpl::background_x11_thread() {
  DCHECK(CalledOnValidThread());
  // The BACKGROUND_X11 thread is created when the IO thread is created.
  if (!created_io_thread_)
    CreateIOThread();
  return background_x11_thread_.get();
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

DevToolsManager* BrowserProcessImpl::devtools_manager() {
  DCHECK(CalledOnValidThread());
  if (!created_devtools_manager_)
    CreateDevToolsManager();
  return devtools_manager_.get();
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
    browser_policy_connector_.reset(new policy::BrowserPolicyConnector());
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
  return &thumbnail_generator_;
}

AutomationProviderList* BrowserProcessImpl::InitAutomationProviderList() {
  DCHECK(CalledOnValidThread());
  if (automation_provider_list_.get() == NULL) {
    automation_provider_list_.reset(AutomationProviderList::GetInstance());
  }
  return automation_provider_list_.get();
}

void BrowserProcessImpl::InitDevToolsHttpProtocolHandler(
    const std::string& ip,
    int port,
    const std::string& frontend_url) {
  DCHECK(CalledOnValidThread());
  devtools_http_handler_ =
      DevToolsHttpProtocolHandler::Start(ip,
                                         port,
                                         frontend_url,
                                         new BrowserListTabContentsProvider());
}

void BrowserProcessImpl::InitDevToolsLegacyProtocolHandler(int port) {
  DCHECK(CalledOnValidThread());
  devtools_legacy_handler_ = DevToolsProtocolHandler::Start(port);
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
  return &download_status_updater_;
}

base::WaitableEvent* BrowserProcessImpl::shutdown_event() {
  return shutdown_event_.get();
}

TabCloseableStateWatcher* BrowserProcessImpl::tab_closeable_state_watcher() {
  DCHECK(CalledOnValidThread());
  if (!tab_closeable_state_watcher_.get())
    CreateTabCloseableStateWatcher();
  return tab_closeable_state_watcher_.get();
}

safe_browsing::ClientSideDetectionService*
    BrowserProcessImpl::safe_browsing_detection_service() {
  DCHECK(CalledOnValidThread());
  if (!created_safe_browsing_detection_service_) {
    CreateSafeBrowsingDetectionService();
  }
  return safe_browsing_detection_service_.get();
}

bool BrowserProcessImpl::plugin_finder_disabled() const {
  return *plugin_finder_disabled_pref_;
}

void BrowserProcessImpl::CheckForInspectorFiles() {
  file_thread()->message_loop()->PostTask
      (FROM_HERE,
       NewRunnableMethod(this, &BrowserProcessImpl::DoInspectorFilesCheck));
}

void BrowserProcessImpl::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NotificationType::APP_TERMINATING) {
    Profile* profile = ProfileManager::GetDefaultProfile();
    if (profile) {
      PrefService* prefs = profile->GetPrefs();
      if (prefs->GetBoolean(prefs::kClearSiteDataOnExit) &&
          local_state()->GetBoolean(prefs::kClearPluginLSODataEnabled)) {
        plugin_data_remover_ = new PluginDataRemover();
        if (!plugin_data_remover_mime_type().empty())
          plugin_data_remover_->set_mime_type(plugin_data_remover_mime_type());
        plugin_data_remover_->StartRemoving(base::Time());
      }
    }
  } else if (type == NotificationType::PREF_CHANGED) {
    std::string* pref = Details<std::string>(details).ptr();
    if (*pref == prefs::kDefaultBrowserSettingEnabled) {
      if (local_state_->GetBoolean(prefs::kDefaultBrowserSettingEnabled))
        ShellIntegration::SetAsDefaultBrowser();
    }
  } else {
    NOTREACHED();
  }
}

void BrowserProcessImpl::WaitForPluginDataRemoverToFinish() {
  if (plugin_data_remover_.get())
    plugin_data_remover_->Wait();
}

#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
void BrowserProcessImpl::StartAutoupdateTimer() {
  autoupdate_timer_.Start(
      base::TimeDelta::FromHours(kUpdateCheckIntervalHours),
      this,
      &BrowserProcessImpl::OnAutoupdateTimer);
}
#endif

bool BrowserProcessImpl::have_inspector_files() const {
  return have_inspector_files_;
}

ChromeNetLog* BrowserProcessImpl::net_log() {
  return net_log_.get();
}

void BrowserProcessImpl::ClearLocalState(const FilePath& profile_path) {
  webkit_database::DatabaseTracker::ClearLocalState(profile_path);
}

bool BrowserProcessImpl::ShouldClearLocalState(FilePath* profile_path) {
  FilePath user_data_dir;
  Profile* profile;

  // Check for the existence of a profile manager. When quitting early,
  // e.g. because another chrome instance is running, or when invoked with
  // options such as --uninstall or --try-chrome-again=0, the profile manager
  // does not exist yet.
  if (!profile_manager_.get())
    return false;

  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  profile = profile_manager_->GetDefaultProfile(user_data_dir);
  if (!profile)
    return false;
  *profile_path = profile->GetPath();
  return profile->GetPrefs()->GetBoolean(prefs::kClearSiteDataOnExit);
}

void BrowserProcessImpl::CreateResourceDispatcherHost() {
  DCHECK(!created_resource_dispatcher_host_ &&
         resource_dispatcher_host_.get() == NULL);
  created_resource_dispatcher_host_ = true;

  resource_dispatcher_host_.reset(new ResourceDispatcherHost());
  resource_dispatcher_host_->Initialize();
}

void BrowserProcessImpl::CreateMetricsService() {
  DCHECK(!created_metrics_service_ && metrics_service_.get() == NULL);
  created_metrics_service_ = true;

  metrics_service_.reset(new MetricsService);
}

void BrowserProcessImpl::CreateIOThread() {
  DCHECK(!created_io_thread_ && io_thread_.get() == NULL);
  created_io_thread_ = true;

  // Prior to starting the io thread, we create the plugin service as
  // it is predominantly used from the io thread, but must be created
  // on the main thread. The service ctor is inexpensive and does not
  // invoke the io_thread() accessor.
  PluginService::GetInstance();

#if defined(USE_X11)
  // The lifetime of the BACKGROUND_X11 thread is a subset of the IO thread so
  // we start it now.
  scoped_ptr<base::Thread> background_x11_thread(
      new BrowserProcessSubThread(BrowserThread::BACKGROUND_X11));
  if (!background_x11_thread->Start())
    return;
  background_x11_thread_.swap(background_x11_thread);
#endif

  scoped_ptr<IOThread> thread(new IOThread(local_state(), net_log_.get()));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  io_thread_.swap(thread);
}

void BrowserProcessImpl::CreateFileThread() {
  DCHECK(!created_file_thread_ && file_thread_.get() == NULL);
  created_file_thread_ = true;

  scoped_ptr<base::Thread> thread(
      new BrowserProcessSubThread(BrowserThread::FILE));
  base::Thread::Options options;
#if defined(OS_WIN)
  // On Windows, the FILE thread needs to be have a UI message loop which pumps
  // messages in such a way that Google Update can communicate back to us.
  options.message_loop_type = MessageLoop::TYPE_UI;
#else
  options.message_loop_type = MessageLoop::TYPE_IO;
#endif
  if (!thread->StartWithOptions(options))
    return;
  file_thread_.swap(thread);
}

void BrowserProcessImpl::CreateDBThread() {
  DCHECK(!created_db_thread_ && db_thread_.get() == NULL);
  created_db_thread_ = true;

  scoped_ptr<base::Thread> thread(
      new BrowserProcessSubThread(BrowserThread::DB));
  if (!thread->Start())
    return;
  db_thread_.swap(thread);
}

void BrowserProcessImpl::CreateProcessLauncherThread() {
  DCHECK(!created_process_launcher_thread_ && !process_launcher_thread_.get());
  created_process_launcher_thread_ = true;

  scoped_ptr<base::Thread> thread(
      new BrowserProcessSubThread(BrowserThread::PROCESS_LAUNCHER));
  if (!thread->Start())
    return;
  process_launcher_thread_.swap(thread);
}

void BrowserProcessImpl::CreateCacheThread() {
  DCHECK(!created_cache_thread_ && !cache_thread_.get());
  created_cache_thread_ = true;

  scoped_ptr<base::Thread> thread(
      new BrowserThread(BrowserThread::CACHE));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  cache_thread_.swap(thread);
}

void BrowserProcessImpl::CreateProfileManager() {
  DCHECK(!created_profile_manager_ && profile_manager_.get() == NULL);
  created_profile_manager_ = true;

  profile_manager_.reset(new ProfileManager());
}

void BrowserProcessImpl::CreateLocalState() {
  DCHECK(!created_local_state_ && local_state_.get() == NULL);
  created_local_state_ = true;

  FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  local_state_.reset(
      PrefService::CreatePrefService(local_state_path, NULL, NULL));

  pref_change_registrar_.Init(local_state_.get());

  // Make sure the the plugin updater gets notifications of changes
  // in the plugin blacklist.
  local_state_->RegisterListPref(prefs::kPluginsPluginsBlacklist);
  pref_change_registrar_.Add(prefs::kPluginsPluginsBlacklist,
                             PluginUpdater::GetInstance());

  // Initialize and set up notifications for the printing enabled
  // preference.
  local_state_->RegisterBooleanPref(prefs::kPrintingEnabled, true);
  bool printing_enabled =
      local_state_->GetBoolean(prefs::kPrintingEnabled);
  print_job_manager_->set_printing_enabled(printing_enabled);
  pref_change_registrar_.Add(prefs::kPrintingEnabled,
                             print_job_manager_.get());

  // Initialize the notification for the default browser setting policy.
  local_state_->RegisterBooleanPref(prefs::kDefaultBrowserSettingEnabled,
                                    false);
  if (local_state_->IsManagedPreference(prefs::kDefaultBrowserSettingEnabled)) {
    if (local_state_->GetBoolean(prefs::kDefaultBrowserSettingEnabled))
      ShellIntegration::SetAsDefaultBrowser();
  }
  pref_change_registrar_.Add(prefs::kDefaultBrowserSettingEnabled, this);

  // Initialize the preference for the plugin finder policy.
  // This preference is only needed on the IO thread so make it available there.
  local_state_->RegisterBooleanPref(prefs::kDisablePluginFinder, false);
  plugin_finder_disabled_pref_.Init(prefs::kDisablePluginFinder,
                                   local_state_.get(), NULL);
  plugin_finder_disabled_pref_.MoveToThread(BrowserThread::IO);
}

void BrowserProcessImpl::CreateIconManager() {
  DCHECK(!created_icon_manager_ && icon_manager_.get() == NULL);
  created_icon_manager_ = true;
  icon_manager_.reset(new IconManager);
}

void BrowserProcessImpl::CreateDevToolsManager() {
  DCHECK(devtools_manager_.get() == NULL);
  created_devtools_manager_ = true;
  devtools_manager_ = new DevToolsManager();
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

void BrowserProcessImpl::CreatePrintPreviewTabController() {
  DCHECK(print_preview_tab_controller_.get() == NULL);
  print_preview_tab_controller_ = new printing::PrintPreviewTabController();
}

void BrowserProcessImpl::CreateSafeBrowsingDetectionService() {
  DCHECK(safe_browsing_detection_service_.get() == NULL);
  // Set this flag to true so that we don't retry indefinitely to
  // create the service class if there was an error.
  created_safe_browsing_detection_service_ = true;

  FilePath model_file_path;
  Profile* profile = profile_manager() ?
    profile_manager()->GetDefaultProfile() : NULL;
  if (IsSafeBrowsingDetectionServiceEnabled() &&
      PathService::Get(chrome::DIR_USER_DATA, &model_file_path) &&
      profile && profile->GetRequestContext()) {
    safe_browsing_detection_service_.reset(
        safe_browsing::ClientSideDetectionService::Create(
            model_file_path.Append(chrome::kSafeBrowsingPhishingModelFilename),
            profile->GetRequestContext()));
  }
}

bool BrowserProcessImpl::IsSafeBrowsingDetectionServiceEnabled() {
  // The safe browsing client-side detection is enabled only if the switch is
  // enabled and when safe browsing related stats is allowed to be collected.
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableClientSidePhishingDetection) &&
      resource_dispatcher_host()->safe_browsing_service() &&
      resource_dispatcher_host()->safe_browsing_service()->CanReportStats();
}

// The BrowserProcess object must outlive the file thread so we use traits
// which don't do any management.
DISABLE_RUNNABLE_METHOD_REFCOUNT(BrowserProcessImpl);

#if defined(IPC_MESSAGE_LOG_ENABLED)

void BrowserProcessImpl::SetIPCLoggingEnabled(bool enable) {
  // First enable myself.
  if (enable)
    IPC::Logging::GetInstance()->Enable();
  else
    IPC::Logging::GetInstance()->Disable();

  // Now tell subprocesses.  Messages to ChildProcess-derived
  // processes must be done on the IO thread.
  io_thread()->message_loop()->PostTask
      (FROM_HERE,
       NewRunnableMethod(
           this,
           &BrowserProcessImpl::SetIPCLoggingEnabledForChildProcesses,
           enable));

  // Finally, tell the renderers which don't derive from ChildProcess.
  // Messages to the renderers must be done on the UI (main) thread.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    i.GetCurrentValue()->Send(new ViewMsg_SetIPCLoggingEnabled(enable));
}

// Helper for SetIPCLoggingEnabled.
void BrowserProcessImpl::SetIPCLoggingEnabledForChildProcesses(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserChildProcessHost::Iterator i;  // default constr references a singleton
  while (!i.Done()) {
    i->Send(new PluginProcessMsg_SetIPCLoggingEnabled(enabled));
    ++i;
  }
}

#endif  // IPC_MESSAGE_LOG_ENABLED

void BrowserProcessImpl::DoInspectorFilesCheck() {
  // Runs on FILE thread.
  DCHECK(file_thread_->message_loop() == MessageLoop::current());
  bool result = false;

  FilePath inspector_dir;
  if (PathService::Get(chrome::DIR_INSPECTOR, &inspector_dir)) {
    result = file_util::PathExists(inspector_dir);
  }

  have_inspector_files_ = result;
}

// Mac is currently not supported.
#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)

bool BrowserProcessImpl::CanAutorestartForUpdate() const {
  // Check if browser is in the background and if it needs to be restarted to
  // apply a pending update.
  return BrowserList::size() == 0 && BrowserList::WillKeepAlive() &&
         Upgrade::IsUpdatePendingRestart();
}

// Switches to add when auto-restarting Chrome.
const char* const kSwitchesToAddOnAutorestart[] = {
  switches::kNoStartupWindow
};

void BrowserProcessImpl::RestartPersistentInstance() {
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
  BrowserList::CloseAllBrowsersAndExit();

  // Transfer ownership to Upgrade.
  Upgrade::SetNewCommandLine(new_cl.release());
}

void BrowserProcessImpl::OnAutoupdateTimer() {
  if (CanAutorestartForUpdate()) {
    DLOG(WARNING) << "Detected update.  Restarting browser.";
    RestartPersistentInstance();
  }
}

#endif  // (defined(OS_WIN) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
