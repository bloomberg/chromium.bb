// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/external_tab.h"
#include "base/tracked.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome_frame/utils.h"

DISABLE_RUNNABLE_METHOD_REFCOUNT(ExternalTabProxy);
DISABLE_RUNNABLE_METHOD_REFCOUNT(UIDelegate);

namespace {
  struct UserDataHolder : public SyncMessageContext {
    explicit UserDataHolder(void* p) : data(p) {}
    void* data;
  };
}


ExternalTabProxy::ExternalTabProxy() : state_(NONE), tab_(0), proxy_(NULL),
    ui_delegate_(NULL) {
}

ExternalTabProxy::~ExternalTabProxy() {
  Destroy();
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
  done_.reset(new base::WaitableEvent(true, false));
  proxy_factory_->ReleaseProxy(this, tab_params_.proxy_params.profile);
  done_->Wait();
  done_.reset(NULL);

  proxy_ = NULL;
  tab_ = 0;
}

void ExternalTabProxy::CreateTab(const CreateTabParams& create_params,
                                 UIDelegate* delegate) {
  DCHECK(ui_delegate_ != NULL);
  DCHECK_EQ(NONE, state_);
  // Create host window if needed.
  Init();
  ui_delegate_ = delegate;
  tab_params_ = create_params;
  state_ = INIT_IN_PROGRESS;
  proxy_factory_->GetProxy(this, create_params.proxy_params);
}

void ExternalTabProxy::Connected(ChromeProxy* proxy) {
  // in ipc thread
  ui_.PostTask(FROM_HERE, NewRunnableMethod(this,
      &ExternalTabProxy::UiConnected, proxy));
}

void ExternalTabProxy::UiConnected(ChromeProxy* proxy) {
  proxy_ = proxy;
  IPC::ExternalTabSettings settings;
  // TODO(stoyan): Initialize settings.
  proxy->CreateTab(this, settings);
}

void ExternalTabProxy::Disconnected() {
  // in ipc thread
  DCHECK(done_.get() != NULL);
  done_->Signal();
}

void ExternalTabProxy::PeerLost(ChromeProxy* proxy, DisconnectReason reason) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(this, &ExternalTabProxy::UiPeerLost,
      proxy, reason));
}

void ExternalTabProxy::UiPeerLost(ChromeProxy* proxy, DisconnectReason reason) {
  // TODO(stoyan):
}

void ExternalTabProxy::Navigate(const std::string& url,
    const std::string& referrer, bool is_privileged) {
  // in ui thread
  // Catch invalid URLs early. Can we allow this navigation to happen?
  GURL parsed_url(url);
  if (!CanNavigate(parsed_url, security_manager_, is_privileged)) {
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

void ExternalTabProxy::SetZoomLevel(PageZoom::Function zoom_level) {
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

void ExternalTabProxy::SetEnableExtensionAutomation(
    const std::vector<std::string>& functions_enabled) {
  proxy_->Tab_SetEnableExtensionAutomation(tab_, functions_enabled);
}

void ExternalTabProxy::InstallExtension(const FilePath& crx_path,
                                        void* user_data) {
  proxy_->InstallExtension(this, crx_path, new UserDataHolder(user_data));
}

void ExternalTabProxy::LoadExpandedExtension(const FilePath& path,
                                             void* user_data) {
  proxy_->LoadExtension(this, path, new UserDataHolder(user_data));
}

void ExternalTabProxy::GetEnabledExtensions(void* user_data) {
  proxy_->GetEnabledExtensions(this, new UserDataHolder(user_data));
}

void ExternalTabProxy::ChromeFrameHostMoved() {
  proxy_->Tab_OnHostMoved(tab_);
}

//////////////////////////////////////////////////////////////////////////
void ExternalTabProxy::Completed_CreateTab(bool success, HWND chrome_wnd,
                                           HWND tab_window, int tab_handle) {
  // in ipc_thread.
  // ui_.PostTask()
}

void ExternalTabProxy::Completed_ConnectToTab(bool success,
    HWND chrome_window, HWND tab_window, int tab_handle) {
  CHECK(0);
}

void ExternalTabProxy::Completed_Navigate(bool success,
    enum AutomationMsg_NavigationResponseValues res) {
  // ipc_thread;
  CHECK(0);
}

void ExternalTabProxy::Completed_InstallExtension(bool success,
    enum AutomationMsg_ExtensionResponseValues res, SyncMessageContext* ctx) {
  CHECK(0);
}

void ExternalTabProxy::Completed_LoadExpandedExtension(bool success,
    enum AutomationMsg_ExtensionResponseValues res, SyncMessageContext* ctx) {
  CHECK(0);
}

void ExternalTabProxy::Completed_GetEnabledExtensions(bool success,
    const std::vector<FilePath>* extensions) {
  CHECK(0);
}

void ExternalTabProxy::NavigationStateChanged(int flags,
    const IPC::NavigationInfo& nav_info) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnNavigationStateChanged, flags, nav_info));
}

void ExternalTabProxy::UpdateTargetUrl(const std::wstring& url) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnUpdateTargetUrl, url));
}

void ExternalTabProxy::TabLoaded(const GURL& url) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnLoad, url));
}

void ExternalTabProxy::MessageToHost(const std::string& message,
                                     const std::string& origin,
                                     const std::string& target) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnMessageFromChromeFrame, message, origin, target));
}

void ExternalTabProxy::HandleAccelerator(const MSG& accel_message) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnHandleAccelerator, accel_message));
}

void ExternalTabProxy::HandleContextMenu(HANDLE menu_handle, int align_flags,
                                         const IPC::ContextMenuParams& params) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnHandleContextMenu, menu_handle, align_flags, params));
}

void ExternalTabProxy::TabbedOut(bool reverse) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnTabbedOut, reverse));
}

void ExternalTabProxy::GoToHistoryOffset(int offset) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnGoToHistoryOffset, offset));
}

void ExternalTabProxy::OpenURL(const GURL& url_to_open, const GURL& referrer,
                               int open_disposition) {
  ui_.PostTask(FROM_HERE, NewRunnableMethod(ui_delegate_,
      &UIDelegate::OnOpenURL, url_to_open, referrer, open_disposition));
}

void ExternalTabProxy::NavigationFailed(int error_code, const GURL& gurl) {
  // TODO(stoyan):
}

void ExternalTabProxy::DidNavigate(const IPC::NavigationInfo& navigation_info) {
  // TODO(stoyan):
}

void ExternalTabProxy::Network_Start(
    int request_id, const IPC::AutomationURLRequest& request_info) {
  // TODO(stoyan): url_fetcher_.Start();
}

void ExternalTabProxy::Network_Read(int request_id, int bytes_to_read) {
  // TODO(stoyan): url_fetcher_.Read();
}

void ExternalTabProxy::Network_End(int request_id, const URLRequestStatus& s) {
  // TODO(stoyan):
}

void ExternalTabProxy::Network_DownloadInHost(int request_id) {
  // TODO(stoyan):
}

void ExternalTabProxy::GetCookies(const GURL& url, int cookie_id) {
  // TODO(stoyan):
}

void ExternalTabProxy::SetCookie(const GURL& url, const std::string& cookie) {
  // TODO(stoyan):
}

void ExternalTabProxy::TabClosed() {
  // TODO(stoyan):
}

void ExternalTabProxy::AttachTab(
    const IPC::AttachExternalTabParams& attach_params) {
  // TODO(stoyan):
}
