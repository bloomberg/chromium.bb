// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/webui_login_display_host.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/oobe_display.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// URL which corresponds to the login WebUI.
const char kLoginURL[] = "chrome://oobe/login";
// URL which corresponds to the OOBE WebUI.
const char kOobeURL[] = "chrome://oobe";

// Duration of sign-in transition animation.
const int kLoginFadeoutTransitionDurationMs = 700;

}  // namespace

// WebUILoginDisplayHost -------------------------------------------------------

WebUILoginDisplayHost::WebUILoginDisplayHost(const gfx::Rect& background_bounds)
    : BaseLoginDisplayHost(background_bounds),
      login_window_(NULL),
      login_view_(NULL),
      webui_login_display_(NULL),
      is_showing_login_(false) {
}

WebUILoginDisplayHost::~WebUILoginDisplayHost() {
  if (login_window_)
    login_window_->Close();
}

// LoginDisplayHost implementation ---------------------------------------------

LoginDisplay* WebUILoginDisplayHost::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) {
  webui_login_display_ = new WebUILoginDisplay(delegate);
  webui_login_display_->set_background_bounds(background_bounds());
  return webui_login_display_;
}

gfx::NativeWindow WebUILoginDisplayHost::GetNativeWindow() const {
  return login_window_ ? login_window_->GetNativeWindow() : NULL;
}

views::Widget* WebUILoginDisplayHost::GetWidget() const {
  return login_window_;
}

void WebUILoginDisplayHost::OpenProxySettings() {
  if (login_view_)
    login_view_->OpenProxySettings();
}

void WebUILoginDisplayHost::SetOobeProgressBarVisible(bool visible) {
  GetOobeUI()->ShowOobeUI(visible);
}

void WebUILoginDisplayHost::SetShutdownButtonEnabled(bool enable) {
}

void WebUILoginDisplayHost::SetStatusAreaVisible(bool visible) {
  if (login_view_)
    login_view_->SetStatusAreaVisible(visible);
}

void WebUILoginDisplayHost::StartWizard(const std::string& first_screen_name,
                                        DictionaryValue* screen_parameters) {
  is_showing_login_ = false;

  scoped_ptr<DictionaryValue> scoped_parameters(screen_parameters);
  // This is a special case for WebUI. We don't want to go through the
  // OOBE WebUI page loading. Since we already have the browser we just
  // show the corresponding page.
  if (first_screen_name == WizardController::kHTMLPageScreenName) {
    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
    const CommandLine::StringVector& args = cmd_line->GetArgs();
    std::string html_page_url;
    for (size_t i = 0; i < args.size(); i++) {
      // It's strange but |args| may contain empty strings.
      if (!args[i].empty()) {
        DCHECK(html_page_url.empty()) << "More than one URL in command line";
        html_page_url = args[i];
      }
    }
    DCHECK(!html_page_url.empty()) << "No URL in command line";
    DCHECK(!login_window_) << "Login window has already been created.";

    LoadURL(GURL(html_page_url));
    return;
  }

  if (!login_window_)
    LoadURL(GURL(kOobeURL));

  BaseLoginDisplayHost::StartWizard(first_screen_name,
                                    scoped_parameters.release());
}

void WebUILoginDisplayHost::StartSignInScreen() {
  is_showing_login_ = true;

  if (!login_window_)
    LoadURL(GURL(kLoginURL));

  BaseLoginDisplayHost::StartSignInScreen();
  CHECK(webui_login_display_);
  GetOobeUI()->ShowSigninScreen(webui_login_display_);
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    SetStatusAreaVisible(false);
}

void WebUILoginDisplayHost::OnPreferencesChanged() {
  if (is_showing_login_)
    webui_login_display_->OnPreferencesChanged();
}

void WebUILoginDisplayHost::OnBrowserCreated() {
  // Close lock window now so that the launched browser can receive focus.
  if (login_window_) {
    login_window_->Close();
    login_window_ = NULL;
    login_view_ = NULL;
  }
}

void WebUILoginDisplayHost::LoadURL(const GURL& url) {
  if (!login_window_) {
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = background_bounds();
    params.show_state = ui::SHOW_STATE_FULLSCREEN;
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableNewOobe))
      params.transparent = true;
    params.parent =
        ash::Shell::GetContainer(
            ash::Shell::GetPrimaryRootWindow(),
            ash::internal::kShellWindowId_LockScreenContainer);

    login_window_ = new views::Widget;
    login_window_->Init(params);
    login_view_ = new WebUILoginView();

    login_view_->Init(login_window_);

    ash::SetWindowVisibilityAnimationDuration(
        login_window_->GetNativeView(),
        base::TimeDelta::FromMilliseconds(kLoginFadeoutTransitionDurationMs));
    ash::SetWindowVisibilityAnimationTransition(
        login_window_->GetNativeView(),
        ash::ANIMATE_HIDE);

    login_window_->SetContentsView(login_view_);
    login_view_->UpdateWindowType();

    login_window_->Show();
    login_window_->GetNativeView()->SetName("WebUILoginView");
    login_view_->OnWindowCreated();
  }
  login_view_->LoadURL(url);
}

OobeUI* WebUILoginDisplayHost::GetOobeUI() const {
  return static_cast<OobeUI*>(login_view_->GetWebUI()->GetController());
}

WizardController* WebUILoginDisplayHost::CreateWizardController() {
  // TODO(altimofeev): ensure that WebUI is ready.
  OobeDisplay* oobe_display = GetOobeUI();
  return new WizardController(this, oobe_display);
}

}  // namespace chromeos
