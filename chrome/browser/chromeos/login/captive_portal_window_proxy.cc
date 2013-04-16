// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"

#include "chrome/browser/chromeos/login/captive_portal_view.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/profiles/profile_manager.h"
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
}

CaptivePortalWindowProxy::~CaptivePortalWindowProxy() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_->Close();
  }
}

void CaptivePortalWindowProxy::ShowIfRedirected() {
  if (widget_) {
    // Invalid state as when widget is created (Show())
    // CaptivePortalView ownership is transferred to it.
    if (captive_portal_view_.get()) {
      NOTREACHED();
    }
    // Dialog is already shown, no need to reload.
    return;
  }

  // Dialog is not initialized yet.
  if (!captive_portal_view_.get()) {
    captive_portal_view_.reset(
        new CaptivePortalView(ProfileManager::GetDefaultProfile(), this));
  }

  // If dialog has been created (but not shown) previously, force reload.
  captive_portal_view_->StartLoad();
}

void CaptivePortalWindowProxy::Show() {
  if (ProxySettingsDialog::IsShown()) {
    // ProxySettingsDialog is being shown, don't cover it.
    Close();
    return;
  }

  if (!captive_portal_view_.get() || widget_) {
    // Dialog is already shown, do nothing.
    return;
  }

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
}

void CaptivePortalWindowProxy::Close() {
  if (widget_) {
    widget_->Close();
  } else {
    captive_portal_view_.reset();
  }
}

void CaptivePortalWindowProxy::OnRedirected() {
  Show();
  delegate_->OnPortalDetected();
}

void CaptivePortalWindowProxy::OnOriginalURLLoaded() {
  Close();
}

void CaptivePortalWindowProxy::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(widget == widget_);
  DCHECK(captive_portal_view_.get() == NULL);
  widget->RemoveObserver(this);
  widget_ = NULL;
}

}  // namespace chromeos
