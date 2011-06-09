// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_display_host.h"

#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/login/touch_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "views/widget/widget.h"

namespace chromeos {

namespace {

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "chrome://login";
// URL which corresponds to the OOBE WebUI.
const char kOobeURL[] = "chrome://oobe";

}  // namespace

// WebUILoginDisplayHost -------------------------------------------------------

WebUILoginDisplayHost::WebUILoginDisplayHost(const gfx::Rect& background_bounds)
    : BaseLoginDisplayHost(background_bounds),
      login_window_(NULL),
      login_view_(NULL) {
}

WebUILoginDisplayHost::~WebUILoginDisplayHost() {
  if (login_window_)
    login_window_->Close();
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

void WebUILoginDisplayHost::StartWizard(const std::string& first_screen_name,
                                        const GURL& start_url) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAllowWebUIOobe))
    LoadURL(GURL(kOobeURL));
  BaseLoginDisplayHost::StartWizard(first_screen_name, start_url);
}

void WebUILoginDisplayHost::StartSignInScreen() {
  LoadURL(GURL(kLoginURL));
  WebUILoginDisplay::GetInstance()->set_login_window(login_window_);
  BaseLoginDisplayHost::StartSignInScreen();
}

void WebUILoginDisplayHost::LoadURL(const GURL& url) {
  if (!login_window_) {
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = background_bounds();

    login_window_ = new views::Widget;
    login_window_->Init(params);
#if defined(TOUCH_UI)
    login_view_ = new TouchLoginView();
#else
    login_view_ = new WebUILoginView();
#endif

    login_view_->Init();
    login_window_->SetContentsView(login_view_);
    login_view_->UpdateWindowType();

    login_window_->Show();
  }
  login_view_->LoadURL(url);
}

WizardController* WebUILoginDisplayHost::CreateWizardController() {
  // TODO(altimofeev): ensure that WebUI is ready.
  OobeDisplay* oobe_display = static_cast<OobeUI*>(login_view_->GetWebUI());
  return new WizardController(this, oobe_display);
}

}  // namespace chromeos
