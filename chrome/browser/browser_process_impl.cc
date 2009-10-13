// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_impl.h"

#include "app/clipboard/clipboard.h"
#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/browser/browser_trial.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/debugger/debugger_wrapper.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/download/download_file.h"
#include "chrome/browser/download/save_file_manager.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/plugin_service.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "ipc/ipc_logging.h"

#if defined(OS_WIN)
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "views/focus/view_storage.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/printing/print_job_manager.h"
#elif defined(OS_LINUX)
// TODO(port): Remove the temporary scaffolding as we port the above headers.
#include "chrome/common/temp_scaffolding_stubs.h"
#endif

#if defined(IPC_MESSAGE_LOG_ENABLED)
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#endif

namespace {

// ----------------------------------------------------------------------------
// BrowserProcessSubThread
//
// This simple thread object is used for the specialized threads that the
// BrowserProcess spins up.
//
// Applications must initialize the COM library before they can call
// COM library functions other than CoGetMalloc and memory allocation
// functions, so this class initializes COM for those users.
class BrowserProcessSubThread : public ChromeThread {
 public:
  explicit BrowserProcessSubThread(ChromeThread::ID identifier)
      : ChromeThread(identifier) {
  }

  virtual ~BrowserProcessSubThread() {
    // We cannot rely on our base class to stop the thread since we want our
    // CleanUp function to run.
    Stop();
  }

 protected:
  virtual void Init() {
#if defined(OS_WIN)
    // Initializes the COM library on the current thread.
    CoInitialize(NULL);
#endif

    notification_service_ = new NotificationService;
  }

  virtual void CleanUp() {
    delete notification_service_;
    notification_service_ = NULL;

#if defined(OS_WIN)
    // Closes the COM library on the current thread. CoInitialize must
    // be balanced by a corresponding call to CoUninitialize.
    CoUninitialize();
#endif
  }

 private:
  // Each specialized thread has its own notification service.
  // Note: We don't use scoped_ptr because the destructor runs on the wrong
  // thread.
  NotificationService* notification_service_;
};

class IOThread : public BrowserProcessSubThread {
 public:
  IOThread() : BrowserProcessSubThread(ChromeThread::IO) {}

  virtual ~IOThread() {
    // We cannot rely on our base class to stop the thread since we want our
    // CleanUp function to run.
    Stop();
  }

 protected:
  virtual void CleanUp() {
    // URLFetcher and URLRequest instances must NOT outlive the IO thread.
    //
    // Strictly speaking, URLFetcher's CheckForLeaks() should be done on the
    // UI thread. However, since there _shouldn't_ be any instances left
    // at this point, it shouldn't be a race.
    //
    // We check URLFetcher first, since if it has leaked then an associated
    // URLRequest will also have leaked. However it is more useful to
    // crash showing the callstack of URLFetcher's allocation than its
    // URLRequest member.
    base::LeakTracker<URLFetcher>::CheckForLeaks();
    base::LeakTracker<URLRequest>::CheckForLeaks();

    BrowserProcessSubThread::CleanUp();
  }
};

}  // namespace

BrowserProcessImpl::BrowserProcessImpl(const CommandLine& command_line)
    : created_resource_dispatcher_host_(false),
      created_metrics_service_(false),
      created_io_thread_(false),
      created_file_thread_(false),
      created_db_thread_(false),
      created_profile_manager_(false),
      created_local_state_(false),
#if defined(OS_WIN)
      initialized_broker_services_(false),
      broker_services_(NULL),
#endif  // defined(OS_WIN)
      created_icon_manager_(false),
      created_debugger_wrapper_(false),
      created_devtools_manager_(false),
      module_ref_count_(0),
      checked_for_new_frames_(false),
      using_new_frames_(false),
      have_inspector_files_(true) {
  g_browser_process = this;
  clipboard_.reset(new Clipboard);
  main_notification_service_.reset(new NotificationService);

  // Must be created after the NotificationService.
  print_job_manager_.reset(new printing::PrintJobManager);

  shutdown_event_.reset(new base::WaitableEvent(true, false));
}

BrowserProcessImpl::~BrowserProcessImpl() {
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

  // We need to destroy the MetricsService and GoogleURLTracker before the
  // io_thread_ gets destroyed, since both destructors can call the URLFetcher
  // destructor, which does an PostDelayedTask operation on the IO thread.  (The
  // IO thread will handle that URLFetcher operation before going away.)
  metrics_service_.reset();
  google_url_tracker_.reset();

  // Need to clear profiles (download managers) before the io_thread_.
  profile_manager_.reset();

  // Debugger must be cleaned up before IO thread and NotificationService.
  debugger_wrapper_ = NULL;

  if (resource_dispatcher_host_.get()) {
    // Need to tell Safe Browsing Service that the IO thread is going away
    // since it cached a pointer to it.
    if (resource_dispatcher_host()->safe_browsing_service())
      resource_dispatcher_host()->safe_browsing_service()->ShutDown();

    // Cancel pending requests and prevent new requests.
    resource_dispatcher_host()->Shutdown();
  }

#if defined(OS_LINUX)
  // The IO thread must outlive the BACKGROUND_X11 thread.
  background_x11_thread_.reset();
#endif

  // Need to stop io_thread_ before resource_dispatcher_host_, since
  // io_thread_ may still deref ResourceDispatcherHost and handle resource
  // request before going away.
  ResetIOThread();

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
  // SafeBrowsingService, since it caches a pointer to it.
  resource_dispatcher_host_.reset();

  // Wait for the pending print jobs to finish.
  print_job_manager_->OnQuit();
  print_job_manager_.reset();

  // Now OK to destroy NotificationService.
  main_notification_service_.reset();

  g_browser_process = NULL;
}

// Send a QuitTask to the given MessageLoop.
static void PostQuit(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void BrowserProcessImpl::EndSession() {
#if defined(OS_WIN)
  // Notify we are going away.
  ::SetEvent(shutdown_event_->handle());
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
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(PostQuit, MessageLoop::current()));
  MessageLoop::current()->Run();
}

printing::PrintJobManager* BrowserProcessImpl::print_job_manager() {
  // TODO(abarth): DCHECK(CalledOnValidThread());
  //               See <http://b/1287209>.
  // print_job_manager_ is initialized in the constructor and destroyed in the
  // destructor, so it should always be valid.
  DCHECK(print_job_manager_.get());
  return print_job_manager_.get();
}

const std::string& BrowserProcessImpl::GetApplicationLocale() {
  DCHECK(CalledOnValidThread());
  if (locale_.empty()) {
    locale_ = l10n_util::GetApplicationLocale(
        local_state()->GetString(prefs::kApplicationLocale));
  }
  return locale_;
}

void BrowserProcessImpl::CreateResourceDispatcherHost() {
  DCHECK(!created_resource_dispatcher_host_ &&
         resource_dispatcher_host_.get() == NULL);
  created_resource_dispatcher_host_ = true;

  resource_dispatcher_host_.reset(
      new ResourceDispatcherHost(io_thread()->message_loop()));
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

#if defined(OS_LINUX)
  // The lifetime of the BACKGROUND_X11 thread is a subset of the IO thread so
  // we start it now.
  scoped_ptr<base::Thread> background_x11_thread(
      new BrowserProcessSubThread(ChromeThread::BACKGROUND_X11));
  if (!background_x11_thread->Start())
    return;
  background_x11_thread_.swap(background_x11_thread);
#endif

  scoped_ptr<base::Thread> thread(new IOThread);
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  io_thread_.swap(thread);
}

void BrowserProcessImpl::ResetIOThread() {
  if (io_thread_.get()) {
    io_thread_->message_loop()->PostTask(FROM_HERE,
        NewRunnableFunction(CleanupOnIOThread));
  }
  io_thread_.reset();
}

// static
void BrowserProcessImpl::CleanupOnIOThread() {
  // Shutdown DNS prefetching now to ensure that network stack objects
  // living on the IO thread get destroyed before the IO thread goes away.
  chrome_browser_net::EnsureDnsPrefetchShutdown();
  // TODO(eroman): can this be merged into IOThread::CleanUp() ?
}

void BrowserProcessImpl::CreateFileThread() {
  DCHECK(!created_file_thread_ && file_thread_.get() == NULL);
  created_file_thread_ = true;

  scoped_ptr<base::Thread> thread(
      new BrowserProcessSubThread(ChromeThread::FILE));
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
      new BrowserProcessSubThread(ChromeThread::DB));
  if (!thread->Start())
    return;
  db_thread_.swap(thread);
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
  local_state_.reset(new PrefService(local_state_path, file_thread()));
}

#if defined(OS_WIN)
void BrowserProcessImpl::InitBrokerServices(
    sandbox::BrokerServices* broker_services) {
  DCHECK(!initialized_broker_services_ && broker_services_ == NULL);
  broker_services->Init();
  initialized_broker_services_ = true;
  broker_services_ = broker_services;
}
#endif  // defined(OS_WIN)

void BrowserProcessImpl::CreateIconManager() {
  DCHECK(!created_icon_manager_ && icon_manager_.get() == NULL);
  created_icon_manager_ = true;
  icon_manager_.reset(new IconManager);
}

void BrowserProcessImpl::CreateDebuggerWrapper(int port) {
  DCHECK(debugger_wrapper_.get() == NULL);
  created_debugger_wrapper_ = true;

  debugger_wrapper_ = new DebuggerWrapper(port);
}

void BrowserProcessImpl::CreateDevToolsManager() {
  DCHECK(devtools_manager_.get() == NULL);
  created_devtools_manager_ = true;
  devtools_manager_ = new DevToolsManager();
}

void BrowserProcessImpl::CreateGoogleURLTracker() {
  DCHECK(google_url_tracker_.get() == NULL);
  scoped_ptr<GoogleURLTracker> google_url_tracker(new GoogleURLTracker);
  google_url_tracker_.swap(google_url_tracker);
}

// The BrowserProcess object must outlive the file thread so we use traits
// which don't do any management.
template <>
struct RunnableMethodTraits<BrowserProcessImpl> {
  void RetainCallee(BrowserProcessImpl*) {}
  void ReleaseCallee(BrowserProcessImpl*) {}
};

void BrowserProcessImpl::CheckForInspectorFiles() {
  file_thread()->message_loop()->PostTask
      (FROM_HERE,
       NewRunnableMethod(this, &BrowserProcessImpl::DoInspectorFilesCheck));
}

#if defined(IPC_MESSAGE_LOG_ENABLED)

void BrowserProcessImpl::SetIPCLoggingEnabled(bool enable) {
  // First enable myself.
  if (enable)
    IPC::Logging::current()->Enable();
  else
    IPC::Logging::current()->Disable();

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
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  ChildProcessHost::Iterator i;  // default constr references a singleton
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
