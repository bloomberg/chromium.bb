// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_EXTERNAL_TAB_H_
#define CHROME_FRAME_EXTERNAL_TAB_H_
#pragma once

#include <windows.h>
#include <atlbase.h>
#include <atlwin.h>
#include <vector>
#include <string>
#include "base/scoped_comptr_win.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/common//page_zoom.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome_frame/cfproxy.h"
#include "chrome_frame/task_marshaller.h"
#include "googleurl/src/gurl.h"

class Task;
class CancelableTask;
namespace base {
  class TimeDelta;
  class WaitableEvent;
}

namespace IPC {
  struct NavigationInfo;
  struct ContextMenuParams;
}

// This is the delegate/callback interface that has to be implemented
// by the customers of ExternalTabProxy class.
class UIDelegate {
 public:
  virtual void OnNavigationStateChanged(int flags,
      const IPC::NavigationInfo& nav_info) = 0;
  virtual void OnUpdateTargetUrl(const std::wstring& new_target_url) = 0;
  virtual void OnExtensionInstalled(const FilePath& path, void* user_data,
      AutomationMsg_ExtensionResponseValues response) = 0;
  virtual void OnLoad(const GURL& url) = 0;
  virtual void OnMessageFromChromeFrame(const std::string& message,
      const std::string& origin, const std::string& target) = 0;
  virtual void OnHandleContextMenu(HANDLE menu_handle, int align_flags,
      const IPC::ContextMenuParams& params) = 0;
  virtual void OnHandleAccelerator(const MSG& accel_message) = 0;
  virtual void OnTabbedOut(bool reverse) = 0;
  virtual void OnGoToHistoryOffset(int offset) = 0;
  virtual void OnOpenURL(const GURL& url_to_open, const GURL& referrer,
                       int open_disposition) = 0;
 protected:
  ~UIDelegate() {}
};

struct CreateTabParams {
  struct ProxyParams proxy_params;
  bool is_incognito;
  bool is_widget_mode;
  GURL url;
  GURL referrer;
};

/////////////////////////////////////////////////////////////////////////
//  ExternalTabProxy is a mediator between ChromeProxy (which runs mostly in
//  background IPC-channel thread and the UI object (ActiveX, NPAPI,
//  ActiveDocument).
//  The lifetime of ExternalTabProxy is determined by the UI object.
//
//  When ExternalTabProxy dies:
//  1. Remove itself as a ChromeProxyDelegate. This blocks until _Disconnected()
//     is received.
//  2. Kills all posted tasks to the UI thread.
//  3. Stop all network requests
//  => It does not have to (and should not) be a refcount-ed object.

// Non-public inheritance is not allowed by the style-guide.
class ExternalTabProxy : public CWindowImpl<ExternalTabProxy>,
                         public ChromeProxyDelegate {
 public:
  ExternalTabProxy();
  ~ExternalTabProxy();

#ifdef UNIT_TEST
  void set_proxy_factory(ChromeProxyFactory* factory) {
    proxy_factory_ = factory;
  }
#endif
  //
  virtual void CreateTab(const CreateTabParams& create_params,
                         UIDelegate* delegate);
  virtual void Navigate(const std::string& url, const std::string& referrer,
                        bool is_privileged);
  virtual void NavigateToIndex(int index);
  virtual void ForwardMessageFromExternalHost(const std::string& message,
      const std::string& origin, const std::string& target);
  virtual void ChromeFrameHostMoved();

  virtual void SetEnableExtensionAutomation(
      const std::vector<std::string>& functions_enabled);
  virtual void InstallExtension(const FilePath& crx_path, void* user_data);
  virtual void LoadExpandedExtension(const FilePath& path, void* user_data);
  virtual void GetEnabledExtensions(void* user_data);

  // Attaches an existing external tab to this automation client instance.
  virtual void ConnectToExternalTab(uint64 external_tab_cookie);
  virtual void BlockExternalTab(uint64 cookie);

  void SetZoomLevel(PageZoom::Function zoom_level);

 private:
  BEGIN_MSG_MAP(ExternalTabProxy)
    CHAIN_MSG_MAP_MEMBER(ui_);
  END_MSG_MAP()

  //////////////////////////////////////////////////////////////////////////
  // ChromeProxyDelegate implementation
  virtual int tab_handle() {
    return tab_;
  }
  virtual void Connected(ChromeProxy* proxy);
  virtual void PeerLost(ChromeProxy* proxy, DisconnectReason reason);
  virtual void Disconnected();


  // Sync message responses.
  virtual void Completed_CreateTab(bool success, HWND chrome_wnd,
      HWND tab_window, int tab_handle, int session_id);
  virtual void Completed_ConnectToTab(bool success, HWND chrome_window,
      HWND tab_window, int tab_handle, int session_id);
  virtual void Completed_Navigate(bool success,
      enum AutomationMsg_NavigationResponseValues res);
  virtual void Completed_InstallExtension(bool success,
      enum AutomationMsg_ExtensionResponseValues res, SyncMessageContext* ctx);
  virtual void Completed_LoadExpandedExtension(bool success,
      enum AutomationMsg_ExtensionResponseValues res, SyncMessageContext* ctx);
  virtual void Completed_GetEnabledExtensions(bool success,
      const std::vector<FilePath>* extensions);

  // Network requests from Chrome.
  virtual void Network_Start(int request_id,
      const IPC::AutomationURLRequest& request_info);
  virtual void Network_Read(int request_id, int bytes_to_read);
  virtual void Network_End(int request_id, const URLRequestStatus& s);
  virtual void Network_DownloadInHost(int request_id);
  virtual void GetCookies(const GURL& url, int cookie_id);
  virtual void SetCookie(const GURL& url, const std::string& cookie);

  // Navigation progress notifications.
  virtual void NavigationStateChanged(int flags,
      const IPC::NavigationInfo& nav_info);
  virtual void UpdateTargetUrl(const std::wstring& url);
  virtual void NavigationFailed(int error_code, const GURL& gurl);
  virtual void DidNavigate(const IPC::NavigationInfo& navigation_info);
  virtual void TabLoaded(const GURL& url);

  virtual void OpenURL(const GURL& url_to_open, const GURL& referrer,
                       int open_disposition);
  virtual void GoToHistoryOffset(int offset);
  virtual void MessageToHost(const std::string& message,
      const std::string& origin, const std::string& target);

  // Misc. UI.
  virtual void HandleAccelerator(const MSG& accel_message);
  virtual void HandleContextMenu(HANDLE menu_handle, int align_flags,
                                 const IPC::ContextMenuParams& params);
  virtual void TabbedOut(bool reverse);

  // Other
  virtual void TabClosed();
  virtual void AttachTab(const IPC::AttachExternalTabParams& attach_params);

  // end of ChromeProxyDelegate methods
  //////////////////////////////////////////////////////////////////////////
  void Init();
  void Destroy();

  // The UiXXXX are the ChromeProxyDelegate methods but on UI thread.
  void UiConnected(ChromeProxy* proxy);
  void UiPeerLost(ChromeProxy* proxy, DisconnectReason reason);
  void UiCompleted_CreateTab(bool success, HWND chrome_window,
                             HWND tab_window, int tab_handle, int session_id);

  // With the present state of affairs the only response we can possibly handle
  // in the background IPC thread is Completed_CreateTab() where we can
  // initiate a navigation (if there is a pending one).
  // To simplify - will handle Completed_CreateTab in UI thread and avoid
  // the need of lock when accessing members.
  enum {
    NONE,
    INIT_IN_PROGRESS,
    CREATE_TAB_IN_PROGRESS,
    READY,
    RELEASE_CF_PROXY_IN_PROGRESS
  } state_;
  int tab_;
  HWND tab_wnd_;
  HWND chrome_wnd_;
  ChromeProxyFactory* proxy_factory_;
  // Accessed only in the UI thread for simplicity.
  ChromeProxy* proxy_;
  // Accessed from ipc thread as well. It's safe if the object goes away
  // because this should be preceded by destruction of the window and
  // therefore all queued tasks should be destroyed.
  UIDelegate* ui_delegate_;
  TaskMarshallerThroughMessageQueue ui_;

  scoped_ptr<base::WaitableEvent> done_;

  CreateTabParams tab_params_;
  struct PendingNavigation {
    GURL url;
    GURL referrer;
    void Set(const GURL& gurl, const GURL& ref) {
      url = gurl;
      referrer = ref;
    }
  } pending_navigation_;

  ScopedComPtr<IInternetSecurityManager> security_manager_;
};

#endif  // CHROME_FRAME_EXTERNAL_TAB_H_
