// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_
#define CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_

#include <atlbase.h>
#include <atlwin.h>
#include <string>
#include <map>
#include <vector>

#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "base/stack_container.h"
#include "base/task.h"
#include "base/timer.h"
#include "base/thread.h"
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

// We extend the AutomationProxy class to handle our custom
// IPC messages
class ChromeFrameAutomationProxyImpl : public ChromeFrameAutomationProxy,
  // We have to derive from automationproxy since we want access to some members
  // (tracker_ & channel_) - simple aggregation wont work;
  // .. and non-public inheritance is verboten.
  public AutomationProxy {
 public:
  virtual void SendAsAsync(
      IPC::SyncMessage* msg,
      SyncMessageReplyDispatcher::SyncMessageCallContext* context,
      void* key);

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
  explicit ChromeFrameAutomationProxyImpl(int launch_timeout);
  ~ChromeFrameAutomationProxyImpl();
  class CFMsgDispatcher;
  scoped_refptr<CFMsgDispatcher> sync_;
  class TabProxyNotificationMessageFilter;
  scoped_refptr<TabProxyNotificationMessageFilter> message_filter_;
  friend class ProxyFactory;
};

// This structure contains information used for launching chrome.
struct ChromeFrameLaunchParams {
  int automation_server_launch_timeout;
  GURL url;
  GURL referrer;
  FilePath profile_path;
  std::wstring profile_name;
  std::wstring extra_chrome_arguments;
  bool perform_version_check;
  bool incognito_mode;
  bool is_widget_mode;
};

// We must create and destroy automation proxy in a thread with a message loop.
// Hence thread cannot be a member of the proxy.
class ProxyFactory {
 public:
  // Callback when chrome process launch is complete and automation handshake
  // (Hello message) is established.
  struct DECLSPEC_NOVTABLE LaunchDelegate {  // NOLINT
    virtual void LaunchComplete(ChromeFrameAutomationProxy* proxy,
                                AutomationLaunchResult result) = 0;
  };  // NOLINT

  ProxyFactory();
  virtual ~ProxyFactory();

  virtual void GetAutomationServer(LaunchDelegate* delegate,
                                   const ChromeFrameLaunchParams& params,
                                   void** automation_server_id);
  virtual bool ReleaseAutomationServer(void* server_id);

 private:
  struct ProxyCacheEntry {
    std::wstring profile_name;
    int ref_count;
    scoped_ptr<base::Thread> thread;
    ChromeFrameAutomationProxyImpl* proxy;
    AutomationLaunchResult launch_result;
    explicit ProxyCacheEntry(const std::wstring& profile);
  };

  void CreateProxy(ProxyCacheEntry* entry,
                   const ChromeFrameLaunchParams& params,
                   LaunchDelegate* delegate);
  void ReleaseProxy(ProxyCacheEntry* entry, base::WaitableEvent* done);

  void SendUMAData(ProxyCacheEntry* proxy_entry);

  typedef StackVector<ProxyCacheEntry*, 4> Vector;
  Vector proxies_;
  // Lock if we are going to call GetAutomationServer from more than one thread.
  Lock lock_;

  // Used for UMA histogram logging to measure the time for the chrome
  // automation server to start;
  base::TimeTicks automation_server_launch_start_time_;

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
      public ProxyFactory::LaunchDelegate {
 public:
  ChromeFrameAutomationClient();
  ~ChromeFrameAutomationClient();

  // Called from UI thread.
  virtual bool Initialize(ChromeFrameDelegate* chrome_frame_delegate,
                          const ChromeFrameLaunchParams& chrome_launch_params);
  void Uninitialize();

  virtual bool InitiateNavigation(const std::string& url,
                                  const std::string& referrer,
                                  bool is_privileged);
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

  // Sends an IPC message to the external tab container requesting it to run
  // unload handlers on the page.
  void RunUnloadHandlers(HWND notification_window, int notification_message);

 protected:
  // ChromeFrameAutomationProxy::LaunchDelegate implementation.
  virtual void LaunchComplete(ChromeFrameAutomationProxy* proxy,
                            AutomationLaunchResult result);
  // TabProxyDelegate implementation
  virtual void OnMessageReceived(TabProxy* tab, const IPC::Message& msg);
  virtual void OnChannelError(TabProxy* tab);

  void CreateExternalTab();
  AutomationLaunchResult CreateExternalTabComplete(HWND chrome_window,
                                                   HWND tab_window,
                                                   int tab_handle);
  // Called in UI thread. Here we fire event to the client notifying for
  // the result of Initialize() method call.
  void InitializeComplete(AutomationLaunchResult result);

  virtual void OnFinalMessage(HWND wnd) {
    Release();
  }

 private:
  void OnMessageReceivedUIThread(const IPC::Message& msg);
  void OnChannelErrorUIThread();

  HWND chrome_window() const { return chrome_window_; }
  void BeginNavigate(const GURL& url, const GURL& referrer);
  void BeginNavigateCompleted(AutomationMsg_NavigationResponseValues result);

  // Helpers
  void ReportNavigationError(AutomationMsg_NavigationResponseValues error_code,
                             const std::string& url);
  bool is_initialized() const {
    return init_state_ == INITIALIZED;
  }

  HWND parent_window_;
  PlatformThreadId ui_thread_id_;

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
  // Only used if we attach to an existing tab.
  uint64 external_tab_cookie_;

  // Set to true if we received a navigation request prior to the automation
  // server being initialized.
  bool navigate_after_initialization_;

  ChromeFrameLaunchParams chrome_launch_params_;

  // When host network stack is used, this object is in charge of
  // handling network requests.
  PluginUrlRequestManager* url_fetcher_;
  PluginUrlRequestManager::ThreadSafeFlags url_fetcher_flags_;

  bool ProcessUrlRequestMessage(TabProxy* tab, const IPC::Message& msg,
                                bool ui_thread);

  // PluginUrlRequestDelegate implementation. Simply adds tab's handle
  // as parameter and forwards to Chrome via IPC.
  virtual void OnResponseStarted(int request_id, const char* mime_type,
      const char* headers, int size, base::Time last_modified,
      const std::string& redirect_url, int redirect_status);
  virtual void OnReadComplete(int request_id, const std::string& data);
  virtual void OnResponseEnd(int request_id, const URLRequestStatus& status);
  virtual void OnCookiesRetrieved(bool success, const GURL& url,
      const std::string& cookie_string, int cookie_id);

  friend class BeginNavigateContext;
  friend class CreateExternalTabContext;

 public:
  void SetUrlFetcher(PluginUrlRequestManager* url_fetcher) {
    DCHECK(url_fetcher != NULL);
    url_fetcher_ = url_fetcher;
    url_fetcher_flags_ = url_fetcher->GetThreadSafeFlags();
    url_fetcher_->set_delegate(this);
  }
};

#endif  // CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_
