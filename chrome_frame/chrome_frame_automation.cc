// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_automation.h"

#include "app/app_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/client_util.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/chrome_launcher_utils.h"
#include "chrome_frame/crash_reporting/crash_metrics.h"
#include "chrome_frame/custom_sync_call_context.h"
#include "chrome_frame/navigation_constraints.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/utils.h"
#include "ui/base/ui_base_switches.h"

#ifdef NDEBUG
int64 kAutomationServerReasonableLaunchDelay = 1000;  // in milliseconds
#else
int64 kAutomationServerReasonableLaunchDelay = 1000 * 10;
#endif

int kDefaultSendUMADataInterval = 20000;  // in milliseconds.

static const wchar_t kUmaSendIntervalValue[] = L"UmaSendInterval";

// This lock ensures that histograms created by ChromeFrame are thread safe.
// The histograms created in ChromeFrame can be initialized on multiple
// threads.
base::Lock g_ChromeFrameHistogramLock;

class ChromeFrameAutomationProxyImpl::TabProxyNotificationMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit TabProxyNotificationMessageFilter(AutomationHandleTracker* tracker)
      : tracker_(tracker) {
  }

  void AddTabProxy(AutomationHandle tab_proxy) {
    base::AutoLock lock(lock_);
    tabs_list_.push_back(tab_proxy);
  }

  void RemoveTabProxy(AutomationHandle tab_proxy) {
    base::AutoLock lock(lock_);
    tabs_list_.remove(tab_proxy);
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    if (message.is_reply())
      return false;

    if (!ChromeFrameDelegateImpl::IsTabMessage(message))
      return false;

    // Get AddRef-ed pointer to corresponding TabProxy object
    TabProxy* tab = static_cast<TabProxy*>(tracker_->GetResource(
        message.routing_id()));
    bool handled = false;
    if (tab) {
      handled = tab->OnMessageReceived(message);
      tab->Release();
    } else {
      DLOG(ERROR) << "Failed to find TabProxy for tab:" << message.routing_id();
      // To prevent subsequent crashes, we set handled to true in this case.
      handled = true;
    }
    return handled;
  }

  virtual void OnChannelError() {
    std::list<AutomationHandle>::const_iterator iter = tabs_list_.begin();
    for (; iter != tabs_list_.end(); ++iter) {
      // Get AddRef-ed pointer to corresponding TabProxy object
      TabProxy* tab = static_cast<TabProxy*>(tracker_->GetResource(*iter));
      if (tab) {
        tab->OnChannelError();
        tab->Release();
      }
    }
  }

 private:
  AutomationHandleTracker* tracker_;
  std::list<AutomationHandle> tabs_list_;
  base::Lock lock_;
};

class ChromeFrameAutomationProxyImpl::CFMsgDispatcher
    : public SyncMessageReplyDispatcher {
 public:
  CFMsgDispatcher() : SyncMessageReplyDispatcher() {}
 protected:
  virtual bool HandleMessageType(const IPC::Message& msg,
                                 SyncMessageCallContext* context) {
    switch (context->message_type()) {
      case AutomationMsg_CreateExternalTab::ID:
      case AutomationMsg_ConnectExternalTab::ID:
        InvokeCallback<CreateExternalTabContext>(msg, context);
        break;
      case AutomationMsg_NavigateExternalTabAtIndex::ID:
      case AutomationMsg_NavigateInExternalTab::ID:
        InvokeCallback<BeginNavigateContext>(msg, context);
        break;
      case AutomationMsg_InstallExtension::ID:
        InvokeCallback<InstallExtensionContext>(msg, context);
        break;
      case AutomationMsg_LoadExpandedExtension::ID:
        InvokeCallback<InstallExtensionContext>(msg, context);
        break;
      case AutomationMsg_GetEnabledExtensions::ID:
        InvokeCallback<GetEnabledExtensionsContext>(msg, context);
        break;
      case AutomationMsg_RunUnloadHandlers::ID:
        InvokeCallback<UnloadContext>(msg, context);
        break;
      default:
        NOTREACHED();
    }
    return true;
  }
};

ChromeFrameAutomationProxyImpl::ChromeFrameAutomationProxyImpl(
    AutomationProxyCacheEntry* entry,
    std::string channel_id, int launch_timeout)
    : AutomationProxy(launch_timeout, false), proxy_entry_(entry) {
  TRACE_EVENT_BEGIN("chromeframe.automationproxy", this, "");

  InitializeChannel(channel_id, false);

  sync_ = new CFMsgDispatcher();
  message_filter_ = new TabProxyNotificationMessageFilter(tracker_.get());

  // Order of filters is not important.
  channel_->AddFilter(message_filter_.get());
  channel_->AddFilter(sync_.get());
}

ChromeFrameAutomationProxyImpl::~ChromeFrameAutomationProxyImpl() {
  TRACE_EVENT_END("chromeframe.automationproxy", this, "");
}

void ChromeFrameAutomationProxyImpl::SendAsAsync(
    IPC::SyncMessage* msg,
    SyncMessageReplyDispatcher::SyncMessageCallContext* context, void* key) {
  sync_->Push(msg, context, key);
  channel_->ChannelProxy::Send(msg);
}

void ChromeFrameAutomationProxyImpl::CancelAsync(void* key) {
  sync_->Cancel(key);
}

void ChromeFrameAutomationProxyImpl::OnChannelError() {
  DLOG(ERROR) << "Automation server died";
  if (proxy_entry_) {
    proxy_entry_->OnChannelError();
  } else {
    NOTREACHED();
  }
}

scoped_refptr<TabProxy> ChromeFrameAutomationProxyImpl::CreateTabProxy(
    int handle) {
  DCHECK(tracker_->GetResource(handle) == NULL);
  TabProxy* tab_proxy = new TabProxy(this, tracker_.get(), handle);
  if (tab_proxy != NULL)
    message_filter_->AddTabProxy(handle);
  return tab_proxy;
}

void ChromeFrameAutomationProxyImpl::ReleaseTabProxy(AutomationHandle handle) {
  message_filter_->RemoveTabProxy(handle);
}

struct LaunchTimeStats {
#ifndef NDEBUG
  LaunchTimeStats() {
    launch_time_begin_ = base::Time::Now();
  }

  void Dump() {
    base::TimeDelta launch_time = base::Time::Now() - launch_time_begin_;
    THREAD_SAFE_UMA_HISTOGRAM_TIMES("ChromeFrame.AutomationServerLaunchTime",
                                    launch_time);
    const int64 launch_milliseconds = launch_time.InMilliseconds();
    if (launch_milliseconds > kAutomationServerReasonableLaunchDelay) {
      LOG(WARNING) << "Automation server launch took longer than expected: " <<
          launch_milliseconds << " ms.";
    }
  }

  base::Time launch_time_begin_;
#else
  void Dump() {}
#endif
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(AutomationProxyCacheEntry);

AutomationProxyCacheEntry::AutomationProxyCacheEntry(
    ChromeFrameLaunchParams* params, LaunchDelegate* delegate)
    : profile_name(params->profile_name()),
      launch_result_(AUTOMATION_LAUNCH_RESULT_INVALID),
      snapshots_(NULL), uma_send_interval_(1) {
  DCHECK(delegate);
  thread_.reset(new base::Thread(WideToASCII(profile_name).c_str()));
  thread_->Start();
  // Use scoped_refptr so that the params will get released when the task
  // has been run.
  scoped_refptr<ChromeFrameLaunchParams> ref_params(params);
  thread_->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &AutomationProxyCacheEntry::CreateProxy, ref_params, delegate));
}

AutomationProxyCacheEntry::~AutomationProxyCacheEntry() {
  DVLOG(1) << __FUNCTION__ << profile_name;
  // Attempt to fix chrome_frame_tests crash seen at times on the IE6/IE7
  // builders. It appears that there are cases when we can enter here when the
  // AtExitManager is tearing down the global ProxyCache which causes a crash
  // while tearing down the AutomationProxy object due to a NULL MessageLoop
  // The AutomationProxy class uses the SyncChannel which assumes the existence
  // of a MessageLoop instance.
  // We leak the AutomationProxy pointer here to avoid a crash.
  if (MessageLoop::current() == NULL) {
    proxy_.release();
  }
}

void AutomationProxyCacheEntry::StartSendUmaInterval(
    ChromeFrameHistogramSnapshots* snapshots, int send_interval) {
  DCHECK(snapshots);
  DCHECK(!snapshots_);
  snapshots_ = snapshots;
  uma_send_interval_ = send_interval;
  thread_->message_loop()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(this, &AutomationProxyCacheEntry::SendUMAData),
      send_interval);
}

void AutomationProxyCacheEntry::CreateProxy(ChromeFrameLaunchParams* params,
                                            LaunchDelegate* delegate) {
  DCHECK(IsSameThread(base::PlatformThread::CurrentId()));
  DCHECK(delegate);
  DCHECK(params);
  DCHECK(proxy_.get() == NULL);

  // We *must* create automationproxy in a thread that has message loop,
  // since SyncChannel::Context construction registers event to be watched
  // through ObjectWatcher which subscribes for the current thread message loop
  // destruction notification.

  // At same time we must destroy/stop the thread from another thread.
  std::string channel_id = AutomationProxy::GenerateChannelID();
  ChromeFrameAutomationProxyImpl* proxy =
      new ChromeFrameAutomationProxyImpl(this, channel_id,
                                         params->launch_timeout());

  // Ensure that the automation proxy actually respects our choice on whether
  // or not to check the version.
  proxy->set_perform_version_check(params->version_check());

  // Launch browser
  scoped_ptr<CommandLine> command_line(
      chrome_launcher::CreateLaunchCommandLine());
  command_line->AppendSwitchASCII(switches::kAutomationClientChannelID,
                                  channel_id);

  // Run Chrome in Chrome Frame mode. In practice, this modifies the paths
  // and registry keys that Chrome looks in via the BrowserDistribution
  // mechanism.
  command_line->AppendSwitch(switches::kChromeFrame);

  // Chrome Frame never wants Chrome to start up with a First Run UI.
  command_line->AppendSwitch(switches::kNoFirstRun);

  command_line->AppendSwitch(switches::kDisablePopupBlocking);

  // Disable the "Whoa! Chrome has crashed." dialog, because that isn't very
  // useful for Chrome Frame users.
#ifndef NDEBUG
  command_line->AppendSwitch(switches::kNoErrorDialogs);
#endif

  // In headless mode runs like reliability test runs we want full crash dumps
  // from chrome.
  if (IsHeadlessMode())
    command_line->AppendSwitch(switches::kFullMemoryCrashReport);

  // In accessible mode automation tests expect renderer accessibility to be
  // enabled in chrome.
  if (IsAccessibleMode())
    command_line->AppendSwitch(switches::kForceRendererAccessibility);

  DVLOG(1) << "Profile path: " << params->profile_path().value();
  command_line->AppendSwitchPath(switches::kUserDataDir,
                                 params->profile_path());

  // Ensure that Chrome is running the specified version of chrome.dll.
  command_line->AppendSwitchNative(switches::kChromeVersion,
                                   GetCurrentModuleVersion());

  if (!params->language().empty())
    command_line->AppendSwitchNative(switches::kLang, params->language());

  std::wstring command_line_string(command_line->command_line_string());
  // If there are any extra arguments, append them to the command line.
  if (!params->extra_arguments().empty()) {
    command_line_string += L' ' + params->extra_arguments();
  }

  automation_server_launch_start_time_ = base::TimeTicks::Now();

  if (!base::LaunchApp(command_line_string, false, false, NULL)) {
    // We have no code for launch failure.
    launch_result_ = AUTOMATION_LAUNCH_RESULT_INVALID;
  } else {
    // Launch timeout may happen if the new instance tries to communicate
    // with an existing Chrome instance that is hung and displays msgbox
    // asking to kill the previous one. This could be easily observed if the
    // already running Chrome instance is running as high-integrity process
    // (started with "Run as Administrator" or launched by another high
    // integrity process) hence our medium-integrity process
    // cannot SendMessage to it with request to activate itself.

    // TODO(stoyan) AutomationProxy eats Hello message, hence installing
    // message filter is pointless, we can leverage ObjectWatcher and use
    // system thread pool to notify us when proxy->AppLaunch event is signaled.
    LaunchTimeStats launch_stats;
    // Wait for the automation server launch result, then stash away the
    // version string it reported.
    launch_result_ = proxy->WaitForAppLaunch();
    launch_stats.Dump();

    base::TimeDelta delta =
        base::TimeTicks::Now() - automation_server_launch_start_time_;

    if (launch_result_ == AUTOMATION_SUCCESS) {
      THREAD_SAFE_UMA_HISTOGRAM_TIMES(
          "ChromeFrame.AutomationServerLaunchSuccessTime", delta);
    } else {
      THREAD_SAFE_UMA_HISTOGRAM_TIMES(
          "ChromeFrame.AutomationServerLaunchFailedTime", delta);
    }

    THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeFrame.LaunchResult",
                                            launch_result_,
                                            AUTOMATION_SUCCESS,
                                            AUTOMATION_CREATE_TAB_FAILED,
                                            AUTOMATION_CREATE_TAB_FAILED + 1);
  }

  TRACE_EVENT_END("chromeframe.createproxy", this, "");

  // Finally set the proxy.
  proxy_.reset(proxy);
  launch_delegates_.push_back(delegate);

  delegate->LaunchComplete(proxy_.get(), launch_result_);
}

void AutomationProxyCacheEntry::RemoveDelegate(LaunchDelegate* delegate,
                                               base::WaitableEvent* done,
                                               bool* was_last_delegate) {
  DCHECK(IsSameThread(base::PlatformThread::CurrentId()));
  DCHECK(delegate);
  DCHECK(done);
  DCHECK(was_last_delegate);

  *was_last_delegate = false;

  LaunchDelegates::iterator it = std::find(launch_delegates_.begin(),
      launch_delegates_.end(), delegate);
  if (it == launch_delegates_.end()) {
    NOTREACHED();
  } else {
    if (launch_delegates_.size() == 1) {
      *was_last_delegate = true;

      if (snapshots_)
        SendUMAData();

      // Process pending notifications.
      thread_->message_loop()->RunAllPending();

      // Take down the proxy since we no longer have any clients.
      // Make sure we only do this once all pending messages have been cleared.
      proxy_.reset(NULL);
    }
    // Be careful to remove from the list after running pending
    // tasks.  Otherwise the delegate being removed might miss out
    // on pending notifications such as LaunchComplete.
    launch_delegates_.erase(it);
  }

  done->Signal();
}

void AutomationProxyCacheEntry::AddDelegate(LaunchDelegate* delegate) {
  DCHECK(IsSameThread(base::PlatformThread::CurrentId()));
  DCHECK(std::find(launch_delegates_.begin(),
                   launch_delegates_.end(),
                   delegate) == launch_delegates_.end())
      << "Same delegate being added twice";
  DCHECK(launch_result_ != AUTOMATION_LAUNCH_RESULT_INVALID);

  launch_delegates_.push_back(delegate);
  delegate->LaunchComplete(proxy_.get(), launch_result_);
}

void AutomationProxyCacheEntry::OnChannelError() {
  DCHECK(IsSameThread(base::PlatformThread::CurrentId()));
  launch_result_ = AUTOMATION_SERVER_CRASHED;
  LaunchDelegates::const_iterator it = launch_delegates_.begin();
  for (; it != launch_delegates_.end(); ++it) {
    (*it)->AutomationServerDied();
  }
}

void AutomationProxyCacheEntry::SendUMAData() {
  DCHECK(IsSameThread(base::PlatformThread::CurrentId()));
  DCHECK(snapshots_);
  // IE uses the chrome frame provided UMA data uploading scheme. NPAPI
  // continues to use Chrome to upload UMA data.
  if (CrashMetricsReporter::GetInstance()->active()) {
    return;
  }

  if (!proxy_.get()) {
    DLOG(WARNING) << __FUNCTION__ << " NULL proxy, can't send UMA data";
  } else {
    ChromeFrameHistogramSnapshots::HistogramPickledList histograms =
        snapshots_->GatherAllHistograms();

    if (!histograms.empty()) {
      proxy_->Send(new AutomationMsg_RecordHistograms(histograms));
    }

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this, &AutomationProxyCacheEntry::SendUMAData),
        uma_send_interval_);
  }
}


DISABLE_RUNNABLE_METHOD_REFCOUNT(ProxyFactory);

ProxyFactory::ProxyFactory()
    : uma_send_interval_(0) {
  uma_send_interval_ = GetConfigInt(kDefaultSendUMADataInterval,
                                    kUmaSendIntervalValue);
}

ProxyFactory::~ProxyFactory() {
  for (size_t i = 0; i < proxies_.container().size(); ++i) {
    DWORD result = proxies_[i]->WaitForThread(0);
    if (WAIT_OBJECT_0 != result)
      // TODO(stoyan): Don't leak proxies on exit.
      DLOG(ERROR) << "Proxies leaked on exit.";
  }
}

void ProxyFactory::GetAutomationServer(
    LaunchDelegate* delegate, ChromeFrameLaunchParams* params,
    void** automation_server_id) {
  TRACE_EVENT_BEGIN("chromeframe.createproxy", this, "");

  scoped_refptr<AutomationProxyCacheEntry> entry;
  // Find already existing launcher thread for given profile
  base::AutoLock lock(lock_);
  for (size_t i = 0; i < proxies_.container().size(); ++i) {
    if (proxies_[i]->IsSameProfile(params->profile_name())) {
      entry = proxies_[i];
      break;
    }
  }

  if (entry == NULL) {
    DVLOG(1) << __FUNCTION__ << " creating new proxy entry";
    entry = new AutomationProxyCacheEntry(params, delegate);
    proxies_.container().push_back(entry);

    // IE uses the chrome frame provided UMA data uploading scheme. NPAPI
    // continues to use Chrome to upload UMA data.
    if (!CrashMetricsReporter::GetInstance()->active()) {
      entry->StartSendUmaInterval(&chrome_frame_histograms_,
                                  uma_send_interval_);
    }
  } else if (delegate) {
    // Notify the new delegate of the launch status from the worker thread
    // and add it to the list of delegates.
    entry->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(entry.get(),
        &AutomationProxyCacheEntry::AddDelegate, delegate));
  }

  DCHECK(automation_server_id != NULL);
  DCHECK(!entry->IsSameThread(base::PlatformThread::CurrentId()));

  *automation_server_id = entry;
}

bool ProxyFactory::ReleaseAutomationServer(void* server_id,
                                           LaunchDelegate* delegate) {
  if (!server_id) {
    NOTREACHED();
    return false;
  }

  AutomationProxyCacheEntry* entry =
      reinterpret_cast<AutomationProxyCacheEntry*>(server_id);

#ifndef NDEBUG
  lock_.Acquire();
  Vector::ContainerType::iterator it = std::find(proxies_.container().begin(),
                                                 proxies_.container().end(),
                                                 entry);
  DCHECK(it != proxies_.container().end());
  DCHECK(!entry->IsSameThread(base::PlatformThread::CurrentId()));

  lock_.Release();
#endif

  // AddRef the entry object as we might need to take it out of the proxy
  // stack and then uninitialize the entry.
  entry->AddRef();

  bool last_delegate = false;
  if (delegate) {
    base::WaitableEvent done(true, false);
    entry->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(entry,
        &AutomationProxyCacheEntry::RemoveDelegate, delegate, &done,
        &last_delegate));
    done.Wait();
  }

  if (last_delegate) {
    lock_.Acquire();
    Vector::ContainerType::iterator it = std::find(proxies_.container().begin(),
                                                   proxies_.container().end(),
                                                   entry);
    proxies_.container().erase(it);
    lock_.Release();
  }

  entry->Release();

  return true;
}

static base::LazyInstance<ProxyFactory,
                          base::LeakyLazyInstanceTraits<ProxyFactory> >
    g_proxy_factory(base::LINKER_INITIALIZED);

template <> struct RunnableMethodTraits<ChromeFrameAutomationClient> {
  static void RetainCallee(ChromeFrameAutomationClient* obj) {}
  static void ReleaseCallee(ChromeFrameAutomationClient* obj) {}
};

ChromeFrameAutomationClient::ChromeFrameAutomationClient()
    : chrome_frame_delegate_(NULL),
      chrome_window_(NULL),
      tab_window_(NULL),
      parent_window_(NULL),
      automation_server_(NULL),
      automation_server_id_(NULL),
      ui_thread_id_(NULL),
      init_state_(UNINITIALIZED),
      use_chrome_network_(false),
      proxy_factory_(g_proxy_factory.Pointer()),
      handle_top_level_requests_(false),
      tab_handle_(-1),
      session_id_(-1),
      external_tab_cookie_(0),
      url_fetcher_(NULL),
      url_fetcher_flags_(PluginUrlRequestManager::NOT_THREADSAFE),
      navigate_after_initialization_(false),
      route_all_top_level_navigations_(false) {
}

ChromeFrameAutomationClient::~ChromeFrameAutomationClient() {
  // Uninitialize must be called prior to the destructor
  DCHECK(automation_server_ == NULL);
}

bool ChromeFrameAutomationClient::Initialize(
    ChromeFrameDelegate* chrome_frame_delegate,
    ChromeFrameLaunchParams* chrome_launch_params) {
  DCHECK(!IsWindow());
  chrome_frame_delegate_ = chrome_frame_delegate;

#ifndef NDEBUG
  if (chrome_launch_params_ && chrome_launch_params_ != chrome_launch_params) {
    DCHECK_EQ(chrome_launch_params_->url(), chrome_launch_params->url());
    DCHECK_EQ(chrome_launch_params_->referrer(),
              chrome_launch_params->referrer());
  }
#endif

  chrome_launch_params_ = chrome_launch_params;

  ui_thread_id_ = base::PlatformThread::CurrentId();
#ifndef NDEBUG
  // In debug mode give more time to work with a debugger.
  if (IsDebuggerPresent()) {
    // Don't use INFINITE (which is -1) or even MAXINT since we will convert
    // from milliseconds to microseconds when stored in a base::TimeDelta,
    // thus * 1000. An hour should be enough.
    chrome_launch_params_->set_launch_timeout(60 * 60 * 1000);
  } else {
    DCHECK_LT(chrome_launch_params_->launch_timeout(),
              MAXINT / 2000);
    chrome_launch_params_->set_launch_timeout(
        chrome_launch_params_->launch_timeout() * 2);
  }
#endif  // NDEBUG

  // Create a window on the UI thread for marshaling messages back and forth
  // from the IPC thread. This window cannot be a message only window as the
  // external chrome tab window is created as a child of this window. This
  // window is eventually reparented to the ActiveX/NPAPI plugin window.
  if (!Create(GetDesktopWindow(), NULL, NULL,
              WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
              WS_EX_TOOLWINDOW)) {
    NOTREACHED();
    return false;
  }

  // Keep object in memory, while the window is alive.
  // Corresponding Release is in OnFinalMessage();
  AddRef();

  // Mark our state as initializing.  We'll reach initialized once
  // InitializeComplete is called successfully.
  init_state_ = INITIALIZING;

  HRESULT hr = S_OK;

  if (chrome_launch_params_->url().is_valid())
    navigate_after_initialization_ = false;

  proxy_factory_->GetAutomationServer(static_cast<LaunchDelegate*>(this),
      chrome_launch_params_, &automation_server_id_);

  return true;
}

void ChromeFrameAutomationClient::Uninitialize() {
  if (init_state_ == UNINITIALIZED) {
    DLOG(WARNING) << __FUNCTION__ << ": Automation client not initialized";
    return;
  }

  init_state_ = UNINITIALIZING;

  // Called from client's FinalRelease() / destructor
  // ChromeFrameAutomationClient may wait for the initialization (launch)
  // to complete while Uninitialize is called.
  // We either have to:
  // 1) Make ReleaseAutomationServer blocking call (wait until thread exits)
  // 2) Behave like a COM object i.e. increase module lock count.
  // Otherwise the DLL may get unloaded while we have running threads.
  // Unfortunately in NPAPI case we cannot increase module lock count, hence
  // we stick with blocking/waiting
  if (url_fetcher_) {
    // Clean up any outstanding requests
    url_fetcher_->StopAllRequests();
    url_fetcher_ = NULL;
  }

  if (tab_) {
    tab_->RemoveObserver(this);
    if (automation_server_)
      automation_server_->ReleaseTabProxy(tab_->handle());
    tab_ = NULL;    // scoped_refptr::Release
  }

  // Wait for the background thread to exit.
  ReleaseAutomationServer();

  // We must destroy the window, since if there are pending tasks
  // window procedure may be invoked after DLL is unloaded.
  // Unfortunately pending tasks are leaked.
  if (::IsWindow(m_hWnd))
    DestroyWindow();

  // DCHECK(navigate_after_initialization_ == false);
  handle_top_level_requests_ = false;
  ui_thread_id_ = 0;
  chrome_frame_delegate_ = NULL;
  init_state_ = UNINITIALIZED;
}

bool ChromeFrameAutomationClient::InitiateNavigation(
    const std::string& url,
    const std::string& referrer,
    NavigationConstraints* navigation_constraints) {
  if (url.empty())
    return false;

  GURL parsed_url(url);

  // Catch invalid URLs early.
  // Can we allow this navigation to happen?
  if (!CanNavigate(parsed_url, navigation_constraints)) {
    DLOG(ERROR) << __FUNCTION__ << " Not allowing navigation to: " << url;
    return false;
  }

  // If we are not yet initialized ignore attempts to navigate to the same url.
  // Navigation attempts to the same URL could occur if the automation client
  // was reused for a new active document instance.
  if (!chrome_launch_params_ || is_initialized() ||
      parsed_url != chrome_launch_params_->url()) {
    // Important: Since we will be using the referrer_ variable from a
    // different thread, we need to force a new std::string buffer instance for
    // the referrer_ GURL variable.  Otherwise we can run into strangeness when
    // the GURL is accessed and it could result in a bad URL that can cause the
    // referrer to be dropped or something worse.
    GURL referrer_gurl(referrer.c_str());
    if (!chrome_launch_params_) {
      FilePath profile_path;
      chrome_launch_params_ = new ChromeFrameLaunchParams(parsed_url,
          referrer_gurl, profile_path, L"", SimpleResourceLoader::GetLanguage(),
          L"", false, false, route_all_top_level_navigations_);
    } else {
      chrome_launch_params_->set_referrer(referrer_gurl);
      chrome_launch_params_->set_url(parsed_url);
    }

    navigate_after_initialization_ = false;

    if (is_initialized()) {
      BeginNavigate();
    } else {
      navigate_after_initialization_ = true;
    }
  }

  return true;
}

bool ChromeFrameAutomationClient::NavigateToIndex(int index) {
  // Could be NULL if we failed to launch Chrome in LaunchAutomationServer()
  if (!automation_server_ || !tab_.get() || !tab_->is_valid()) {
    return false;
  }

  DCHECK(::IsWindow(chrome_window_));

  IPC::SyncMessage* msg = new AutomationMsg_NavigateExternalTabAtIndex(
      tab_->handle(), index, NULL);
  automation_server_->SendAsAsync(msg, new BeginNavigateContext(this),
                                  this);
  return true;
}

bool ChromeFrameAutomationClient::ForwardMessageFromExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  // Could be NULL if we failed to launch Chrome in LaunchAutomationServer()
  if (!is_initialized())
    return false;

  tab_->HandleMessageFromExternalHost(message, origin, target);
  return true;
}

bool ChromeFrameAutomationClient::SetProxySettings(
    const std::string& json_encoded_proxy_settings) {
  if (!is_initialized())
    return false;
  automation_server_->SendProxyConfig(json_encoded_proxy_settings);
  return true;
}

void ChromeFrameAutomationClient::BeginNavigate() {
  // Could be NULL if we failed to launch Chrome in LaunchAutomationServer()
  if (!automation_server_ || !tab_.get()) {
    DLOG(WARNING) << "BeginNavigate - can't navigate.";
    ReportNavigationError(AUTOMATION_MSG_NAVIGATION_ERROR,
                          chrome_launch_params_->url().spec());
    return;
  }

  DCHECK(::IsWindow(chrome_window_));

  if (!tab_->is_valid()) {
    DLOG(WARNING) << "BeginNavigate - tab isn't valid.";
    return;
  }

  IPC::SyncMessage* msg =
      new AutomationMsg_NavigateInExternalTab(tab_->handle(),
          chrome_launch_params_->url(), chrome_launch_params_->referrer(),
          NULL);
  automation_server_->SendAsAsync(msg, new BeginNavigateContext(this), this);

  RECT client_rect = {0};
  chrome_frame_delegate_->GetBounds(&client_rect);
  Resize(client_rect.right - client_rect.left,
         client_rect.bottom - client_rect.top,
         SWP_NOACTIVATE | SWP_NOZORDER);
}

void ChromeFrameAutomationClient::BeginNavigateCompleted(
    AutomationMsg_NavigationResponseValues result) {
  if (result == AUTOMATION_MSG_NAVIGATION_ERROR)
     ReportNavigationError(AUTOMATION_MSG_NAVIGATION_ERROR,
                           chrome_launch_params_->url().spec());
}

void ChromeFrameAutomationClient::FindInPage(const std::wstring& search_string,
                                             FindInPageDirection forward,
                                             FindInPageCase match_case,
                                             bool find_next) {
  DCHECK(tab_.get());

  // What follows is quite similar to TabProxy::FindInPage() but uses
  // the SyncMessageReplyDispatcher to avoid concerns about blocking
  // synchronous messages.
  AutomationMsg_Find_Params params;
  params.unused = 0;
  params.search_string = WideToUTF16Hack(search_string);
  params.find_next = find_next;
  params.match_case = (match_case == CASE_SENSITIVE);
  params.forward = (forward == FWD);

  IPC::SyncMessage* msg =
      new AutomationMsg_Find(tab_->handle(), params, NULL, NULL);
  automation_server_->SendAsAsync(msg, NULL, this);
}

void ChromeFrameAutomationClient::InstallExtension(
    const FilePath& crx_path,
    void* user_data) {
  if (automation_server_ == NULL) {
    InstallExtensionComplete(crx_path,
                             user_data,
                             AUTOMATION_MSG_EXTENSION_INSTALL_FAILED);
    return;
  }

  InstallExtensionContext* ctx = new InstallExtensionContext(
      this, crx_path, user_data);

  IPC::SyncMessage* msg = new AutomationMsg_InstallExtension(crx_path, NULL);

  // The context will delete itself after it is called.
  automation_server_->SendAsAsync(msg, ctx, this);
}

void ChromeFrameAutomationClient::InstallExtensionComplete(
    const FilePath& crx_path,
    void* user_data,
    AutomationMsg_ExtensionResponseValues res) {
  DCHECK_EQ(base::PlatformThread::CurrentId(), ui_thread_id_);

  if (chrome_frame_delegate_) {
    chrome_frame_delegate_->OnExtensionInstalled(crx_path, user_data, res);
  }
}

void ChromeFrameAutomationClient::GetEnabledExtensions(void* user_data) {
    if (automation_server_ == NULL) {
      GetEnabledExtensionsComplete(user_data, &std::vector<FilePath>());
      return;
    }

    GetEnabledExtensionsContext* ctx = new GetEnabledExtensionsContext(
        this, user_data);

    IPC::SyncMessage* msg = new AutomationMsg_GetEnabledExtensions(
        ctx->extension_directories());

    // The context will delete itself after it is called.
    automation_server_->SendAsAsync(msg, ctx, this);
}

void ChromeFrameAutomationClient::GetEnabledExtensionsComplete(
    void* user_data,
    std::vector<FilePath>* extension_directories) {
  DCHECK_EQ(base::PlatformThread::CurrentId(), ui_thread_id_);

  if (chrome_frame_delegate_) {
    chrome_frame_delegate_->OnGetEnabledExtensionsComplete(
        user_data, *extension_directories);
  }

  delete extension_directories;
}

int ChromeFrameAutomationClient::GetSessionId() const {
  return session_id_;
}

void ChromeFrameAutomationClient::OnChromeFrameHostMoved() {
  // Use a local var to avoid the small possibility of getting the tab_
  // member be cleared while we try to use it.
  // Note that TabProxy is a RefCountedThreadSafe object, so we should be OK.
  scoped_refptr<TabProxy> tab(tab_);
  // There also is a possibility that tab_ has not been set yet,
  // so we still need to test for NULL.
  if (tab)
    tab->OnHostMoved();
}

void ChromeFrameAutomationClient::LoadExpandedExtension(
    const FilePath& path,
    void* user_data) {
  if (automation_server_ == NULL) {
    InstallExtensionComplete(path,
                             user_data,
                             AUTOMATION_MSG_EXTENSION_INSTALL_FAILED);
    return;
  }

  InstallExtensionContext* ctx = new InstallExtensionContext(
      this, path, user_data);

  IPC::SyncMessage* msg = new AutomationMsg_LoadExpandedExtension(path, NULL);

  // The context will delete itself after it is called.
  automation_server_->SendAsAsync(msg, ctx, this);
}

void ChromeFrameAutomationClient::CreateExternalTab() {
  AutomationLaunchResult launch_result = AUTOMATION_SUCCESS;
  DCHECK(IsWindow());
  DCHECK(automation_server_ != NULL);

  if (chrome_launch_params_->url().is_valid()) {
    navigate_after_initialization_ = false;
  }

  const ExternalTabSettings settings(
      m_hWnd,
      gfx::Rect(),
      WS_CHILD,
      chrome_launch_params_->incognito(),
      !use_chrome_network_,
      handle_top_level_requests_,
      chrome_launch_params_->url(),
      chrome_launch_params_->referrer(),
      // Infobars disabled in widget mode.
      !chrome_launch_params_->widget_mode(),
      chrome_launch_params_->route_all_top_level_navigations());

  THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS(
      "ChromeFrame.HostNetworking", !use_chrome_network_, 0, 1, 2);

  THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS(
      "ChromeFrame.HandleTopLevelRequests", handle_top_level_requests_, 0, 1,
      2);

  IPC::SyncMessage* message =
      new AutomationMsg_CreateExternalTab(settings, NULL, NULL, 0, 0);
  automation_server_->SendAsAsync(message, new CreateExternalTabContext(this),
                                  this);
}

AutomationLaunchResult ChromeFrameAutomationClient::CreateExternalTabComplete(
    HWND chrome_window, HWND tab_window, int tab_handle, int session_id) {
  if (!automation_server_) {
    // If we receive this notification while shutting down, do nothing.
    DLOG(ERROR) << "CreateExternalTabComplete called when automation server "
                << "was null!";
    return AUTOMATION_CREATE_TAB_FAILED;
  }

  AutomationLaunchResult launch_result = AUTOMATION_SUCCESS;
  if (tab_handle == 0 || !::IsWindow(chrome_window)) {
    launch_result = AUTOMATION_CREATE_TAB_FAILED;
  } else {
    chrome_window_ = chrome_window;
    tab_window_ = tab_window;
    tab_ = automation_server_->CreateTabProxy(tab_handle);
    tab_->AddObserver(this);
    tab_handle_ = tab_handle;
    session_id_ = session_id;
  }
  return launch_result;
}

void ChromeFrameAutomationClient::SetEnableExtensionAutomation(
    const std::vector<std::string>& functions_enabled) {
  if (!is_initialized())
    return;

  // We are doing initialization, so there is no need to reset extension
  // automation, only to set it.  Also, we want to avoid resetting extension
  // automation that some other automation client has set up.  Therefore only
  // send the message if we are going to enable automation of some functions.
  if (!functions_enabled.empty())
    tab_->SetEnableExtensionAutomation(functions_enabled);
}

// Invoked in launch background thread.
void ChromeFrameAutomationClient::LaunchComplete(
    ChromeFrameAutomationProxy* proxy,
    AutomationLaunchResult result) {
  // If we're shutting down we don't keep a pointer to the automation server.
  if (init_state_ != UNINITIALIZING) {
    DCHECK(init_state_ == INITIALIZING);
    automation_server_ = proxy;
  } else {
    DVLOG(1) << "Not storing automation server pointer due to shutting down";
  }

  if (result == AUTOMATION_SUCCESS) {
    // NOTE: A potential problem here is that Uninitialize() may just have
    // been called so we need to be careful and check the automation_server_
    // pointer.
    if (automation_server_ != NULL) {
      // If we have a valid tab_handle here it means that we are attaching to
      // an existing ExternalTabContainer instance, in which case we don't
      // want to create an external tab instance in Chrome.
      if (external_tab_cookie_ == 0) {
        // Continue with Initialization - Create external tab
        CreateExternalTab();
      } else {
        // Send a notification to Chrome that we are ready to connect to the
        // ExternalTab.
        IPC::SyncMessage* message =
            new AutomationMsg_ConnectExternalTab(external_tab_cookie_, true,
              m_hWnd, NULL, NULL, NULL, 0);
        automation_server_->SendAsAsync(message,
                                        new CreateExternalTabContext(this),
                                        this);
        DVLOG(1) << __FUNCTION__ << ": sending CreateExternalTabComplete";
      }
    }
  } else {
    // Launch failed. Note, we cannot delete proxy here.
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &ChromeFrameAutomationClient::InitializeComplete, result));
  }
}

void ChromeFrameAutomationClient::AutomationServerDied() {
  // Make sure we notify our delegate.
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::InitializeComplete,
      AUTOMATION_SERVER_CRASHED));
  // Then uninitialize.
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::Uninitialize));
}

void ChromeFrameAutomationClient::InitializeComplete(
    AutomationLaunchResult result) {
  DCHECK_EQ(base::PlatformThread::CurrentId(), ui_thread_id_);
  if (result != AUTOMATION_SUCCESS) {
    DLOG(WARNING) << "InitializeComplete: failure " << result;
    ReleaseAutomationServer();
  } else {
    init_state_ = INITIALIZED;

    // If the host already have a window, ask Chrome to re-parent.
    if (parent_window_)
      SetParentWindow(parent_window_);

    // If host specified destination URL - navigate. Apparently we do not use
    // accelerator table.
    if (navigate_after_initialization_) {
      navigate_after_initialization_ = false;
      BeginNavigate();
    }
  }

  if (chrome_frame_delegate_) {
    if (result == AUTOMATION_SUCCESS) {
      chrome_frame_delegate_->OnAutomationServerReady();
    } else {
      std::string version;
      if (automation_server_)
        version = automation_server_->server_version();
      chrome_frame_delegate_->OnAutomationServerLaunchFailed(result, version);
    }
  }
}

bool ChromeFrameAutomationClient::ProcessUrlRequestMessage(TabProxy* tab,
    const IPC::Message& msg, bool ui_thread) {
  // Either directly call appropriate url_fetcher function
  // or postpone call to the UI thread.
  uint16 msg_type = msg.type();
  switch (msg_type) {
    default:
      return false;

    case AutomationMsg_RequestStart::ID:
      if (ui_thread || (url_fetcher_flags_ &
                           PluginUrlRequestManager::START_REQUEST_THREADSAFE)) {
        AutomationMsg_RequestStart::Dispatch(&msg, url_fetcher_, this,
            &PluginUrlRequestManager::StartUrlRequest);
        return true;
      }
      break;

    case AutomationMsg_RequestRead::ID:
      if (ui_thread || (url_fetcher_flags_ &
                            PluginUrlRequestManager::READ_REQUEST_THREADSAFE)) {
        AutomationMsg_RequestRead::Dispatch(&msg, url_fetcher_, this,
            &PluginUrlRequestManager::ReadUrlRequest);
        return true;
      }
      break;

    case AutomationMsg_RequestEnd::ID:
      if (ui_thread || (url_fetcher_flags_ &
                            PluginUrlRequestManager::STOP_REQUEST_THREADSAFE)) {
        AutomationMsg_RequestEnd::Dispatch(&msg, url_fetcher_, this,
            &PluginUrlRequestManager::EndUrlRequest);
        return true;
      }
      break;

    case AutomationMsg_DownloadRequestInHost::ID:
      if (ui_thread || (url_fetcher_flags_ &
                        PluginUrlRequestManager::DOWNLOAD_REQUEST_THREADSAFE)) {
        AutomationMsg_DownloadRequestInHost::Dispatch(&msg, url_fetcher_, this,
            &PluginUrlRequestManager::DownloadUrlRequestInHost);
        return true;
      }
      break;

    case AutomationMsg_GetCookiesFromHost::ID:
      if (ui_thread || (url_fetcher_flags_ &
                          PluginUrlRequestManager::COOKIE_REQUEST_THREADSAFE)) {
        AutomationMsg_GetCookiesFromHost::Dispatch(&msg, url_fetcher_, this,
            &PluginUrlRequestManager::GetCookiesFromHost);
        return true;
      }
      break;

    case AutomationMsg_SetCookieAsync::ID:
      if (ui_thread || (url_fetcher_flags_ &
                          PluginUrlRequestManager::COOKIE_REQUEST_THREADSAFE)) {
        AutomationMsg_SetCookieAsync::Dispatch(&msg, url_fetcher_, this,
            &PluginUrlRequestManager::SetCookiesInHost);
        return true;
      }
      break;
  }

  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::ProcessUrlRequestMessage, tab, msg, true));
  return true;
}

// These are invoked in channel's background thread.
// Cannot call any method of the activex/npapi here since they are STA
// kind of beings.
// By default we marshal the IPC message to the main/GUI thread and from there
// we safely invoke chrome_frame_delegate_->OnMessageReceived(msg).
bool ChromeFrameAutomationClient::OnMessageReceived(TabProxy* tab,
                                                    const IPC::Message& msg) {
  DCHECK(tab == tab_.get());
  // Quickly process network related messages.
  if (url_fetcher_ && ProcessUrlRequestMessage(tab, msg, false))
    return true;

  // Early check to avoid needless marshaling
  if (chrome_frame_delegate_ == NULL)
    return false;

  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::OnMessageReceivedUIThread, msg));
  return true;
}

void ChromeFrameAutomationClient::OnChannelError(TabProxy* tab) {
  DCHECK(tab == tab_.get());
  // Early check to avoid needless marshaling
  if (chrome_frame_delegate_ == NULL)
    return;

  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::OnChannelErrorUIThread));
}

void ChromeFrameAutomationClient::OnMessageReceivedUIThread(
    const IPC::Message& msg) {
  DCHECK_EQ(base::PlatformThread::CurrentId(), ui_thread_id_);
  // Forward to the delegate.
  if (chrome_frame_delegate_)
    chrome_frame_delegate_->OnMessageReceived(msg);
}

void ChromeFrameAutomationClient::OnChannelErrorUIThread() {
  DCHECK_EQ(base::PlatformThread::CurrentId(), ui_thread_id_);

  // Report a metric that something went wrong unexpectedly.
  CrashMetricsReporter::GetInstance()->IncrementMetric(
      CrashMetricsReporter::CHANNEL_ERROR_COUNT);

  // Forward to the delegate.
  if (chrome_frame_delegate_)
    chrome_frame_delegate_->OnChannelError();
}

void ChromeFrameAutomationClient::ReportNavigationError(
    AutomationMsg_NavigationResponseValues error_code,
    const std::string& url) {
  if (!chrome_frame_delegate_)
    return;

  if (ui_thread_id_ == base::PlatformThread::CurrentId()) {
    chrome_frame_delegate_->OnLoadFailed(error_code, url);
  } else {
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &ChromeFrameAutomationClient::ReportNavigationError,
        error_code, url));
  }
}

void ChromeFrameAutomationClient::Resize(int width, int height,
                                         int flags) {
  if (tab_.get() && ::IsWindow(chrome_window())) {
    SetWindowPos(HWND_TOP, 0, 0, width, height, flags);
    tab_->Reposition(chrome_window(), HWND_TOP, 0, 0, width, height,
                     flags, m_hWnd);
  }
}

void ChromeFrameAutomationClient::SetParentWindow(HWND parent_window) {
  parent_window_ = parent_window;
  // If we're done with the initialization step, go ahead
  if (is_initialized()) {
    if (parent_window == NULL) {
      // Hide and reparent the automation window. This window will get
      // reparented to the new ActiveX/Active document window when it gets
      // created.
      ShowWindow(SW_HIDE);
      SetParent(GetDesktopWindow());
    } else {
      if (!::IsWindow(chrome_window())) {
        DLOG(WARNING) << "Invalid Chrome Window handle in SetParentWindow";
        return;
      }

      if (!SetParent(parent_window)) {
        DLOG(WARNING) << "Failed to set parent window for automation window. "
                      << "Error = "
                      << GetLastError();
        return;
      }

      RECT parent_client_rect = {0};
      ::GetClientRect(parent_window, &parent_client_rect);
      int width = parent_client_rect.right - parent_client_rect.left;
      int height = parent_client_rect.bottom - parent_client_rect.top;

      Resize(width, height, SWP_SHOWWINDOW | SWP_NOZORDER);
    }
  }
}

void ChromeFrameAutomationClient::ReleaseAutomationServer() {
  if (automation_server_id_) {
    // Cache the server id and clear the automation_server_id_ before
    // calling ReleaseAutomationServer.  The reason we do this is that
    // we must cancel pending messages before we release the automation server.
    // Furthermore, while ReleaseAutomationServer is running, we could get
    // a callback to LaunchComplete which could cause an external tab to be
    // created. Ideally the callbacks should be dropped.
    // TODO(ananta)
    // Refactor the ChromeFrameAutomationProxy code to not depend on
    // AutomationProxy and simplify the whole mess.
    void* server_id = automation_server_id_;
    automation_server_id_ = NULL;

    if (automation_server_) {
      // Make sure to clean up any pending sync messages before we go away.
      automation_server_->CancelAsync(this);
    }

    proxy_factory_->ReleaseAutomationServer(server_id, this);
    automation_server_ = NULL;

    // automation_server_ must not have been set to non NULL.
    // (if this regresses, start by looking at LaunchComplete()).
    DCHECK(automation_server_ == NULL);
  } else {
    DCHECK(automation_server_ == NULL);
  }
}

void ChromeFrameAutomationClient::SendContextMenuCommandToChromeFrame(
  int selected_command) {
  if (tab_)
    tab_->SendContextMenuCommand(selected_command);
}

std::wstring ChromeFrameAutomationClient::GetVersion() const {
  return GetCurrentModuleVersion();
}

void ChromeFrameAutomationClient::Print(HDC print_dc,
                                        const RECT& print_bounds) {
  if (!tab_window_) {
    NOTREACHED();
    return;
  }

  HDC window_dc = ::GetDC(tab_window_);

  BitBlt(print_dc, print_bounds.left, print_bounds.top,
         print_bounds.right - print_bounds.left,
         print_bounds.bottom - print_bounds.top,
         window_dc, print_bounds.left, print_bounds.top,
         SRCCOPY);

  ::ReleaseDC(tab_window_, window_dc);
}

void ChromeFrameAutomationClient::PrintTab() {
  if (tab_)
    tab_->PrintAsync();
}

bool ChromeFrameAutomationClient::Reinitialize(
    ChromeFrameDelegate* delegate,
    PluginUrlRequestManager* url_fetcher) {
  if (url_fetcher_) {
    // Clean up any outstanding requests
    url_fetcher_->StopAllRequests();
    url_fetcher_ = NULL;
  }

  if (!tab_.get() || !::IsWindow(chrome_window_)) {
    NOTREACHED();
    DLOG(WARNING) << "ChromeFrameAutomationClient instance reused "
                  << "with invalid tab";
    return false;
  }

  if (!delegate) {
    NOTREACHED();
    return false;
  }

  chrome_frame_delegate_ = delegate;
  DeleteAllPendingTasks();
  SetUrlFetcher(url_fetcher);
  SetParentWindow(NULL);
  return true;
}

void ChromeFrameAutomationClient::AttachExternalTab(
    uint64 external_tab_cookie) {
  DCHECK_EQ(static_cast<TabProxy*>(NULL), tab_.get());
  DCHECK_EQ(-1, tab_handle_);

  external_tab_cookie_ = external_tab_cookie;
}

void ChromeFrameAutomationClient::BlockExternalTab(uint64 cookie) {
  // The host does not want this tab to be shown (due popup blocker).
  IPC::SyncMessage* message =
      new AutomationMsg_ConnectExternalTab(cookie, false, m_hWnd,
                                           NULL, NULL, NULL, 0);
  automation_server_->SendAsAsync(message, NULL, this);
}

void ChromeFrameAutomationClient::SetPageFontSize(
    enum AutomationPageFontSize font_size) {
  if (font_size < SMALLEST_FONT ||
      font_size > LARGEST_FONT) {
      NOTREACHED() << "Invalid font size specified : "
                   << font_size;
      return;
  }

  automation_server_->Send(
      new AutomationMsg_SetPageFontSize(tab_handle_, font_size));
}

void ChromeFrameAutomationClient::RemoveBrowsingData(int remove_mask) {
  automation_server_->Send(new AutomationMsg_RemoveBrowsingData(remove_mask));
}

void ChromeFrameAutomationClient::SetUrlFetcher(
    PluginUrlRequestManager* url_fetcher) {
  DCHECK(url_fetcher != NULL);
  url_fetcher_ = url_fetcher;
  url_fetcher_flags_ = url_fetcher->GetThreadSafeFlags();
  url_fetcher_->set_delegate(this);
}

void ChromeFrameAutomationClient::SetZoomLevel(PageZoom::Function zoom_level) {
  if (automation_server_) {
    automation_server_->Send(new AutomationMsg_SetZoomLevel(tab_handle_,
                                                            zoom_level));
  }
}

void ChromeFrameAutomationClient::OnUnload(bool* should_unload) {
  *should_unload = true;
  if (automation_server_) {
    const DWORD kUnloadEventTimeout = 20000;

    IPC::SyncMessage* msg = new AutomationMsg_RunUnloadHandlers(tab_handle_,
                                                                should_unload);
    base::WaitableEvent unload_call_finished(false, false);
    UnloadContext* unload_context = new UnloadContext(&unload_call_finished,
                                                      should_unload);
    automation_server_->SendAsAsync(msg, unload_context, this);
    HANDLE done = unload_call_finished.handle();
    WaitWithMessageLoop(&done, 1, kUnloadEventTimeout);
  }
}

//////////////////////////////////////////////////////////////////////////
// PluginUrlRequestDelegate implementation.
// Forward network related responses to Chrome.

void ChromeFrameAutomationClient::OnResponseStarted(int request_id,
    const char* mime_type,  const char* headers, int size,
    base::Time last_modified, const std::string& redirect_url,
    int redirect_status, const net::HostPortPair& socket_address) {
  const AutomationURLResponse response(
      mime_type,
      headers ? headers : "",
      size,
      last_modified,
      redirect_url,
      redirect_status,
      socket_address);

  automation_server_->Send(new AutomationMsg_RequestStarted(
      tab_->handle(), request_id, response));
}

void ChromeFrameAutomationClient::OnReadComplete(int request_id,
                                                 const std::string& data) {
  automation_server_->Send(new AutomationMsg_RequestData(
      tab_->handle(), request_id, data));
}

void ChromeFrameAutomationClient::OnResponseEnd(
    int request_id,
    const net::URLRequestStatus& status) {
  automation_server_->Send(new AutomationMsg_RequestEnd(
      tab_->handle(), request_id, status));
}

void ChromeFrameAutomationClient::OnCookiesRetrieved(bool success,
    const GURL& url, const std::string& cookie_string, int cookie_id) {
  automation_server_->Send(new AutomationMsg_GetCookiesHostResponse(
      tab_->handle(), success, url, cookie_string, cookie_id));
}
