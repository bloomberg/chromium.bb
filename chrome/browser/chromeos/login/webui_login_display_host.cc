// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_display_host.h"

#include "chrome/browser/chromeos/login/webui_login_display.h"

namespace chromeos {

// WebUILoginDisplayHost -------------------------------------------------------

WebUILoginDisplayHost::WebUILoginDisplayHost(const gfx::Rect& background_bounds)
    : BaseLoginDisplayHost(background_bounds) {
}

WebUILoginDisplayHost::~WebUILoginDisplayHost() {
}

// LoginDisplayHost implementation ---------------------------------------------

LoginDisplay* WebUILoginDisplayHost::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) const {
  WebUILoginDisplay* webui_login_display = WebUILoginDisplay::GetInstance();
  webui_login_display->set_delegate(delegate);
  webui_login_display->set_background_bounds(background_bounds());
  return webui_login_display;
}

gfx::NativeWindow WebUILoginDisplayHost::GetNativeWindow() const {
  return NULL;
}

void WebUILoginDisplayHost::SetOobeProgress(BackgroundView::LoginStep step) {
}

void WebUILoginDisplayHost::SetOobeProgressBarVisible(bool visible) {
}

void WebUILoginDisplayHost::SetShutdownButtonEnabled(bool enable) {
}

void WebUILoginDisplayHost::SetStatusAreaEnabled(bool enable) {
}

void WebUILoginDisplayHost::SetStatusAreaVisible(bool visible) {
}

void WebUILoginDisplayHost::ShowBackground() {
}

}  // namespace chromeos

