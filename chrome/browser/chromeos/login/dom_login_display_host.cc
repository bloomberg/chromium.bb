// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/dom_login_display_host.h"

#include "chrome/browser/chromeos/login/dom_login_display.h"

namespace chromeos {

// DOMLoginDisplayHost -------------------------------------------------------

DOMLoginDisplayHost::DOMLoginDisplayHost(const gfx::Rect& background_bounds)
    : BaseLoginDisplayHost(background_bounds) {}

DOMLoginDisplayHost::~DOMLoginDisplayHost() {}

// LoginDisplayHost implementation -----------------------------------------

LoginDisplay* DOMLoginDisplayHost::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) const {
  DOMLoginDisplay* dom_login_display = DOMLoginDisplay::GetInstance();
  dom_login_display->set_delegate(delegate);
  dom_login_display->set_background_bounds(background_bounds());
  return dom_login_display;
}

gfx::NativeWindow DOMLoginDisplayHost::GetNativeWindow() const {
  return NULL;
}

void DOMLoginDisplayHost::SetOobeProgress(BackgroundView::LoginStep step) {}

void DOMLoginDisplayHost::SetOobeProgressBarVisible(bool visible) {}

void DOMLoginDisplayHost::SetShutdownButtonEnabled(bool enable) {}

void DOMLoginDisplayHost::SetStatusAreaEnabled(bool enable) {}

void DOMLoginDisplayHost::SetStatusAreaVisible(bool visible) {}

void DOMLoginDisplayHost::ShowBackground() {}

}  // namespace chromeos

