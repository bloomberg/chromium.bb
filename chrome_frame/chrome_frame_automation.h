// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_
#define CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_

#include <atlbase.h>
#include <atlwin.h>
#include <string>
#include <map>

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

// By a convoluated route, this timeout also winds up being the sync automation
// message timeout. See the ChromeFrameAutomationProxyImpl ctor and the
// AutomationProxy ctor for details.
const unsigned long kCommandExecutionTimeout = 60000;  // NOLINT, 60 seconds

class ProxyFactory;
enum AutomationPageFontSize;

struct DECLSPEC_NOVTABLE ChromeFrameAutomationProxy {
  virtual bool Send(IPC::Message* msg) = 0;

  virtual void SendAsAsync(IPC::SyncMessage* msg, void* callback,
                           void* key) = 0;
  virtual void CancelAsync(void* key) = 0;
  virtual scoped_refptr<TabProxy> CreateTabProxy(int handle) = 0;
  virtual std::string server_version() = 0;

  virtual void SendProxyConfig(const std::string&) = 0;
 protected:
  ~ChromeFrameAutomationProxy() {}
};

// We extend the AutomationProxy class to handle our custom
// IPC messages
class ChromeFrameAutomationProxyImpl : public ChromeFrameAutomationProxy,
  // We have to derive from automationproxy since we want access to some members
  // (tracker_ & channel_) - simple aggregation wont work;
  // .. and non-public inheritance is verboten.
  public AutomationProxy {
 public:
  virtual void SendAsAsync(IPC::SyncMessage* msg, void* callback, void* key);

  virtual void CancelAsync(void* key);

  virtual scoped_refptr<TabProxy> CreateTabProxy(int handle);
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
  friend class ProxyFactory;
};

// This structure contains information used for launching chrome.
struct ChromeFrameLaunchParams {
  int automation_server_launch_timeout;
  GURL url;
  GURL referrer;
  std::wstring profile_name;
  std::wstring extra_chrome_arguments;
  bool perform_version_check;
  bool incognito_mode;
};

// We must create and destroy automation proxy in a thread with a message loop.
// Hence thread cannot be a member of the proxy.
class ProxyFactory {
 public:
  // Callback when chrome process launch is complete and automation handshake
  // (Hello message) is established.
  struct DECLSPEC_NOVTABLE LaunchDelegate {
    virtual void LaunchComplete(ChromeFrameAutomationProxy* proxy,
                                AutomationLaunchResult result) = 0;
  };

  ProxyFactory();
  ~ProxyFactory();

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
  void DestroyProxy(ProxyCacheEntry* entry);

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
                          int automation_server_launch_timeout,
                          bool perform_version_check,
                          const std::wstring& profile_name,
                          const std::wstring& extra_chrome_arguments,
                          bool incognito);
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

  virtual void InstallExtensionComplete(
      const FilePath& path,
      void* user_data,
      AutomationMsg_ExtensionResponseValues res);

  TabProxy* tab() const { return tab_.get(); }

  BEGIN_MSG_MAP(ChromeFrameAutomationClient)
    CHAIN_MSG_MAP(
        TaskMarshallerThroughWindowsMessages<ChromeFrameAutomationClient>)
  END_MSG_MAP()

  void set_delegate(ChromeFrameDelegate* d) {
    chrome_frame_delegate_ = d;
  }

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
  void AttachExternalTab(intptr_t external_tab_cookie);

  void SetPageFontSize(enum AutomationPageFontSize);

 protected:
  // ChromeFrameAutomationProxy::LaunchDelegate implementation.
  virtual void LaunchComplete(ChromeFrameAutomationProxy* proxy,
                            AutomationLaunchResult result);
  // TabProxyDelegate implementation
  virtual void OnMessageReceived(TabProxy* tab, const IPC::Message& msg);

  void CreateExternalTab();
  void CreateExternalTabComplete(HWND chrome_window, HWND tab_window,
                                 int tab_handle);
  // Called in UI thread. Here we fire event to the client notifying for
  // the result of Initialize() method call.
  void InitializeComplete(AutomationLaunchResult result);

 private:
  // Usage: From bkgnd thread invoke:
  // CallDelegate(FROM_HERE, NewRunnableMethod(chrome_frame_delegate_,
  //                                           ChromeFrameDelegate::Something,
  //                                           param1,
  //                                           param2));
  void CallDelegate(const tracked_objects::Location& from_here,
                    Task* delegate_task);
  // The workhorse method called in main/GUI thread which is going to
  // execute ChromeFrameDelegate method encapsulated in delegate_task.
  void CallDelegateImpl(Task* delegate_task);

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
  GURL url_;
  GURL referrer_;

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
  intptr_t external_tab_cookie_;

  // Set to true if we received a navigation request prior to the automation
  // server being initialized.
  bool navigate_after_initialization_;

  ChromeFrameLaunchParams chrome_launch_params_;

  // When host network stack is used, this object is in charge of
  // handling network requests.
  PluginUrlRequestManager* url_fetcher_;
  bool thread_safe_url_fetcher_;

  bool ProcessUrlRequestMessage(TabProxy* tab, const IPC::Message& msg,
                                bool ui_thread);

  // PluginUrlRequestDelegate implementation. Simply adds tab's handle
  // as parameter and forwards to Chrome via IPC.
  virtual void OnResponseStarted(int request_id, const char* mime_type,
      const char* headers, int size, base::Time last_modified,
      const std::string& peristent_cookies, const std::string& redirect_url,
      int redirect_status);
  virtual void OnReadComplete(int request_id, const void* buffer, int len);
  virtual void OnResponseEnd(int request_id, const URLRequestStatus& status);

 public:
  void SetUrlFetcher(PluginUrlRequestManager* url_fetcher) {
    DCHECK(url_fetcher != NULL);
    url_fetcher_ = url_fetcher;
    thread_safe_url_fetcher_ = url_fetcher->IsThreadSafe();
    url_fetcher_->set_delegate(this);
  }
};

#endif  // CHROME_FRAME_CHROME_FRAME_AUTOMATION_H_
