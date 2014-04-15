// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"

#include "chrome/browser/chromeos/login/captive_portal_view.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/views/widget/widget.h"

using web_modal::WebContentsModalDialogManager;
using web_modal::WebContentsModalDialogManagerDelegate;

namespace chromeos {

CaptivePortalWindowProxy::CaptivePortalWindowProxy(
    Delegate* delegate,
    content::WebContents* web_contents)
    : delegate_(delegate),
      widget_(NULL),
      web_contents_(web_contents),
      captive_portal_view_for_testing_(NULL) {
  DCHECK(GetState() == STATE_IDLE);
}

CaptivePortalWindowProxy::~CaptivePortalWindowProxy() {
  if (!widget_)
    return;
  DCHECK(GetState() == STATE_DISPLAYED);
  widget_->RemoveObserver(this);
  widget_->Close();
}

void CaptivePortalWindowProxy::ShowIfRedirected() {
  if (GetState() != STATE_IDLE)
    return;
  InitCaptivePortalView();
  DCHECK(GetState() == STATE_WAITING_FOR_REDIRECTION);
}

void CaptivePortalWindowProxy::Show() {
  if (ProxySettingsDialog::IsShown()) {
    // ProxySettingsDialog is being shown, don't cover it.
    Close();
    return;
  }

  if (GetState() == STATE_DISPLAYED)  // Dialog is already shown, do nothing.
    return;

  InitCaptivePortalView();

  CaptivePortalView* captive_portal_view = captive_portal_view_.release();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents_);
  DCHECK(web_contents_modal_dialog_manager);
  WebContentsModalDialogManagerDelegate* delegate =
      web_contents_modal_dialog_manager->delegate();
  DCHECK(delegate);
  widget_ = views::Widget::CreateWindowAsFramelessChild(
      captive_portal_view,
      delegate->GetWebContentsModalDialogHost()->GetHostView());
  captive_portal_view->Init();

  widget_->AddObserver(this);
  web_contents_modal_dialog_manager->ShowModalDialog(
      widget_->GetNativeView());
  DCHECK(GetState() == STATE_DISPLAYED);
}

void CaptivePortalWindowProxy::Close() {
  if (GetState() == STATE_DISPLAYED)
    widget_->Close();
  captive_portal_view_.reset();
  captive_portal_view_for_testing_ = NULL;
}

void CaptivePortalWindowProxy::OnRedirected() {
  if (GetState() == STATE_WAITING_FOR_REDIRECTION)
    Show();
  delegate_->OnPortalDetected();
}

void CaptivePortalWindowProxy::OnOriginalURLLoaded() {
  Close();
}

void CaptivePortalWindowProxy::OnWidgetClosing(views::Widget* widget) {
  DCHECK(GetState() == STATE_DISPLAYED);
  DCHECK(widget == widget_);

  DetachFromWidget(widget);

  DCHECK(GetState() == STATE_IDLE);
}

void CaptivePortalWindowProxy::OnWidgetDestroying(views::Widget* widget) {
  DetachFromWidget(widget);
}

void CaptivePortalWindowProxy::OnWidgetDestroyed(views::Widget* widget) {
  DetachFromWidget(widget);
}

void CaptivePortalWindowProxy::InitCaptivePortalView() {
  DCHECK(GetState() == STATE_IDLE ||
         GetState() == STATE_WAITING_FOR_REDIRECTION);
  if (!captive_portal_view_.get()) {
    captive_portal_view_.reset(
        new CaptivePortalView(ProfileHelper::GetSigninProfile(), this));
    captive_portal_view_for_testing_ = captive_portal_view_.get();
  }
  captive_portal_view_->StartLoad();
}

CaptivePortalWindowProxy::State CaptivePortalWindowProxy::GetState() const {
  if (widget_ == NULL) {
    if (captive_portal_view_.get() == NULL)
      return STATE_IDLE;
    else
      return STATE_WAITING_FOR_REDIRECTION;
  } else {
    if (captive_portal_view_.get() == NULL)
      return STATE_DISPLAYED;
    else
      NOTREACHED();
  }
  return STATE_UNKNOWN;
}

void CaptivePortalWindowProxy::DetachFromWidget(views::Widget* widget) {
  if (!widget_ || widget_ != widget)
    return;
  widget_->RemoveObserver(this);
  widget_ = NULL;
}

}  // namespace chromeos
