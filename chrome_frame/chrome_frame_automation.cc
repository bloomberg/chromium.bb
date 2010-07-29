// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_automation.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/trace_event.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/waitable_event.h"
#include "chrome/app/client_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/chrome_launcher_utils.h"
#include "chrome_frame/crash_reporting/crash_metrics.h"
#include "chrome_frame/custom_sync_call_context.h"
#include "chrome_frame/utils.h"

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
Lock g_ChromeFrameHistogramLock;

class ChromeFrameAutomationProxyImpl::TabProxyNotificationMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit TabProxyNotificationMessageFilter(AutomationHandleTracker* tracker)
      : tracker_(tracker) {
  }

  void AddTabProxy(AutomationHandle tab_proxy) {
    tabs_list_.push_back(tab_proxy);
  }

  void RemoveTabProxy(AutomationHandle tab_proxy) {
    tabs_list_.remove(tab_proxy);
  }

  virtual bool OnMessageReceived(const IPC::Message& message) {
    if (message.is_reply())
      return false;

    int tab_handle = 0;
    if (!ChromeFrameDelegateImpl::IsTabMessage(message, &tab_handle))
      return false;

    // Get AddRef-ed pointer to corresponding TabProxy object
    TabProxy* tab = static_cast<TabProxy*>(tracker_->GetResource(tab_handle));
    if (tab) {
      tab->OnMessageReceived(message);
      tab->Release();
    } else {
      DLOG(ERROR) << "Failed to find TabProxy for tab:" << tab_handle;
    }
    return true;
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
      default:
        NOTREACHED();
    }
    return true;
  }
};

ChromeFrameAutomationProxyImpl::ChromeFrameAutomationProxyImpl(
    int launch_timeout)
    : AutomationProxy(launch_timeout) {
  TRACE_EVENT_BEGIN("chromeframe.automationproxy", this, "");

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

ProxyFactory::ProxyCacheEntry::ProxyCacheEntry(const std::wstring& profile)
    : proxy(NULL), profile_name(profile), ref_count(1),
    launch_result(AutomationLaunchResult(-1)) {
  thread.reset(new base::Thread(WideToASCII(profile_name).c_str()));
  thread->Start();
}

DISABLE_RUNNABLE_METHOD_REFCOUNT(ProxyFactory);

ProxyFactory::ProxyFactory()
    : uma_send_interval_(0) {
  uma_send_interval_ = GetConfigInt(kDefaultSendUMADataInterval,
                                    kUmaSendIntervalValue);
}

ProxyFactory::~ProxyFactory() {
  for (size_t i = 0; i < proxies_.container().size(); ++i) {
    DWORD result = WaitForSingleObject(proxies_[i]->thread->thread_handle(), 0);
    if (WAIT_OBJECT_0 != result)
      // TODO(stoyan): Don't leak proxies on exit.
      DLOG(ERROR) << "Proxies leaked on exit.";
  }
}

void ProxyFactory::GetAutomationServer(
    LaunchDelegate* delegate, const ChromeFrameLaunchParams& params,
    void** automation_server_id) {
  TRACE_EVENT_BEGIN("chromeframe.createproxy", this, "");

  ProxyCacheEntry* entry = NULL;
  // Find already existing launcher thread for given profile
  AutoLock lock(lock_);
  for (size_t i = 0; i < proxies_.container().size(); ++i) {
    if (!lstrcmpiW(proxies_[i]->profile_name.c_str(),
                   params.profile_name.c_str())) {
      entry = proxies_[i];
      DCHECK(entry->thread.get() != NULL);
      break;
    }
  }

  if (entry == NULL) {
    entry = new ProxyCacheEntry(params.profile_name);
    proxies_.container().push_back(entry);
  } else {
    entry->ref_count++;
  }

  DCHECK(delegate != NULL);
  DCHECK(automation_server_id != NULL);

  *automation_server_id = entry;
  // Note we always queue request to the launch thread, even if we already
  // have established proxy object. A simple lock around entry->proxy = proxy
  // would allow calling LaunchDelegate directly from here if
  // entry->proxy != NULL. Drawback is that callback may be invoked either in
  // main thread or in background thread, which may confuse the client.
  entry->thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &ProxyFactory::CreateProxy, entry, params, delegate));

  // IE uses the chrome frame provided UMA data uploading scheme. NPAPI
  // continues to use Chrome to upload UMA data.
  if (!CrashMetricsReporter::GetInstance()->active()) {
    entry->thread->message_loop()->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this, &ProxyFactory::SendUMAData, entry),
        uma_send_interval_);
  }
}

void ProxyFactory::CreateProxy(ProxyFactory::ProxyCacheEntry* entry,
                               const ChromeFrameLaunchParams& params,
                               LaunchDelegate* delegate) {
  DCHECK(entry->thread->thread_id() == PlatformThread::CurrentId());
  if (entry->proxy) {
    delegate->LaunchComplete(entry->proxy, entry->launch_result);
    return;
  }

  // We *must* create automationproxy in a thread that has message loop,
  // since SyncChannel::Context construction registers event to be watched
  // through ObjectWatcher which subscribes for the current thread message loop
  // destruction notification.

  // At same time we must destroy/stop the thread from another thread.
  ChromeFrameAutomationProxyImpl* proxy =
      new ChromeFrameAutomationProxyImpl(
          params.automation_server_launch_timeout);

  // Launch browser
  scoped_ptr<CommandLine> command_line(
      chrome_launcher::CreateLaunchCommandLine());
  command_line->AppendSwitchWithValue(switches::kAutomationClientChannelID,
      proxy->channel_id());

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

  DLOG(INFO) << "Profile path: " << params.profile_path.value();
  command_line->AppendSwitchPath(switches::kUserDataDir, params.profile_path);

  std::wstring command_line_string(command_line->command_line_string());
  // If there are any extra arguments, append them to the command line.
  if (!params.extra_chrome_arguments.empty()) {
    command_line_string += L' ' + params.extra_chrome_arguments;
  }

  automation_server_launch_start_time_ = base::TimeTicks::Now();

  if (!base::LaunchApp(command_line_string, false, false, NULL)) {
    // We have no code for launch failure.
    entry->launch_result = AutomationLaunchResult(-1);
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
    entry->launch_result = proxy->WaitForAppLaunch();
    launch_stats.Dump();

    base::TimeDelta delta =
        base::TimeTicks::Now() - automation_server_launch_start_time_;

    if (entry->launch_result == AUTOMATION_SUCCESS) {
      THREAD_SAFE_UMA_HISTOGRAM_TIMES(
          "ChromeFrame.AutomationServerLaunchSuccessTime", delta);
    } else {
      THREAD_SAFE_UMA_HISTOGRAM_TIMES(
          "ChromeFrame.AutomationServerLaunchFailedTime", delta);
    }

    THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeFrame.LaunchResult",
                                            entry->launch_result,
                                            AUTOMATION_SUCCESS,
                                            AUTOMATION_CREATE_TAB_FAILED,
                                            AUTOMATION_CREATE_TAB_FAILED + 1);
  }

  TRACE_EVENT_END("chromeframe.createproxy", this, "");

  // Finally set the proxy.
  entry->proxy = proxy;
  delegate->LaunchComplete(proxy, entry->launch_result);
}

bool ProxyFactory::ReleaseAutomationServer(void* server_id) {
  if (!server_id) {
    NOTREACHED();
    return false;
  }

  ProxyCacheEntry* entry = reinterpret_cast<ProxyCacheEntry*>(server_id);

#ifndef NDEBUG
  lock_.Acquire();
  Vector::ContainerType::iterator it = std::find(proxies_.container().begin(),
                                                 proxies_.container().end(),
                                                 entry);
  DCHECK(it != proxies_.container().end());
  DCHECK(entry->thread->thread_id() != PlatformThread::CurrentId());
  DCHECK_GT(entry->ref_count, 0);

  lock_.Release();
#endif

  base::WaitableEvent done(true, false);
  entry->thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &ProxyFactory::ReleaseProxy, entry, &done));
  done.Wait();

  // Stop the thread and destroy the entry if there is no more clients.
  if (entry->ref_count == 0) {
    DCHECK(entry->proxy == NULL);
    entry->thread.reset();
    delete entry;
  }

  return true;
}

void ProxyFactory::ReleaseProxy(ProxyCacheEntry* entry,
                                base::WaitableEvent* done) {
  DCHECK(entry->thread->thread_id() == PlatformThread::CurrentId());

  lock_.Acquire();
  if (!--entry->ref_count) {
    Vector::ContainerType::iterator it = std::find(proxies_.container().begin(),
                                                   proxies_.container().end(),
                                                   entry);
    proxies_->erase(it);
  }
  lock_.Release();

  // Send pending UMA data if any.
  if (!entry->ref_count) {
    SendUMAData(entry);
    delete entry->proxy;
    entry->proxy = NULL;
  }

  done->Signal();
}

Singleton<ProxyFactory> g_proxy_factory;

void ProxyFactory::SendUMAData(ProxyCacheEntry* proxy_entry) {
  // IE uses the chrome frame provided UMA data uploading scheme. NPAPI
  // continues to use Chrome to upload UMA data.
  if (CrashMetricsReporter::GetInstance()->active()) {
    return;
  }

  if (!proxy_entry) {
    NOTREACHED() << __FUNCTION__ << " Invalid proxy entry";
    return;
  }

  DCHECK(proxy_entry->thread->thread_id() == PlatformThread::CurrentId());

  if (proxy_entry->proxy) {
    ChromeFrameHistogramSnapshots::HistogramPickledList histograms =
        chrome_frame_histograms_.GatherAllHistograms();

    if (!histograms.empty()) {
      proxy_entry->proxy->Send(
          new AutomationMsg_RecordHistograms(0, histograms));
    }
  } else {
    DLOG(INFO) << __FUNCTION__ << " No proxy available to service the request";
    return;
  }

  MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
      this, &ProxyFactory::SendUMAData, proxy_entry), uma_send_interval_);
}

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
      proxy_factory_(g_proxy_factory.get()),
      handle_top_level_requests_(false),
      tab_handle_(-1),
      external_tab_cookie_(0),
      url_fetcher_(NULL),
      url_fetcher_flags_(PluginUrlRequestManager::NOT_THREADSAFE),
      navigate_after_initialization_(false) {
}

ChromeFrameAutomationClient::~ChromeFrameAutomationClient() {
  // Uninitialize must be called prior to the destructor
  DCHECK(automation_server_ == NULL);
}

bool ChromeFrameAutomationClient::Initialize(
    ChromeFrameDelegate* chrome_frame_delegate,
    const ChromeFrameLaunchParams& chrome_launch_params) {
  DCHECK(!IsWindow());
  chrome_frame_delegate_ = chrome_frame_delegate;
  chrome_launch_params_ = chrome_launch_params;
  ui_thread_id_ = PlatformThread::CurrentId();
#ifndef NDEBUG
  // In debug mode give more time to work with a debugger.
  if (IsDebuggerPresent()) {
    // Don't use INFINITE (which is -1) or even MAXINT since we will convert
    // from milliseconds to microseconds when stored in a base::TimeDelta,
    // thus * 1000. An hour should be enough.
    chrome_launch_params_.automation_server_launch_timeout = 60 * 60 * 1000;
  } else {
    DCHECK_LT(chrome_launch_params_.automation_server_launch_timeout,
              MAXINT / 2000);
    chrome_launch_params_.automation_server_launch_timeout *= 2;
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
  // Corresponsing Release is in OnFinalMessage();
  AddRef();

  // Mark our state as initializing.  We'll reach initialized once
  // InitializeComplete is called successfully.
  init_state_ = INITIALIZING;

  if (chrome_launch_params_.url.is_valid())
    navigate_after_initialization_ = false;

  if (!navigate_after_initialization_) {
    chrome_launch_params_.url = url_;
  }

  proxy_factory_->GetAutomationServer(
      static_cast<ProxyFactory::LaunchDelegate*>(this),
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

  if (tab_.get()) {
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

  chrome_frame_delegate_ = NULL;
  init_state_ = UNINITIALIZED;
}

bool ChromeFrameAutomationClient::InitiateNavigation(
    const std::string& url, const std::string& referrer, bool is_privileged) {
  if (url.empty())
    return false;

  GURL parsed_url(url);
  // Catch invalid URLs early.
  if (!parsed_url.is_valid() ||
      !IsValidUrlScheme(UTF8ToWide(url), is_privileged)) {
    DLOG(ERROR) << "Invalid URL passed to InitiateNavigation: " << url
                << " is_privileged=" << is_privileged;
    return false;
  }

  // Important: Since we will be using the referrer_ variable from a different
  // thread, we need to force a new std::string buffer instance for the
  // referrer_ GURL variable.  Otherwise we can run into strangeness when the
  // GURL is accessed and it could result in a bad URL that can cause the
  // referrer to be dropped or something worse.
  referrer_ = GURL(referrer.c_str());
  url_ = parsed_url;
  navigate_after_initialization_ = false;

  if (is_initialized()) {
    BeginNavigate(url_, referrer_);
  } else {
    navigate_after_initialization_ = true;
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
      0, tab_->handle(), index, NULL);
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

void ChromeFrameAutomationClient::BeginNavigate(const GURL& url,
                                                const GURL& referrer) {
  // Could be NULL if we failed to launch Chrome in LaunchAutomationServer()
  if (!automation_server_ || !tab_.get()) {
    DLOG(WARNING) << "BeginNavigate - can't navigate.";
    ReportNavigationError(AUTOMATION_MSG_NAVIGATION_ERROR, url_.spec());
    return;
  }

  DCHECK(::IsWindow(chrome_window_));

  if (!tab_->is_valid()) {
    DLOG(WARNING) << "BeginNavigate - tab isn't valid.";
    return;
  }

  IPC::SyncMessage* msg =
      new AutomationMsg_NavigateInExternalTab(0, tab_->handle(), url,
                                              referrer, NULL);
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
     ReportNavigationError(AUTOMATION_MSG_NAVIGATION_ERROR, url_.spec());
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
      new AutomationMsg_Find(0, tab_->handle(), params, NULL, NULL);
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

  IPC::SyncMessage* msg =
      new AutomationMsg_InstallExtension(0, crx_path, NULL);

  // The context will delete itself after it is called.
  automation_server_->SendAsAsync(msg, ctx, this);
}

void ChromeFrameAutomationClient::InstallExtensionComplete(
    const FilePath& crx_path,
    void* user_data,
    AutomationMsg_ExtensionResponseValues res) {
  DCHECK_EQ(PlatformThread::CurrentId(), ui_thread_id_);

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
        0, ctx->extension_directories());

    // The context will delete itself after it is called.
    automation_server_->SendAsAsync(msg, ctx, this);
}

void ChromeFrameAutomationClient::GetEnabledExtensionsComplete(
    void* user_data,
    std::vector<FilePath>* extension_directories) {
  DCHECK_EQ(PlatformThread::CurrentId(), ui_thread_id_);

  if (chrome_frame_delegate_) {
    chrome_frame_delegate_->OnGetEnabledExtensionsComplete(
        user_data, *extension_directories);
  }

  delete extension_directories;
}

void ChromeFrameAutomationClient::OnChromeFrameHostMoved() {
  // Use a local var to avoid the small possibility of getting the tab_
  // member be cleared while we try to use it.
  // Note that TabProxy is a RefCountedThreadSafe object, so we should be OK.
  scoped_refptr<TabProxy> tab(tab_);
  // There also is a possibility that tab_ has not been set yet,
  // so we still need to test for NULL.
  if (tab.get() != NULL)
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

  IPC::SyncMessage* msg =
      new AutomationMsg_LoadExpandedExtension(0, path, NULL);

  // The context will delete itself after it is called.
  automation_server_->SendAsAsync(msg, ctx, this);
}

void ChromeFrameAutomationClient::CreateExternalTab() {
  AutomationLaunchResult launch_result = AUTOMATION_SUCCESS;
  DCHECK(IsWindow());
  DCHECK(automation_server_ != NULL);

  // TODO(ananta)
  // We should pass in the referrer for the initial navigation.
  const IPC::ExternalTabSettings settings = {
    m_hWnd,
    gfx::Rect(),
    WS_CHILD,
    chrome_launch_params_.incognito_mode,
    !use_chrome_network_,
    handle_top_level_requests_,
    chrome_launch_params_.url,
    chrome_launch_params_.referrer,
    !chrome_launch_params_.is_widget_mode  // Infobars disabled in widget mode.
  };

  THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS(
      "ChromeFrame.HostNetworking", !use_chrome_network_, 0, 1, 2);

  THREAD_SAFE_UMA_HISTOGRAM_CUSTOM_COUNTS(
      "ChromeFrame.HandleTopLevelRequests", handle_top_level_requests_, 0, 1,
      2);

  IPC::SyncMessage* message =
      new AutomationMsg_CreateExternalTab(0, settings, NULL, NULL, NULL);
  automation_server_->SendAsAsync(message, new CreateExternalTabContext(this),
                                  this);
}

AutomationLaunchResult ChromeFrameAutomationClient::CreateExternalTabComplete(
    HWND chrome_window, HWND tab_window, int tab_handle) {
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
  if (functions_enabled.size() > 0) {
    tab_->SetEnableExtensionAutomation(functions_enabled);
  }
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
    DLOG(INFO) << "Not storing automation server pointer due to shutting down";
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
            new AutomationMsg_ConnectExternalTab(0, external_tab_cookie_, true,
              m_hWnd, NULL, NULL, NULL);
        automation_server_->SendAsAsync(message,
                                        new CreateExternalTabContext(this),
                                        this);
        DLOG(INFO) << __FUNCTION__ << ": sending CreateExternalTabComplete";
      }
    }
  } else {
    // Launch failed. Note, we cannot delete proxy here.
    PostTask(FROM_HERE, NewRunnableMethod(this,
        &ChromeFrameAutomationClient::InitializeComplete, result));
  }
}

void ChromeFrameAutomationClient::InitializeComplete(
    AutomationLaunchResult result) {
  DCHECK_EQ(PlatformThread::CurrentId(), ui_thread_id_);
  std::string version = automation_server_->server_version();

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
      BeginNavigate(url_, referrer_);
    }
  }

  if (chrome_frame_delegate_) {
    if (result == AUTOMATION_SUCCESS) {
      chrome_frame_delegate_->OnAutomationServerReady();
    } else {
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
        AutomationMsg_RequestStart::Dispatch(&msg, url_fetcher_,
            &PluginUrlRequestManager::StartUrlRequest);
        return true;
      }
      break;

    case AutomationMsg_RequestRead::ID:
      if (ui_thread || (url_fetcher_flags_ &
                            PluginUrlRequestManager::READ_REQUEST_THREADSAFE)) {
        AutomationMsg_RequestRead::Dispatch(&msg, url_fetcher_,
            &PluginUrlRequestManager::ReadUrlRequest);
        return true;
      }
      break;

    case AutomationMsg_RequestEnd::ID:
      if (ui_thread || (url_fetcher_flags_ &
                            PluginUrlRequestManager::STOP_REQUEST_THREADSAFE)) {
        AutomationMsg_RequestEnd::Dispatch(&msg, url_fetcher_,
            &PluginUrlRequestManager::EndUrlRequest);
        return true;
      }
      break;

    case AutomationMsg_DownloadRequestInHost::ID:
      if (ui_thread || (url_fetcher_flags_ &
                        PluginUrlRequestManager::DOWNLOAD_REQUEST_THREADSAFE)) {
        AutomationMsg_DownloadRequestInHost::Dispatch(&msg, url_fetcher_,
            &PluginUrlRequestManager::DownloadUrlRequestInHost);
        return true;
      }
      break;

    case AutomationMsg_GetCookiesFromHost::ID:
      if (ui_thread || (url_fetcher_flags_ &
                          PluginUrlRequestManager::COOKIE_REQUEST_THREADSAFE)) {
        AutomationMsg_GetCookiesFromHost::Dispatch(&msg, url_fetcher_,
            &PluginUrlRequestManager::GetCookiesFromHost);
        return true;
      }
      break;

    case AutomationMsg_SetCookieAsync::ID:
      if (ui_thread || (url_fetcher_flags_ &
                          PluginUrlRequestManager::COOKIE_REQUEST_THREADSAFE)) {
        AutomationMsg_SetCookieAsync::Dispatch(&msg, url_fetcher_,
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
void ChromeFrameAutomationClient::OnMessageReceived(TabProxy* tab,
                                                    const IPC::Message& msg) {
  DCHECK(tab == tab_.get());
  // Quickly process network related messages.
  if (url_fetcher_ && ProcessUrlRequestMessage(tab, msg, false))
    return;

  // Early check to avoid needless marshaling
  if (chrome_frame_delegate_ == NULL)
    return;

  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::OnMessageReceivedUIThread, msg));
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
  DCHECK_EQ(PlatformThread::CurrentId(), ui_thread_id_);
  // Forward to the delegate.
  if (chrome_frame_delegate_)
    chrome_frame_delegate_->OnMessageReceived(msg);
}

void ChromeFrameAutomationClient::OnChannelErrorUIThread() {
  DCHECK_EQ(PlatformThread::CurrentId(), ui_thread_id_);
  // Forward to the delegate.
  if (chrome_frame_delegate_)
    chrome_frame_delegate_->OnChannelError();
}

void ChromeFrameAutomationClient::ReportNavigationError(
    AutomationMsg_NavigationResponseValues error_code,
    const std::string& url) {
  if (!chrome_frame_delegate_)
    return;

  if (ui_thread_id_ == PlatformThread::CurrentId()) {
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

    proxy_factory_->ReleaseAutomationServer(server_id);
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
  DCHECK(tab_ != NULL);
  tab_->SendContextMenuCommand(selected_command);
}

std::wstring ChromeFrameAutomationClient::GetVersion() const {
  static FileVersionInfo* version_info =
      FileVersionInfo::CreateFileVersionInfoForCurrentModule();

  std::wstring version;
  if (version_info)
    version = version_info->product_version();

  return version;
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
      new AutomationMsg_ConnectExternalTab(0, cookie, false, m_hWnd,
                                           NULL, NULL, NULL);
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
      new AutomationMsg_SetPageFontSize(0, tab_handle_, font_size));
}

void ChromeFrameAutomationClient::RemoveBrowsingData(int remove_mask) {
  automation_server_->Send(
      new AutomationMsg_RemoveBrowsingData(0, remove_mask));
}

void ChromeFrameAutomationClient::RunUnloadHandlers(HWND notification_window,
                                                    int notification_message) {
  if (automation_server_) {
    automation_server_->Send(
        new AutomationMsg_RunUnloadHandlers(0, tab_handle_,
                                            notification_window,
                                            notification_message));
  } else {
    // Post this message to ensure that the caller exits his message loop.
    ::PostMessage(notification_window, notification_message, 0, 0);
  }
}

//////////////////////////////////////////////////////////////////////////
// PluginUrlRequestDelegate implementation.
// Forward network related responses to Chrome.

void ChromeFrameAutomationClient::OnResponseStarted(int request_id,
    const char* mime_type,  const char* headers, int size,
    base::Time last_modified, const std::string& redirect_url,
    int redirect_status) {
  const IPC::AutomationURLResponse response = {
      mime_type,
      headers ? headers : "",
      size,
      last_modified,
      redirect_url,
      redirect_status
  };

  automation_server_->Send(new AutomationMsg_RequestStarted(0,
      tab_->handle(), request_id, response));
}

void ChromeFrameAutomationClient::OnReadComplete(int request_id,
                                                 const std::string& data) {
  automation_server_->Send(new AutomationMsg_RequestData(0, tab_->handle(),
      request_id, data));
}

void ChromeFrameAutomationClient::OnResponseEnd(int request_id,
    const URLRequestStatus& status) {
  automation_server_->Send(new AutomationMsg_RequestEnd(0, tab_->handle(),
      request_id, status));
}

void ChromeFrameAutomationClient::OnCookiesRetrieved(bool success,
    const GURL& url, const std::string& cookie_string, int cookie_id) {
  automation_server_->Send(new AutomationMsg_GetCookiesHostResponse(0,
      tab_->handle(), success, url, cookie_string, cookie_id));
}
