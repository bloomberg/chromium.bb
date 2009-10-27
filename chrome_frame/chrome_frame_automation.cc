// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_automation.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "chrome/app/client_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/chrome_launcher.h"
#include "chrome_frame/utils.h"
#include "chrome_frame/sync_msg_reply_dispatcher.h"

#ifdef NDEBUG
int64 kAutomationServerReasonableLaunchDelay = 1000;  // in milliseconds
#else
int64 kAutomationServerReasonableLaunchDelay = 1000 * 10;
#endif

int kDefaultSendUMADataInterval = 20000;  // in milliseconds.

static const wchar_t kUmaSendIntervalValue[] = L"UmaSendInterval";

class TabProxyNotificationMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit TabProxyNotificationMessageFilter(AutomationHandleTracker* tracker)
      : tracker_(tracker) {
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
    }
    return true;
  }


 private:
  AutomationHandleTracker* tracker_;
};

class ChromeFrameAutomationProxyImpl::CFMsgDispatcher
    : public SyncMessageReplyDispatcher {
 public:
  CFMsgDispatcher() : SyncMessageReplyDispatcher() {}
 protected:
  virtual bool HandleMessageType(const IPC::Message& msg,
                                 const MessageSent& origin) {
    switch (origin.type) {
      case AutomationMsg_CreateExternalTab::ID:
      case AutomationMsg_ConnectExternalTab::ID:
        InvokeCallback<Tuple3<HWND, HWND, int> >(msg, origin);
        break;
      case AutomationMsg_NavigateInExternalTab::ID:
        InvokeCallback<Tuple1<AutomationMsg_NavigationResponseValues> >(msg,
              origin);
        break;
      case AutomationMsg_InstallExtension::ID:
        InvokeCallback<Tuple1<AutomationMsg_ExtensionResponseValues> >(msg,
              origin);
        break;
      case AutomationMsg_LoadExpandedExtension::ID:
        InvokeCallback<Tuple1<AutomationMsg_ExtensionResponseValues> >(msg,
              origin);
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
  sync_ = new CFMsgDispatcher();
  // Order of filters is not important.
  channel_->AddFilter(new TabProxyNotificationMessageFilter(tracker_.get()));
  channel_->AddFilter(sync_.get());
}

ChromeFrameAutomationProxyImpl::~ChromeFrameAutomationProxyImpl() {
}

void ChromeFrameAutomationProxyImpl::SendAsAsync(IPC::SyncMessage* msg,
                                                 void* callback, void* key) {
  sync_->Push(msg, callback, key);
  channel_->ChannelProxy::Send(msg);
}

void ChromeFrameAutomationProxyImpl::CancelAsync(void* key) {
  sync_->Cancel(key);
}

scoped_refptr<TabProxy> ChromeFrameAutomationProxyImpl::CreateTabProxy(
    int handle) {
  DCHECK(tracker_->GetResource(handle) == NULL);
  return new TabProxy(this, tracker_.get(), handle);
}

struct LaunchTimeStats {
#ifndef NDEBUG
  LaunchTimeStats() {
    launch_time_begin_ = base::Time::Now();
  }

  void Dump() {
    base::TimeDelta launch_time = base::Time::Now() - launch_time_begin_;
    HISTOGRAM_TIMES("ChromeFrame.AutomationServerLaunchTime", launch_time);
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

template <> struct RunnableMethodTraits<ProxyFactory> {
  void RetainCallee(ProxyFactory* obj) {}
  void ReleaseCallee(ProxyFactory* obj) {}
};

ProxyFactory::ProxyFactory()
    : uma_send_interval_(0) {
  uma_send_interval_ = GetConfigInt(kDefaultSendUMADataInterval,
                                    kUmaSendIntervalValue);
}

ProxyFactory::~ProxyFactory() {
  DCHECK_EQ(proxies_.container().size(), 0);
}

void* ProxyFactory::GetAutomationServer(int launch_timeout,
    const std::wstring& profile_name,
    const std::wstring& extra_argument,
    bool perform_version_check,
    LaunchDelegate* delegate) {
  ProxyCacheEntry* entry = NULL;
  // Find already existing launcher thread for given profile
  AutoLock lock(lock_);
  for (size_t i = 0; i < proxies_.container().size(); ++i) {
    if (!lstrcmpiW(proxies_[i]->profile_name.c_str(), profile_name.c_str())) {
      entry = proxies_[i];
      DCHECK(entry->thread.get() != NULL);
      break;
    }
  }

  if (entry == NULL) {
    entry = new ProxyCacheEntry(profile_name);
    proxies_.container().push_back(entry);
  } else {
    entry->ref_count++;
  }


  // Note we always queue request to the launch thread, even if we already
  // have established proxy object. A simple lock around entry->proxy = proxy
  // would allow calling LaunchDelegate directly from here if
  // entry->proxy != NULL. Drawback is that callback may be invoked either in
  // main thread or in background thread, which may confuse the client.
  entry->thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &ProxyFactory::CreateProxy, entry,
      launch_timeout, extra_argument,
      perform_version_check, delegate));

  entry->thread->message_loop()->PostDelayedTask(FROM_HERE,
      NewRunnableMethod(this, &ProxyFactory::SendUMAData, entry),
      uma_send_interval_);

  return entry;
}

void ProxyFactory::CreateProxy(ProxyFactory::ProxyCacheEntry* entry,
                               int launch_timeout,
                               const std::wstring& extra_chrome_arguments,
                               bool perform_version_check,
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
      new ChromeFrameAutomationProxyImpl(launch_timeout);

  // Launch browser
  scoped_ptr<CommandLine> command_line(
      chrome_launcher::CreateLaunchCommandLine());
  command_line->AppendSwitchWithValue(switches::kAutomationClientChannelID,
      ASCIIToWide(proxy->channel_id()));

  // The metrics bug out because they attempt to use URLFetcher with a
  // null URLRequestContext::default_request_context_. Turn them off for now.
  // TODO(robertshield): Figure out why this is. It appears to have something
  // to do with an improperly set up profile...
  command_line->AppendSwitch(switches::kDisableMetrics);

  // Chrome Frame never wants Chrome to start up with a First Run UI.
  command_line->AppendSwitch(switches::kNoFirstRun);

  // Disable the "Whoa! Chrome has crashed." dialog, because that isn't very
  // useful for Chrome Frame users.
  command_line->AppendSwitch(switches::kNoErrorDialogs);

  command_line->AppendSwitch(switches::kEnableRendererAccessibility);

  // Place the profile directory in
  // "<chrome_exe_path>\..\User Data\<profile-name>"
  if (!entry->profile_name.empty()) {
    std::wstring profile_path;
    if (GetUserProfileBaseDirectory(&profile_path)) {
      file_util::AppendToPath(&profile_path, entry->profile_name);
      command_line->AppendSwitchWithValue(switches::kUserDataDir,
          profile_path);
    } else {
      // Can't get the profile dir :-( We need one to work, so fail.
      // We have no code for launch failure.
      entry->launch_result = AutomationLaunchResult(-1);
    }
  }

  std::wstring command_line_string(command_line->command_line_string());
  // If there are any extra arguments, append them to the command line.
  if (!extra_chrome_arguments.empty()) {
    command_line_string += L' ' + extra_chrome_arguments;
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
      UMA_HISTOGRAM_TIMES("ChromeFrame.AutomationServerLaunchSuccessTime",
                          delta);
    } else {
      UMA_HISTOGRAM_TIMES("ChromeFrame.AutomationServerLaunchFailedTime",
                          delta);
    }

    UMA_HISTOGRAM_CUSTOM_COUNTS("ChromeFrame.LaunchResult",
                                entry->launch_result,
                                AUTOMATION_SUCCESS,
                                AUTOMATION_CREATE_TAB_FAILED,
                                AUTOMATION_CREATE_TAB_FAILED + 1);
  }

  // Finally set the proxy.
  entry->proxy = proxy;
  delegate->LaunchComplete(proxy, entry->launch_result);
}

bool ProxyFactory::ReleaseAutomationServer(void* server_id) {
  DLOG(INFO) << __FUNCTION__;

  if (!server_id) {
    NOTREACHED();
    return false;
  }

  ProxyCacheEntry* entry = reinterpret_cast<ProxyCacheEntry*>(server_id);

  lock_.Acquire();
  Vector::ContainerType::iterator it = std::find(proxies_.container().begin(),
                                                 proxies_.container().end(),
                                                 entry);
  DCHECK(it != proxies_.container().end());
  DCHECK(entry->thread->thread_id() != PlatformThread::CurrentId());
  if (--entry->ref_count == 0) {
    proxies_.container().erase(it);
  }

  lock_.Release();

  // Destroy it.
  if (entry->ref_count == 0) {
    entry->thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(this,
        &ProxyFactory::DestroyProxy, entry));
    // Wait until thread exits
    entry->thread.reset();
    DCHECK(entry->proxy == NULL);
    delete entry;
  }

  return true;
}

void ProxyFactory::DestroyProxy(ProxyCacheEntry* entry) {
  DCHECK(entry->thread->thread_id() == PlatformThread::CurrentId());
  // Send pending UMA data if any.
  SendUMAData(entry);
  delete entry->proxy;
  entry->proxy = NULL;
}

Singleton<ProxyFactory> g_proxy_factory;

void ProxyFactory::SendUMAData(ProxyCacheEntry* proxy_entry) {
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
      incognito_(false),
      init_state_(UNINITIALIZED),
      use_chrome_network_(false),
      proxy_factory_(g_proxy_factory.get()),
      handle_top_level_requests_(false),
      tab_handle_(-1),
      external_tab_cookie_(NULL) {
}

ChromeFrameAutomationClient::~ChromeFrameAutomationClient() {
  // Uninitialize must be called prior to the destructor
  DCHECK(automation_server_ == NULL);
}

bool ChromeFrameAutomationClient::Initialize(
    ChromeFrameDelegate* chrome_frame_delegate,
    int automation_server_launch_timeout,
    bool perform_version_check,
    const std::wstring& profile_name,
    const std::wstring& extra_chrome_arguments,
    bool incognito) {
  DCHECK(!IsWindow());
  chrome_frame_delegate_ = chrome_frame_delegate;
  incognito_ = incognito;
  ui_thread_id_ = PlatformThread::CurrentId();

#ifndef NDEBUG
  // In debug mode give more time to work with a debugger.
  if (IsDebuggerPresent()) {
    // Don't use INFINITE (which is -1) or even MAXINT since we will convert
    // from milliseconds to microseconds when stored in a base::TimeDelta,
    // thus * 1000. An hour should be enough.
    automation_server_launch_timeout = 60 * 60 * 1000;
  } else {
    DCHECK_LT(automation_server_launch_timeout, MAXINT / 2000);
    automation_server_launch_timeout *= 2;
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

  // Mark our state as initializing.  We'll reach initialized once
  // InitializeComplete is called successfully.
  init_state_ = INITIALIZING;

  automation_server_id_ = proxy_factory_->GetAutomationServer(
      automation_server_launch_timeout,
      profile_name, extra_chrome_arguments, perform_version_check,
      static_cast<ProxyFactory::LaunchDelegate*>(this));

  return true;
}

void ChromeFrameAutomationClient::Uninitialize() {
  DLOG(INFO) << __FUNCTION__;

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
  if (tab_.get()) {
    tab_->RemoveObserver(this);
    tab_ = NULL;    // scoped_refptr::Release
  }

  // Clean up any outstanding requests
  CleanupRequests();

  // Wait for the background thread to exit.
  ReleaseAutomationServer();

  // We must destroy the window, since if there are pending tasks
  // window procedure may be invoked after DLL is unloaded.
  // Unfortunately pending tasks are leaked.
  if (m_hWnd)
    DestroyWindow();

  chrome_frame_delegate_ = NULL;
  init_state_ = UNINITIALIZED;
}

bool ChromeFrameAutomationClient::InitiateNavigation(
    const std::string& url, const std::string& referrer, bool is_privileged) {
  if (url.empty())
    return false;

  url_ = GURL(url);

  // Catch invalid URLs early.
  if (!url_.is_valid() || !IsValidUrlScheme(UTF8ToWide(url), is_privileged)) {
    DLOG(ERROR) << "Invalid URL passed to InitiateNavigation: " << url
                << " is_privileged=" << is_privileged;
    return false;
  }

  if (is_initialized()) {
    BeginNavigate(GURL(url), GURL(referrer));
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
  automation_server_->SendAsAsync(msg, NewCallback(this,
      &ChromeFrameAutomationClient::BeginNavigateCompleted), this);
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
  automation_server_->SendAsAsync(msg, NewCallback(this,
      &ChromeFrameAutomationClient::BeginNavigateCompleted), this);

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

// Class that maintains context during the async load/install extension
// operation.  When done, InstallExtensionComplete is posted back to the UI
// thread so that the users of ChromeFrameAutomationClient can be notified.
class InstallExtensionContext {
 public:
  InstallExtensionContext(ChromeFrameAutomationClient* client,
      const FilePath& crx_path, void* user_data) : client_(client),
      crx_path_(crx_path), user_data_(user_data) {
  }

  ~InstallExtensionContext() {
  }

  void InstallExtensionComplete(AutomationMsg_ExtensionResponseValues res) {
    client_->PostTask(FROM_HERE, NewRunnableMethod(client_,
        &ChromeFrameAutomationClient::InstallExtensionComplete, crx_path_,
        user_data_, res));
    delete this;
  }

 private:
  ChromeFrameAutomationClient* client_;
  FilePath crx_path_;
  void* user_data_;
};

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
  automation_server_->SendAsAsync(msg, NewCallback(ctx,
      &InstallExtensionContext::InstallExtensionComplete), this);
}

void ChromeFrameAutomationClient::InstallExtensionComplete(
    const FilePath& crx_path,
    void* user_data,
    AutomationMsg_ExtensionResponseValues res) {
  DCHECK(PlatformThread::CurrentId() == ui_thread_id_);

  if (chrome_frame_delegate_) {
    chrome_frame_delegate_->OnExtensionInstalled(crx_path, user_data, res);
  }
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
  automation_server_->SendAsAsync(msg, NewCallback(ctx,
      &InstallExtensionContext::InstallExtensionComplete), this);
}

void ChromeFrameAutomationClient::CreateExternalTab() {
  AutomationLaunchResult launch_result = AUTOMATION_SUCCESS;
  DCHECK(IsWindow());
  DCHECK(automation_server_ != NULL);

  const IPC::ExternalTabSettings settings = {
    m_hWnd,
    gfx::Rect(),
    WS_CHILD,
    incognito_,
    !use_chrome_network_,
    handle_top_level_requests_,
    GURL(url_)
  };

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "ChromeFrame.HostNetworking", !use_chrome_network_, 0, 1, 2);

  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "ChromeFrame.HandleTopLevelRequests", handle_top_level_requests_, 0, 1,
      2);

  IPC::SyncMessage* message =
      new AutomationMsg_CreateExternalTab(0, settings, NULL, NULL, NULL);
  automation_server_->SendAsAsync(message, NewCallback(this,
      &ChromeFrameAutomationClient::CreateExternalTabComplete), this);
}

void ChromeFrameAutomationClient::CreateExternalTabComplete(HWND chrome_window,
    HWND tab_window, int tab_handle) {
  if (!automation_server_) {
    // If we receive this notification while shutting down, do nothing.
    DLOG(ERROR) << "CreateExternalTabComplete called when automation server "
                << "was null!";
    return;
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

  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::InitializeComplete, launch_result));
}

void ChromeFrameAutomationClient::SetEnableExtensionAutomation(
    const std::vector<std::string>& functions_enabled) {
  if (!is_initialized())
    return;

  automation_server_->SetEnableExtensionAutomation(functions_enabled);
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
      if (external_tab_cookie_ == NULL) {
        // Continue with Initialization - Create external tab
        CreateExternalTab();
      } else {
        // Send a notification to Chrome that we are ready to connect to the
        // ExternalTab.
        IPC::SyncMessage* message =
            new AutomationMsg_ConnectExternalTab(0, external_tab_cookie_, NULL,
                                                 NULL, NULL);
        automation_server_->SendAsAsync(message, NewCallback(this,
            &ChromeFrameAutomationClient::CreateExternalTabComplete), this);
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
  DCHECK(PlatformThread::CurrentId() == ui_thread_id_);
  std::string version = automation_server_->server_version();

  if (result != AUTOMATION_SUCCESS) {
    DLOG(WARNING) << "InitializeComplete: failure " << result;
    ReleaseAutomationServer();
  } else {
    init_state_ = INITIALIZED;

    // If the host already have a window, ask Chrome to re-parent.
    if (parent_window_)
      SetParentWindow(parent_window_);
  }

  if (chrome_frame_delegate_) {
    if (result == AUTOMATION_SUCCESS) {
      chrome_frame_delegate_->OnAutomationServerReady();
    } else {
      chrome_frame_delegate_->OnAutomationServerLaunchFailed(result, version);
    }
  }
}

// This is invoked in channel's background thread.
// Cannot call any method of the activex/npapi here since they are STA
// kind of beings.
// By default we marshal the IPC message to the main/GUI thread and from there
// we safely invoke chrome_frame_delegate_->OnMessageReceived(msg).
void ChromeFrameAutomationClient::OnMessageReceived(TabProxy* tab,
                                                    const IPC::Message& msg) {
  DCHECK(tab == tab_.get());

  // Early check to avoid needless marshaling
  if (chrome_frame_delegate_ == NULL)
    return;

  CallDelegate(FROM_HERE, NewRunnableMethod(chrome_frame_delegate_,
      &ChromeFrameDelegate::OnMessageReceived, msg));
}

void ChromeFrameAutomationClient::ReportNavigationError(
    AutomationMsg_NavigationResponseValues error_code,
    const std::string& url) {
  CallDelegate(FROM_HERE, NewRunnableMethod(chrome_frame_delegate_,
      &ChromeFrameDelegate::OnLoadFailed,
      error_code,
      url));
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
        NOTREACHED();
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
  DLOG(INFO) << __FUNCTION__;
  if (automation_server_id_) {
    // Cache the server id and clear the automation_server_id_ before
    // calling ReleaseAutomationServer.  The reason we do this is that
    // we must cancel pending messages before we release the automation server.
    // Furthermore, while ReleaseAutomationServer is running, we could get
    // a callback to LaunchComplete which is where we normally get our pointer
    // to the automation server and there we check the server id for NULLness
    // and do nothing if it is NULL.
    void* server_id = automation_server_id_;
    automation_server_id_ = NULL;

    if (automation_server_) {
      // Make sure to clean up any pending sync messages before we go away.
      automation_server_->CancelAsync(this);
      automation_server_ = NULL;
    }

    proxy_factory_->ReleaseAutomationServer(server_id);

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

void ChromeFrameAutomationClient::CallDelegate(
    const tracked_objects::Location& from_here, Task* delegate_task ) {
  delegate_task->SetBirthPlace(from_here);
  PostTask(FROM_HERE, NewRunnableMethod(this,
      &ChromeFrameAutomationClient::CallDelegateImpl,
      delegate_task));
}

void ChromeFrameAutomationClient::CallDelegateImpl(Task* delegate_task) {
  if (chrome_frame_delegate_) {
    // task's object should be == chrome_frame_delegate_
    delegate_task->Run();
  }

  delete delegate_task;
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

// IPC:Message::Sender implementation
bool ChromeFrameAutomationClient::Send(IPC::Message* msg) {
  return automation_server_->Send(msg);
}

bool ChromeFrameAutomationClient::AddRequest(PluginUrlRequest* request) {
  if (!request) {
    NOTREACHED();
    return false;
  }

  DCHECK(request_map_.end() == request_map_.find(request->id()));
  request_map_[request->id()] =  request;
  return true;
}

bool ChromeFrameAutomationClient::ReadRequest(
    int request_id, int bytes_to_read) {
  bool result = false;
  PluginUrlRequest* request = LookupRequest(request_id);
  if (request)
    result = request->Read(bytes_to_read);

  return result;
}

void ChromeFrameAutomationClient::RemoveRequest(PluginUrlRequest* request) {
  DCHECK(request_map_.end() != request_map_.find(request->id()));
  request_map_.erase(request->id());
}

void ChromeFrameAutomationClient::RemoveRequest(
    int request_id, int reason, bool abort) {
  PluginUrlRequest* request = LookupRequest(request_id);
  if (request) {
    if (abort) {
      // The request object will get removed asynchronously.
      request->Stop();
    } else {
      request_map_.erase(request_id);
    }
  }
}

PluginUrlRequest* ChromeFrameAutomationClient::LookupRequest(
    int request_id) const {
  PluginUrlRequest* request = NULL;
  RequestMap::const_iterator it = request_map_.find(request_id);
  if (request_map_.end() != it)
    request = (*it).second;
  return request;
}

bool ChromeFrameAutomationClient::IsValidRequest(
    PluginUrlRequest* request) const {
  bool is_valid = false;
  // if request is invalid then request->id() won't work
  // hence perform reverse map lookup for validity of the
  // request pointer.
  if (request) {
    for (RequestMap::const_iterator it = request_map_.begin();
        it != request_map_.end(); it++) {
      if (request == (*it).second) {
        is_valid = true;
        break;
      }
    }
  }

  return is_valid;
}

void ChromeFrameAutomationClient::CleanupRequests() {
  while (request_map_.size()) {
    PluginUrlRequest* request = request_map_.begin()->second;
    if (request) {
      int request_id = request->id();
      request->Stop();
    }
  }

  DCHECK(request_map_.empty());
  request_map_.clear();
}

void ChromeFrameAutomationClient::CleanupAsyncRequests() {
  RequestMap::iterator index = request_map_.begin();
  while (index != request_map_.end()) {
    PluginUrlRequest* request = (*index).second;
    if (request) {
      int request_id = request->id();
      request->Stop();
    }
    index++;
  }

  request_map_.clear();
}

bool ChromeFrameAutomationClient::Reinitialize(
    ChromeFrameDelegate* delegate) {
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
  SetParentWindow(NULL);
  return true;
}

void ChromeFrameAutomationClient::AttachExternalTab(
    intptr_t external_tab_cookie) {
  DCHECK_EQ(static_cast<TabProxy*>(NULL), tab_.get());
  DCHECK_EQ(-1, tab_handle_);

  external_tab_cookie_ = external_tab_cookie;
}

void ChromeFrameAutomationClient::SetPageFontSize(
    enum AutomationPageFontSize font_size) {
  if (font_size < SMALLEST_FONT ||
      font_size > LARGEST_FONT) {
      NOTREACHED() << "Invalid font size specified : "
                   << font_size;
      return;
  }

  Send(new AutomationMsg_SetPageFontSize(0, tab_handle_, font_size));
}

