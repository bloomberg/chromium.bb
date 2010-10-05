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

ExternalTabProxy::ExternalTabProxy() : state_(NONE), tab_(0), proxy_(NULL),
    ui_delegate_(NULL) {
}

ExternalTabProxy::~ExternalTabProxy() {
  Destroy();
}

void ExternalTabProxy::Destroy() {
  DCHECK(NULL == done_.get());
  done_.reset(new base::WaitableEvent(true, false));
  proxy_factory_->ReleaseProxy(this, tab_params_.profile);
  done_->Wait();
  done_.reset(NULL);

  proxy_ = NULL;
  tab_ = 0;
}

void ExternalTabProxy::CreateTab(const CreateTabParams& create_params,
                                 UIDelegate* delegate) {
  DCHECK(ui_delegate_ != NULL);
  DCHECK_EQ(NONE, state_);
  ui_delegate_ = delegate;
  tab_params_ = create_params;
  state_ = INIT_IN_PROGRESS;
  // TODO(stoyan): initialize ProxyParams from CreateTabParams.
  ProxyParams p;
  proxy_factory_->GetProxy(this, p);
}

void ExternalTabProxy::Connected(ChromeProxy* proxy) {
  // in ipc thread
  ui_.PostTask(FROM_HERE, NewRunnableMethod(this,
      &ExternalTabProxy::UiConnected, proxy));
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

void ExternalTabProxy::Navigate(const std::string& url,
    const std::string& referrer, bool is_privileged) {
  // in ui thread
  // Catch invalid URLs early. Can we allow this navigation to happen?
  GURL parsed_url(url);
  if (!CanNavigate(parsed_url, security_manager_, is_privileged)) {
    DLOG(ERROR) << __FUNCTION__ << " Not allowing navigation to: " << url;
    return;
  }

  if (state_ == INIT_IN_PROGRESS) {
    // TODO(stoyan): replace CreateTabParams with the new ones
  }

  if (state_ == CREATE_TAB_IN_PROGRESS) {
    // ah! too late. wait to get tab handle and then navigate
    pending_navigation_.Set(parsed_url, GURL(referrer));
  }

  if (state_ == READY) {
    proxy_->Tab_Navigate(tab_, parsed_url, GURL(referrer));
  }
}

void ExternalTabProxy::Completed_CreateTab(bool success, HWND chrome_wnd,
                                           HWND tab_window, int tab_handle) {
  // in ipc_thread.
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
