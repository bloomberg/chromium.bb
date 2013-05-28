// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"

#include "chrome/browser/chromeos/login/captive_portal_view.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "ui/views/widget/widget.h"

namespace {

int kMargin = 50;

}  // namespace

namespace chromeos {

CaptivePortalWindowProxy::CaptivePortalWindowProxy(Delegate* delegate,
                                                   gfx::NativeWindow parent)
    : delegate_(delegate),
      widget_(NULL),
      parent_(parent) {
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
  widget_ = views::Widget::CreateWindowWithParent(
      captive_portal_view,
      parent_);
  captive_portal_view->Init();

  gfx::Rect bounds(CalculateScreenBounds(gfx::Size()));
  bounds.Inset(kMargin, kMargin);
  widget_->SetBounds(bounds);

  widget_->AddObserver(this);
  widget_->Show();
  DCHECK(GetState() == STATE_DISPLAYED);
}

void CaptivePortalWindowProxy::Close() {
  if (GetState() == STATE_DISPLAYED)
    widget_->Close();
  captive_portal_view_.reset();
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

  widget->RemoveObserver(this);
  widget_ = NULL;

  DCHECK(GetState() == STATE_IDLE);
}

void CaptivePortalWindowProxy::InitCaptivePortalView() {
  DCHECK(GetState() == STATE_IDLE ||
         GetState() == STATE_WAITING_FOR_REDIRECTION);
  if (!captive_portal_view_.get()) {
    captive_portal_view_.reset(
        new CaptivePortalView(ProfileHelper::GetSigninProfile(), this));
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

}  // namespace chromeos
