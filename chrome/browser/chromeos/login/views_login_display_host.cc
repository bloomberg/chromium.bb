// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/views_login_display_host.h"

#include "chrome/browser/chromeos/login/views_login_display.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"

namespace chromeos {

// ViewsLoginDisplayHost -------------------------------------------------------

ViewsLoginDisplayHost::ViewsLoginDisplayHost(const gfx::Rect& background_bounds)
    : BaseLoginDisplayHost(background_bounds),
      background_view_(NULL),
      background_window_(NULL) {
}

ViewsLoginDisplayHost::~ViewsLoginDisplayHost() {
  if (background_window_)
    background_window_->Close();
}

// LoginDisplayHost implementation -----------------------------------------

LoginDisplay* ViewsLoginDisplayHost::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) const {
  chromeos::WizardAccessibilityHelper::GetInstance()->Init();
  return new ViewsLoginDisplay(delegate, background_bounds());
}

gfx::NativeWindow ViewsLoginDisplayHost::GetNativeWindow() const {
  if (background_view_)
    return background_view_->GetNativeWindow();
  else
    return NULL;
}

void ViewsLoginDisplayHost::SetOobeProgress(BackgroundView::LoginStep step) {
  if (background_view_)
    background_view_->SetOobeProgress(step);
}

void ViewsLoginDisplayHost::SetOobeProgressBarVisible(bool visible) {
  if (background_view_)
    background_view_->SetOobeProgressBarVisible(visible);
}

void ViewsLoginDisplayHost::SetShutdownButtonEnabled(bool enable) {
  if (background_view_)
    background_view_->EnableShutdownButton(enable);
}

void ViewsLoginDisplayHost::SetStatusAreaEnabled(bool enable) {
  if (background_view_)
    background_view_->SetStatusAreaEnabled(enable);
}

void ViewsLoginDisplayHost::SetStatusAreaVisible(bool visible) {
  if (background_view_)
    background_view_->SetStatusAreaVisible(visible);
}

void ViewsLoginDisplayHost::ShowBackground() {
  if (background_window_) {
    background_window_->Show();
    return;
  }
  background_window_ =
      BackgroundView::CreateWindowContainingView(background_bounds(),
                                                 GURL(),
                                                 &background_view_);
  background_window_->Show();
}

}  // namespace chromeos

