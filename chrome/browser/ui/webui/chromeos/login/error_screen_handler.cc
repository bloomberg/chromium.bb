// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/webui_login_display_host.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

void EnableLazyDetection() {
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (NetworkPortalDetector::IsEnabled() && detector)
    detector->EnableLazyDetection();
}

void DisableLazyDetection() {
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (NetworkPortalDetector::IsEnabled() && detector)
    detector->DisableLazyDetection();
}

}  // namespace

ErrorScreenHandler::ErrorScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : network_state_informer_(network_state_informer),
      show_on_init_(false) {
  DCHECK(network_state_informer_.get());
}

ErrorScreenHandler::~ErrorScreenHandler() {
}

void ErrorScreenHandler::Show(OobeDisplay::Screen parent_screen,
                              base::DictionaryValue* params) {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  parent_screen_ = parent_screen;
  ShowScreen(OobeUI::kScreenErrorMessage, params);
  NetworkErrorShown();
  EnableLazyDetection();
}

void ErrorScreenHandler::Hide() {
  if (parent_screen_ == OobeUI::SCREEN_UNKNOWN)
    return;
  std::string screen_name;
  if (GetScreenName(parent_screen_, &screen_name))
    ShowScreen(screen_name.c_str(), NULL);
  DisableLazyDetection();
}

void ErrorScreenHandler::FixCaptivePortal() {
  if (!captive_portal_window_proxy_.get()) {
    captive_portal_window_proxy_.reset(
        new CaptivePortalWindowProxy(network_state_informer_.get(),
                                     GetNativeWindow()));
  }
  captive_portal_window_proxy_->ShowIfRedirected();
}

void ErrorScreenHandler::ShowCaptivePortal() {
  // This call is an explicit user action
  // i.e. clicking on link so force dialog show.
  FixCaptivePortal();
  captive_portal_window_proxy_->Show();
}

void ErrorScreenHandler::HideCaptivePortal() {
  if (captive_portal_window_proxy_.get())
    captive_portal_window_proxy_->Close();
}

void ErrorScreenHandler::SetUIState(ErrorScreen::UIState ui_state) {
  ui_state_ = ui_state;
  base::FundamentalValue ui_state_value(static_cast<int>(ui_state));
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.setUIState",
                                   ui_state_value);
}

void ErrorScreenHandler::SetErrorState(ErrorScreen::ErrorState error_state,
                                       const std::string& network) {
  error_state_ = error_state;
  base::FundamentalValue error_state_value(static_cast<int>(error_state));
  base::StringValue network_value(network);
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.setErrorState",
                                   error_state_value,
                                   network_value);
}

void ErrorScreenHandler::AllowGuestSignin(bool allowed) {
  base::FundamentalValue allowed_value(allowed);
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.allowGuestSignin",
                                   allowed_value);
}

void ErrorScreenHandler::AllowOfflineLogin(bool allowed) {
  base::FundamentalValue allowed_value(allowed);
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.allowOfflineLogin",
                                   allowed_value);
}

void ErrorScreenHandler::NetworkErrorShown() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_NETWORK_ERROR_SHOWN,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

bool ErrorScreenHandler::GetScreenName(OobeUI::Screen screen,
                                       std::string* name) const {
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (!oobe_ui)
    return false;
  *name = oobe_ui->GetScreenName(screen);
  return true;
}

void ErrorScreenHandler::HandleShowCaptivePortal(const base::ListValue* args) {
  ShowCaptivePortal();
}

void ErrorScreenHandler::HandleHideCaptivePortal(const base::ListValue* args) {
  HideCaptivePortal();
}

void ErrorScreenHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("showCaptivePortal",
      base::Bind(&ErrorScreenHandler::HandleShowCaptivePortal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("hideCaptivePortal",
      base::Bind(&ErrorScreenHandler::HandleHideCaptivePortal,
                 base::Unretained(this)));
}

void ErrorScreenHandler::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("proxyErrorTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_PROXY_ERROR_TITLE));
  localized_strings->SetString("signinOfflineMessageBody",
      l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_MESSAGE));
  localized_strings->SetString("captivePortalTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_TITLE));
  localized_strings->SetString("captivePortalMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL));
  localized_strings->SetString("captivePortalProxyMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_PROXY));
  localized_strings->SetString("captivePortalNetworkSelect",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT));
  localized_strings->SetString("signinProxyMessageText",
      l10n_util::GetStringUTF16(IDS_LOGIN_PROXY_ERROR_MESSAGE));
  localized_strings->SetString("updateOfflineMessageBody",
      l10n_util::GetStringUTF16(IDS_UPDATE_OFFLINE_MESSAGE));
  localized_strings->SetString("updateProxyMessageText",
      l10n_util::GetStringUTF16(IDS_UPDATE_PROXY_ERROR_MESSAGE));
}

void ErrorScreenHandler::Initialize() {
  if (!page_is_ready())
    return;
  if (show_on_init_) {
    Show(parent_screen_, NULL);
    show_on_init_ = false;
  }
}

}  // namespace chromeos
