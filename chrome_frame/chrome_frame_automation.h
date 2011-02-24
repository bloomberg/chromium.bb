// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_
#define CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_

#include <atlbase.h>
#include <atlwin.h>
#include <string>
#include <map>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/stack_container.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/timer.h"
#include "chrome/common/page_zoom.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/chrome_frame_histograms.h"
#include "chrome_frame/plugin_url_request.h"
#include "chrome_frame/sync_msg_reply_dispatcher.h"

// By a convoluated route, this timeout also winds up being the sync automation
// message timeout. See the ChromeFrameAutomationProxyImpl ctor and the
// AutomationProxy ctor for details.
const unsigned long kCommandExecutionTimeout = 60000;  // NOLINT, 60 seconds

class ProxyFactory;
class NavigationConstraints;
enum AutomationPageFontSize;

struct DECLSPEC_NOVTABLE ChromeFrameAutomationProxy {  // NOLINT
  virtual bool Send(IPC::Message* msg) = 0;

  virtual void SendAsAsync(
      IPC::SyncMessage* msg,
      SyncMessageReplyDispatcher::SyncMessageCallContext* context,
      void* key) = 0;
  virtual void CancelAsync(void* key) = 0;
  virtual scoped_refptr<TabProxy> CreateTabProxy(int handle) = 0;
  virtual void ReleaseTabProxy(AutomationHandle handle) = 0;
  virtual std::string server_version() = 0;

  virtual void SendProxyConfig(const std::string&) = 0;
 protected:
  virtual ~ChromeFrameAutomationProxy() {}
};

// Forward declarations.
class ProxyFactory;

// We extend the AutomationProxy class to handle our custom
// IPC messages
class ChromeFrameAutomationProxyImpl
  : public ChromeFrameAutomationProxy,
    // We have to derive from automationproxy since we want access to some
    // members (tracker_ & channel_) - simple aggregation wont work;
    // .. and non-public inheritance is verboten.
    public AutomationProxy {
 public:
  ~ChromeFrameAutomationProxyImpl();
  virtual void SendAsAsync(
      IPC::SyncMessage* msg,
      SyncMessageReplyDispatcher::SyncMessageCallContext* context,
      void* key);

  // Called on the worker thread.
  virtual void OnChannelError();

  virtual void CancelAsync(void* key);

  virtual scoped_refptr<TabProxy> CreateTabProxy(int handle);
  virtual void ReleaseTabProxy(AutomationHandle handle);

  virtual std::string server_version() {
    return AutomationProxy::server_version();
  }

  virtual bool Send(IPC::Message* msg) {
    return AutomationProxy::Send(msg);
  }

  virtual void SendProxyConfig(const std::string& p) {
    AutomationProxy::SendProxyConfig(p);
  }

 protected:
  friend class AutomationProxyCacheEntry;
  ChromeFrameAutomationProxyImpl(AutomationProxyCacheEntry* entry,
                                 std::string channel_id,
                                 int launch_timeout);

  class CFMsgDispatcher;
  class TabProxyNotificationMessageFilter;

  scoped_refptr<CFMsgDispatcher> sync_;
  scoped_refptr<TabProxyNotificationMessageFilter> message_filter_;
  AutomationProxyCacheEntry* proxy_entry_;
};

// This class contains information used for launching chrome.
class ChromeFrameLaunchParams :  // NOLINT
    public base::RefCountedThreadSafe<ChromeFrameLaunchParams> {
 public:
  ChromeFrameLaunchParams(const GURL& url, const GURL& referrer,
                          const FilePath& profile_path,
                          const std::wstring& profile_name,
                          const std::wstring& language,
                          const std::wstring& extra_arguments,
                          bool incognito, bool widget_mode,
                          bool route_all_top_level_navigations)
    : launch_timeout_(kCommandExecutionTimeout), url_(url),
      referrer_(referrer), profile_path_(profile_path),
      profile_name_(profile_name), language_(language),
      extra_arguments_(extra_arguments), version_check_(true),
      incognito_mode_(incognito), is_widget_mode_(widget_mode),
      route_all_top_level_navigations_(route_all_top_level_navigations) {
  }

  ~ChromeFrameLaunchParams() {
  }

  void set_launch_timeout(int timeout) {
    launch_timeout_ = timeout;
  }

  int launch_timeout() const {
    return launch_timeout_;
  }

  const GURL& url() const {
    return url_;
  }

  void set_url(const GURL& url) {
    url_ = url;
  }

  const GURL& referrer() const {
    return referrer_;
  }

  void set_referrer(const GURL& referrer) {
    referrer_ = referrer;
  }

  const FilePath& profile_path() const {
    return profile_path_;
  }

  const std::wstring& profile_name() const {
    return profile_name_;
  }

  const std::wstring& language() const {
    return language_;
  }

  const std::wstring& extra_arguments() const {
    return extra_arguments_;
  }

  bool version_check() const {
    return version_check_;
  }

  void set_version_check(bool check) {
    version_check_ = check;
  }

  bool incognito() const {
    return incognito_mode_;
  }

  bool widget_mode() const {
    return is_widget_mode_;
  }

  void set_route_all_top_level_navigations(
      bool route_all_top_level_navigations) {
    route_all_top_level_navigations_ = route_all_top_level_navigations;
  }

  bool route_all_top_level_navigations() const {
    return route_all_top_level_navigations_;
  }

 protected:
  int launch_timeout_;
  GURL url_;
  GURL referrer_;
  FilePath profile_path_;
  std::wstring profile_name_;
  std::wstring language_;
  std::wstring extra_arguments_;
  bool version_check_;
  bool incognito_mode_;
  bool is_widget_mode_;
  bool route_all_top_level_navigations_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeFrameLaunchParams);
};

// Callback when chrome process launch is complete and automation handshake
// (Hello message) is established.
struct DECLSPEC_NOVTABLE LaunchDelegate {  // NOLINT
  virtual void LaunchComplete(ChromeFrameAutomationProxy* proxy,
                              AutomationLaunchResult result) = 0;
  virtual void AutomationServerDied() = 0;
};  // NOLINT

// Manages a cached ChromeFrameAutomationProxyImpl entry and holds
// reference-less pointers to LaunchDelegate(s) to be notified in case
// of automation server process changes.
class AutomationProxyCacheEntry
    : public base::RefCounted<AutomationProxyCacheEntry> {
 public:
  AutomationProxyCacheEntry(ChromeFrameLaunchParams* params,
                            LaunchDelegate* delegate);

  ~AutomationProxyCacheEntry();

  void AddDelegate(LaunchDelegate* delegate);
  void RemoveDelegate(LaunchDelegate* delegate, base::WaitableEvent* done,
                      bool* was_last_delegate);

  void StartSendUmaInterval(ChromeFrameHistogramSnapshots* snapshots,
                            int send_interval);

  DWORD WaitForThread(DWORD timeout) {  // NOLINT
    DCHECK(thread_.get());
    return ::WaitForSingleObject(thread_->thread_handle(), timeout);
  }

  bool IsSameProfile(const std::wstring& name) const {
    return lstrcmpiW(name.c_str(), profile_name.c_str()) == 0;
  }

  base::Thread* thread() const {
    return thread_.get();
  }

  MessageLoop* message_loop() const {
    return thread_->message_loop();
  }

  bool IsSameThread(base::PlatformThreadId id) const {
    return thread_->thread_id() == id;
  }

  ChromeFrameAutomationProxyImpl* proxy() const {
    DCHECK(IsSameThread(base::PlatformThread::CurrentId()));
    return proxy_.get();
  }

  // Called by the proxy when the automation server has unexpectedly gone away.
  void OnChannelError();

 protected:
  void CreateProxy(ChromeFrameLaunchParams* params,
                   LaunchDelegate* delegate);
  void SendUMAData();

 protected:
  std::wstring profile_name;
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<ChromeFrameAutomationProxyImpl> proxy_;
  AutomationLaunchResult launch_result_;
  typedef std::vector<LaunchDelegate*> LaunchDelegates;
  LaunchDelegates launch_delegates_;
  // Used for UMA histogram logging to measure the time for the chrome
  // automation server to start;
  base::TimeTicks automation_server_launch_start_time_;
  ChromeFrameHistogramSnapshots* snapshots_;
  int uma_send_interval_;
};

// We must create and destroy automation proxy in a thread with a message loop.
// Hence thread cannot be a member of the proxy.
class ProxyFactory {
 public:
  ProxyFactory();
  virtual ~ProxyFactory();

  // Fetches or creates a new automation server instance.
  // delegate may be NULL.  If non-null, a pointer to the delegate will
  // be stored for the lifetime of the automation process or until
  // ReleaseAutomationServer is called.
  virtual void GetAutomationServer(LaunchDelegate* delegate,
                                   ChromeFrameLaunchParams* params,
                                   void** automation_server_id);
  virtual bool ReleaseAutomationServer(void* server_id,
                                       LaunchDelegate* delegate);

 private:
  typedef StackVector<scoped_refptr<AutomationProxyCacheEntry>, 4> Vector;
  Vector proxies_;
  // Lock if we are going to call GetAutomationServer from more than one thread.
  base::Lock lock_;

  // Gathers histograms to be sent to Chrome.
  ChromeFrameHistogramSnapshots chrome_frame_histograms_;

  // Interval for sending UMA data
  int uma_send_interval_;
};

// Handles all automation requests initiated from the chrome frame objects.
// These include the chrome tab/chrome frame activex/chrome frame npapi
// plugin objects.
class ChromeFrameAutomationClient
    : public CWindowImpl<ChromeFrameAutomationClient>,
      public TaskMarshallerThroughWindowsMessages<ChromeFrameAutomationClient>,
      public base::RefCountedThreadSafe<ChromeFrameAutomationClient>,
      public PluginUrlRequestDelegate,
      public TabProxy::TabProxyDelegate,
      public LaunchDelegate {
 public:
  ChromeFrameAutomationClient();
  ~ChromeFrameAutomationClient();

  // Called from UI thread.
  virtual bool Initialize(ChromeFrameDelegate* chrome_frame_delegate,
                          ChromeFrameLaunchParams* chrome_launch_params);
  void Uninitialize();
  void NotifyAndUninitialize();

  virtual bool InitiateNavigation(
      const std::string& url,
      const std::string& referrer,
      NavigationConstraints* navigation_constraints);

  virtual bool NavigateToIndex(int index);
  bool ForwardMessageFromExternalHost(const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);
  bool SetProxySettings(const std::string& json_encoded_proxy_settings);

  virtual void SetEnableExtensionAutomation(
      const std::vector<std::string>& functions_enabled);

  void FindInPage(const std::wstring& search_string,
                  FindInPageDirection forward,
                  FindInPageCase match_case,
                  bool find_next);

  virtual void InstallExtension(const FilePath& crx_path, void* user_data);

  virtual void LoadExpandedExtension(const FilePath& path, void* user_data);

  // Starts a request to get the list of enabled extensions' base directories.
  // Response comes back as ChromeFrameDelegate::OnEnabledExtensions().
  virtual void GetEnabledExtensions(void* user_data);

  virtual void InstallExtensionComplete(
      const FilePath& path,
      void* user_data,
      AutomationMsg_ExtensionResponseValues res);

  virtual void GetEnabledExtensionsComplete(
      void* user_data,
      std::vector<FilePath>* extension_directories);

  // Returns the session ID used to identify a Tab in Chrome.
  virtual int GetSessionId() const;

  virtual void OnChromeFrameHostMoved();

  TabProxy* tab() const { return tab_.get(); }

  BEGIN_MSG_MAP(ChromeFrameAutomationClient)
    CHAIN_MSG_MAP(
        TaskMarshallerThroughWindowsMessages<ChromeFrameAutomationClient>)
  END_MSG_MAP()

  // Resizes the hosted chrome window. This is brokered to the chrome
  // automation instance as the host browser could be running under low IL,
  // which would cause the SetWindowPos call to fail.
  void Resize(int width, int height, int flags);

  // Sets the passed in window as the parent of the external tab.
  void SetParentWindow(HWND parent_window);

  void SendContextMenuCommandToChromeFrame(int selected_command);

  HWND tab_window() const {
    return tab_window_;
  }

  void ReleaseAutomationServer();

  // Returns the version number of plugin dll.
  std::wstring GetVersion() const;

  // BitBlts the contents of the chrome window to the print dc.
  void Print(HDC print_dc, const RECT& print_bounds);

  // Called in full tab mode and indicates a request to chrome to print
  // the whole tab.
  void PrintTab();

  void set_use_chrome_network(bool use_chrome_network) {
    use_chrome_network_ = use_chrome_network;
  }
  bool use_chrome_network() const {
    return use_chrome_network_;
  }

#ifdef UNIT_TEST
  void set_proxy_factory(ProxyFactory* factory) {
    proxy_factory_ = factory;
  }
#endif

  void set_handle_top_level_requests(bool handle_top_level_requests) {
    handle_top_level_requests_ = handle_top_level_requests;
  }

  // Url request manager set up.
  void SetUrlFetcher(PluginUrlRequestManager* url_fetcher);

  // Called if the same instance of the ChromeFrameAutomationClient object
  // is reused.
  bool Reinitialize(ChromeFrameDelegate* chrome_frame_delegate,
                    PluginUrlRequestManager* url_fetcher);

  // Attaches an existing external tab to this automation client instance.
  void AttachExternalTab(uint64 external_tab_cookie);
  void BlockExternalTab(uint64 cookie);

  void SetPageFontSize(enum AutomationPageFontSize);

  // For IDeleteBrowsingHistorySupport
  void RemoveBrowsingData(int remove_mask);

  // Sets the current zoom level on the tab.
  void SetZoomLevel(PageZoom::Function zoom_level);

  // Fires before unload and unload handlers on the page if any. Allows the
  // the website to put up a confirmation dialog on unload.
  void OnUnload(bool* should_unload);

  void set_route_all_top_level_navigations(
      bool route_all_top_level_navigations) {
    route_all_top_level_navigations_ = route_all_top_level_navigations;
  }

 protected:
  // ChromeFrameAutomationProxy::LaunchDelegate implementation.
  virtual void LaunchComplete(ChromeFrameAutomationProxy* proxy,
                              AutomationLaunchResult result);
  virtual void AutomationServerDied();

  // TabProxyDelegate implementation
  virtual bool OnMessageReceived(TabProxy* tab, const IPC::Message& msg);
  virtual void OnChannelError(TabProxy* tab);

  void CreateExternalTab();
  AutomationLaunchResult CreateExternalTabComplete(HWND chrome_window,
                                                   HWND tab_window,
                                                   int tab_handle,
                                                   int session_id);
  // Called in UI thread. Here we fire event to the client notifying for
  // the result of Initialize() method call.
  void InitializeComplete(AutomationLaunchResult result);

  virtual void OnFinalMessage(HWND wnd) {
    Release();
  }

  scoped_refptr<ChromeFrameLaunchParams> launch_params() {
    return chrome_launch_params_;
  }

 private:
  void OnMessageReceivedUIThread(const IPC::Message& msg);
  void OnChannelErrorUIThread();

  HWND chrome_window() const { return chrome_window_; }
  void BeginNavigate();
  void BeginNavigateCompleted(AutomationMsg_NavigationResponseValues result);

  // Helpers
  void ReportNavigationError(AutomationMsg_NavigationResponseValues error_code,
                             const std::string& url);

  bool ProcessUrlRequestMessage(TabProxy* tab, const IPC::Message& msg,
                                bool ui_thread);

  // PluginUrlRequestDelegate implementation. Simply adds tab's handle
  // as parameter and forwards to Chrome via IPC.
  virtual void OnResponseStarted(int request_id, const char* mime_type,
      const char* headers, int size, base::Time last_modified,
      const std::string& redirect_url, int redirect_status,
      const net::HostPortPair& socket_address);
  virtual void OnReadComplete(int request_id, const std::string& data);
  virtual void OnResponseEnd(int request_id,
                             const net::URLRequestStatus& status);
  virtual void OnCookiesRetrieved(bool success, const GURL& url,
      const std::string& cookie_string, int cookie_id);

  bool is_initialized() const {
    return init_state_ == INITIALIZED;
  }

  HWND parent_window_;
  base::PlatformThreadId ui_thread_id_;

  void* automation_server_id_;
  ChromeFrameAutomationProxy* automation_server_;

  HWND chrome_window_;
  scoped_refptr<TabProxy> tab_;
  ChromeFrameDelegate* chrome_frame_delegate_;

  // Handle to the underlying chrome window. This is a child of the external
  // tab window.
  HWND tab_window_;

  // Keeps track of the version of Chrome we're talking to.
  std::string automation_server_version_;

  typedef enum InitializationState {
    UNINITIALIZED = 0,
    INITIALIZING,
    INITIALIZED,
    UNINITIALIZING,
  };

  InitializationState init_state_;
  bool use_chrome_network_;
  bool handle_top_level_requests_;
  ProxyFactory* proxy_factory_;
  int tab_handle_;
  // The SessionId used by Chrome as the id in the Javascript Tab object.
  int session_id_;
  // Only used if we attach to an existing tab.
  uint64 external_tab_cookie_;

  // Set to true if we received a navigation request prior to the automation
  // server being initialized.
  bool navigate_after_initialization_;

  scoped_refptr<ChromeFrameLaunchParams> chrome_launch_params_;

  // Cache security manager for URL zone checking
  ScopedComPtr<IInternetSecurityManager> security_manager_;

  // When host network stack is used, this object is in charge of
  // handling network requests.
  PluginUrlRequestManager* url_fetcher_;
  PluginUrlRequestManager::ThreadSafeFlags url_fetcher_flags_;

  // set to true if the host needs to get notified of all top level navigations
  // in this page. This typically applies to hosts which would render the new
  // page without chrome frame. Defaults to false.
  bool route_all_top_level_navigations_;

  friend class BeginNavigateContext;
  friend class CreateExternalTabContext;
};

#endif  // CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_
