// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_EXTERNAL_TAB_H_
#define CHROME_FRAME_EXTERNAL_TAB_H_
#pragma once

#include <windows.h>
#include <atlbase.h>
#include <atlwin.h>

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/win/scoped_comptr.h"
#include "chrome/common/automation_constants.h"
#include "chrome_frame/cfproxy.h"
#include "chrome_frame/task_marshaller.h"
#include "content/public/common/page_zoom.h"
#include "googleurl/src/gurl.h"

struct AttachExternalTabParams;
struct AutomationURLRequest;
struct ContextMenuModel;
struct MiniContextMenuParams;
struct NavigationInfo;
class Task;

namespace base {
class WaitableEvent;
}

namespace gfx {
class Rect;
}

namespace net {
class URLRequestStatus;
}

// This is the delegate/callback interface that has to be implemented
// by the customers of ExternalTabProxy class.
class UIDelegate {
 public:
  virtual void OnNavigationStateChanged(
      int flags, const NavigationInfo& nav_info) = 0;
  virtual void OnUpdateTargetUrl(const std::wstring& new_target_url) = 0;
  virtual void OnLoad(const GURL& url) = 0;
  virtual void OnMoveWindow(const gfx::Rect& pos) = 0;

  virtual void OnMessageFromChromeFrame(
      const std::string& message, const std::string& origin,
      const std::string& target) = 0;
  virtual void OnHandleContextMenu(
      const ContextMenuModel& context_menu_model, int align_flags,
      const MiniContextMenuParams& params) = 0;
  virtual void OnHandleAccelerator(const MSG& accel_message) = 0;
  virtual void OnTabbedOut(bool reverse) = 0;
  virtual void OnGoToHistoryOffset(int offset) = 0;
  virtual void OnOpenURL(
      const GURL& url_to_open, const GURL& referrer, int open_disposition) = 0;
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

class NavigationConstraints;

/////////////////////////////////////////////////////////////////////////
//  ExternalTabProxy is a mediator between ChromeProxy (which runs mostly in the
//  background IPC-channel thread) and the UI object (ActiveX, ActiveDocument).
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

  // IPC::Channel::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message);

  //
  virtual void CreateTab(const CreateTabParams& create_params,
                         UIDelegate* delegate);
  virtual void Navigate(const std::string& url, const std::string& referrer,
                        NavigationConstraints* navigation_constraints);
  virtual void NavigateToIndex(int index);
  virtual void ForwardMessageFromExternalHost(const std::string& message,
      const std::string& origin, const std::string& target);
  virtual void ChromeFrameHostMoved();

  // Attaches an existing external tab to this automation client instance.
  virtual void ConnectToExternalTab(uint64 external_tab_cookie);
  virtual void BlockExternalTab(uint64 cookie);

  void SetZoomLevel(content::PageZoom zoom_level);

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

  // Network requests from Chrome.
  virtual void OnNetwork_Start(
      int request_id, const AutomationURLRequest& request_info);
  virtual void OnNetwork_Read(int request_id, int bytes_to_read);
  virtual void OnNetwork_End(int request_id, const net::URLRequestStatus& s);
  virtual void OnNetwork_DownloadInHost(int request_id);
  virtual void OnGetCookies(const GURL& url, int cookie_id);
  virtual void OnSetCookie(const GURL& url, const std::string& cookie);

  // Navigation progress notifications.
  virtual void OnNavigationStateChanged(
      int flags, const NavigationInfo& nav_info);
  virtual void OnUpdateTargetUrl(const std::wstring& url);
  virtual void OnNavigationFailed(int error_code, const GURL& gurl);
  virtual void OnDidNavigate(const NavigationInfo& navigation_info);
  virtual void OnTabLoaded(const GURL& url);
  virtual void OnMoveWindow(const gfx::Rect& pos);

  virtual void OnOpenURL(const GURL& url_to_open, const GURL& referrer,
                         int open_disposition);
  virtual void OnGoToHistoryOffset(int offset);
  virtual void OnMessageToHost(
      const std::string& message, const std::string& origin,
      const std::string& target);

  // Misc. UI.
  virtual void OnHandleAccelerator(const MSG& accel_message);
  virtual void OnHandleContextMenu(const ContextMenuModel& context_menu_model,
                                   int align_flags,
                                   const MiniContextMenuParams& params);
  virtual void OnTabbedOut(bool reverse);

  // Other
  virtual void OnTabClosed();
  virtual void OnAttachTab(const AttachExternalTabParams& attach_params);

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
};

#endif  // CHROME_FRAME_EXTERNAL_TAB_H_
