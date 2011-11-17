// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "chrome_frame/external_tab.h"
#include "base/task.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/common/automation_messages.h"
#include "chrome_frame/chrome_frame_delegate.h"
#include "chrome_frame/utils.h"

namespace {
  static base::LazyInstance<ChromeProxyFactory> g_proxy_factory =
      LAZY_INSTANCE_INITIALIZER;

  struct UserDataHolder : public SyncMessageContext {
    explicit UserDataHolder(void* p) : data(p) {}
    void* data;
  };
}


ExternalTabProxy::ExternalTabProxy() : state_(NONE), tab_(0), tab_wnd_(NULL),
    chrome_wnd_(NULL), proxy_factory_(g_proxy_factory.Pointer()), proxy_(NULL),
    ui_delegate_(NULL) {
}

ExternalTabProxy::~ExternalTabProxy() {
  Destroy();
}

bool ExternalTabProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExternalTabProxy, message)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationStateChanged,
                        OnNavigationStateChanged)
    IPC_MESSAGE_HANDLER(AutomationMsg_UpdateTargetUrl, OnUpdateTargetUrl)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleAccelerator, OnHandleAccelerator)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabbedOut, OnTabbedOut)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationFailed, OnNavigationFailed)
    IPC_MESSAGE_HANDLER(AutomationMsg_DidNavigate, OnDidNavigate)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabLoaded, OnTabLoaded)
    IPC_MESSAGE_HANDLER(AutomationMsg_MoveWindow, OnMoveWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardMessageToExternalHost,
                        OnMessageToHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardContextMenuToExternalHost,
                        OnHandleContextMenu)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStart, OnNetwork_Start)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestRead, OnNetwork_Read)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnNetwork_End)
    IPC_MESSAGE_HANDLER(AutomationMsg_DownloadRequestInHost,
                        OnNetwork_DownloadInHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookiesFromHost, OnGetCookies)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookieAsync, OnSetCookie)
    IPC_MESSAGE_HANDLER(AutomationMsg_AttachExternalTab, OnAttachTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestGoToHistoryEntryOffset,
                        OnGoToHistoryOffset)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseExternalTab, OnTabClosed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExternalTabProxy::Init() {
  if (m_hWnd == NULL) {
    // Create a window on the UI thread for marshaling messages back and forth
    // from the IPC thread. This window cannot be a message only window as the
    // external chrome tab window initially is created as a child of this window
    CWindowImpl<ExternalTabProxy>::Create(GetDesktopWindow(), NULL, NULL,
        WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_TOOLWINDOW);
    DCHECK(m_hWnd != NULL);
    ui_.SetWindow(m_hWnd, WM_APP + 6);
  }
}

void ExternalTabProxy::Destroy() {
  DCHECK(NULL == done_.get());
  // TODO(stoyan): Should we release proxy first and then destroy the window
  // (parent of the chrome window) or the other way around?
  if (state_ != NONE) {
    done_.reset(new base::WaitableEvent(true, false));
    proxy_factory_->ReleaseProxy(this, tab_params_.proxy_params.profile);
    done_->Wait();
    done_.reset(NULL);

    state_ = NONE;
    proxy_ = NULL;
    tab_ = 0;
    CWindowImpl<ExternalTabProxy>::DestroyWindow();
    tab_wnd_ = NULL;
    chrome_wnd_ = NULL;
    // We shall tell the TaskMarshaller to delete queued tasks.
    // ui_.DeleteAll();
  }
}

void ExternalTabProxy::CreateTab(const CreateTabParams& create_params,
                                 UIDelegate* delegate) {
  DCHECK(ui_delegate_ == NULL);
  DCHECK_EQ(NONE, state_);
  // Create host window if needed.
  Init();
  ui_delegate_ = delegate;
  // TODO(stoyan): Shall we check the CanNavigate(create_params.url)?
  tab_params_ = create_params;
  state_ = INIT_IN_PROGRESS;
  proxy_factory_->GetProxy(this, create_params.proxy_params);
}

void ExternalTabProxy::Connected(ChromeProxy* proxy) {
  // in ipc thread
  ui_.PostTask(FROM_HERE, base::Bind(&ExternalTabProxy::UiConnected,
                                     base::Unretained(this), proxy));
}

void ExternalTabProxy::UiConnected(ChromeProxy* proxy) {
  proxy_ = proxy;
  ExternalTabSettings settings;
  settings.parent = m_hWnd;
  settings.style = WS_CHILD;
  settings.is_incognito = tab_params_.is_incognito;
  // TODO(stoyan): FIX this.
  settings.load_requests_via_automation = true;
  // TODO(stoyan): FIX this.
  settings.handle_top_level_requests = true;
  settings.initial_url = tab_params_.url;
  settings.referrer = tab_params_.referrer;
  // Infobars are disabled in widget mode.
  settings.infobars_enabled = !tab_params_.is_widget_mode;
  // TODO(stoyan): FIX this.
  settings.route_all_top_level_navigations = false;

  state_ = CREATE_TAB_IN_PROGRESS;
  proxy->CreateTab(this, settings);
}

void ExternalTabProxy::Disconnected() {
  // in ipc thread
  DCHECK(done_.get() != NULL);
  done_->Signal();
}

void ExternalTabProxy::PeerLost(ChromeProxy* proxy, DisconnectReason reason) {
  ui_.PostTask(FROM_HERE, base::Bind(&ExternalTabProxy::UiPeerLost,
                                     base::Unretained(this), proxy, reason));
}

void ExternalTabProxy::UiPeerLost(ChromeProxy* proxy, DisconnectReason reason) {
  // TODO(stoyan):
}

void ExternalTabProxy::Navigate(const std::string& url,
    const std::string& referrer,
    NavigationConstraints* navigation_constraints) {
  // in ui thread
  // Catch invalid URLs early. Can we allow this navigation to happen?
  GURL parsed_url(url);
  if (!CanNavigate(parsed_url, navigation_constraints)) {
    DLOG(ERROR) << __FUNCTION__ << " Not allowing navigation to: " << url;
    return;
  }

  GURL parsed_referrer(referrer);
  // If we are still establishing channel, simply replace the params
  if (state_ == INIT_IN_PROGRESS) {
    tab_params_.url = parsed_url;
    tab_params_.referrer = parsed_referrer;
  }

  // Ah! Too late. Wait to get tab handle and then navigate.
  if (state_ == CREATE_TAB_IN_PROGRESS) {
    pending_navigation_.Set(parsed_url, parsed_referrer);
  }

  if (state_ == READY) {
    proxy_->Tab_Navigate(tab_, parsed_url, parsed_referrer);
  }
}

void ExternalTabProxy::ConnectToExternalTab(uint64 external_tab_cookie) {
  proxy_->ConnectTab(this, m_hWnd, external_tab_cookie);
}

void ExternalTabProxy::BlockExternalTab(uint64 cookie) {
  proxy_->BlockTab(cookie);
}

void ExternalTabProxy::SetZoomLevel(content::PageZoom zoom_level) {
  proxy_->Tab_Zoom(tab_, zoom_level);
}

void ExternalTabProxy::NavigateToIndex(int index) {
  CHECK(0);
}

void ExternalTabProxy::ForwardMessageFromExternalHost(
    const std::string& message, const std::string& origin,
    const std::string& target) {
  proxy_->Tab_PostMessage(tab_, message, origin, target);
}

void ExternalTabProxy::ChromeFrameHostMoved() {
  proxy_->Tab_OnHostMoved(tab_);
}

//////////////////////////////////////////////////////////////////////////
void ExternalTabProxy::UiCompleted_CreateTab(bool success, HWND chrome_window,
                                             HWND tab_window, int tab_handle,
                                             int session_id) {
  if (success) {
    state_ = READY;
    tab_ = tab_handle;
    tab_wnd_ = tab_window;
    chrome_wnd_ = chrome_window;

    // If a navigation request came while tab creation was in progress -
    // go ahead and navigate.
    if (pending_navigation_.url.is_valid())
      proxy_->Tab_Navigate(tab_, pending_navigation_.url,
                           pending_navigation_.referrer);
  }
}

void ExternalTabProxy::Completed_CreateTab(bool success, HWND chrome_wnd,
                                           HWND tab_window, int tab_handle,
                                           int session_id) {
  // in ipc_thread.
  ui_.PostTask(
      FROM_HERE, base::Bind(&ExternalTabProxy::UiCompleted_CreateTab,
                            base::Unretained(this), success, chrome_wnd,
                            tab_window, tab_handle, session_id));
}

void ExternalTabProxy::Completed_ConnectToTab(
    bool success, HWND chrome_window, HWND tab_window, int tab_handle,
    int session_id) {
  CHECK(0);
}

void ExternalTabProxy::Completed_Navigate(
    bool success, enum AutomationMsg_NavigationResponseValues res) {
  // ipc_thread;
  CHECK(0);
}

void ExternalTabProxy::OnNavigationStateChanged(
    int flags, const NavigationInfo& nav_info) {
  ui_.PostTask(FROM_HERE,
               base::Bind(&UIDelegate::OnNavigationStateChanged,
                          base::Unretained(ui_delegate_), flags, nav_info));
}

void ExternalTabProxy::OnUpdateTargetUrl(const std::wstring& url) {
  ui_.PostTask(FROM_HERE, base::Bind(&UIDelegate::OnUpdateTargetUrl,
                                     base::Unretained(ui_delegate_), url));
}

void ExternalTabProxy::OnTabLoaded(const GURL& url) {
  ui_.PostTask(FROM_HERE, base::Bind(&UIDelegate::OnLoad,
                                     base::Unretained(ui_delegate_), url));
}

void ExternalTabProxy::OnMoveWindow(const gfx::Rect& pos) {
  ui_.PostTask(FROM_HERE, base::Bind(&UIDelegate::OnMoveWindow,
                                     base::Unretained(ui_delegate_), pos));
}

void ExternalTabProxy::OnMessageToHost(const std::string& message,
                                       const std::string& origin,
                                       const std::string& target) {
  ui_.PostTask(
      FROM_HERE,
      base::Bind(&UIDelegate::OnMessageFromChromeFrame,
                 base::Unretained(ui_delegate_), message, origin, target));
}

void ExternalTabProxy::OnHandleAccelerator(const MSG& accel_message) {
  ui_.PostTask(FROM_HERE,
               base::Bind(&UIDelegate::OnHandleAccelerator,
                          base::Unretained(ui_delegate_), accel_message));
}

void ExternalTabProxy::OnHandleContextMenu(
    const ContextMenuModel& context_menu_model,
    int align_flags,
    const MiniContextMenuParams& params) {
  ui_.PostTask(FROM_HERE,
               base::Bind(&UIDelegate::OnHandleContextMenu,
                          base::Unretained(ui_delegate_), context_menu_model,
                          align_flags, params));
}

void ExternalTabProxy::OnTabbedOut(bool reverse) {
  ui_.PostTask(FROM_HERE, base::Bind(&UIDelegate::OnTabbedOut,
                                     base::Unretained(ui_delegate_), reverse));
}

void ExternalTabProxy::OnGoToHistoryOffset(int offset) {
  ui_.PostTask(FROM_HERE, base::Bind(&UIDelegate::OnGoToHistoryOffset,
                                     base::Unretained(ui_delegate_), offset));
}

void ExternalTabProxy::OnOpenURL(const GURL& url_to_open, const GURL& referrer,
                                 int open_disposition) {
  ui_.PostTask(
      FROM_HERE,
      base::Bind(&UIDelegate::OnOpenURL, base::Unretained(ui_delegate_),
                 url_to_open, referrer, open_disposition));
}

void ExternalTabProxy::OnNavigationFailed(int error_code, const GURL& gurl) {
  // TODO(stoyan):
}

void ExternalTabProxy::OnDidNavigate(const NavigationInfo& navigation_info) {
  // TODO(stoyan):
}

void ExternalTabProxy::OnNetwork_Start(
    int request_id, const AutomationURLRequest& request_info) {
  // TODO(stoyan): url_fetcher_.Start();
}

void ExternalTabProxy::OnNetwork_Read(int request_id, int bytes_to_read) {
  // TODO(stoyan): url_fetcher_.Read();
}

void ExternalTabProxy::OnNetwork_End(int request_id,
                                     const net::URLRequestStatus& s) {
  // TODO(stoyan):
}

void ExternalTabProxy::OnNetwork_DownloadInHost(int request_id) {
  // TODO(stoyan):
}

void ExternalTabProxy::OnGetCookies(const GURL& url, int cookie_id) {
  // TODO(stoyan):
}

void ExternalTabProxy::OnSetCookie(const GURL& url, const std::string& cookie) {
  // TODO(stoyan):
}

void ExternalTabProxy::OnTabClosed() {
  // TODO(stoyan):
}

void ExternalTabProxy::OnAttachTab(
    const AttachExternalTabParams& attach_params) {
  // TODO(stoyan):
}
