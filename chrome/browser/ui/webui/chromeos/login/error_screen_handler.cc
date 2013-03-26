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
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

ErrorScreenHandler::ErrorScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : network_state_informer_(network_state_informer),
      native_window_delegate_(NULL),
      show_on_init_(false) {
  DCHECK(network_state_informer_.get());
}

ErrorScreenHandler::~ErrorScreenHandler() {
}

void ErrorScreenHandler::SetNativeWindowDelegate(
    NativeWindowDelegate* native_window_delegate) {
  native_window_delegate_ = native_window_delegate;
}

void ErrorScreenHandler::Show(OobeUI::Screen parent_screen,
                              base::DictionaryValue* params) {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  parent_screen_ = parent_screen;
  ShowScreen(OobeUI::kScreenErrorMessage, params);
  NetworkErrorShown();
}

void ErrorScreenHandler::Hide() {
  if (parent_screen_ != OobeUI::SCREEN_UNKNOWN) {
    std::string screen_name;
    if (GetScreenName(parent_screen_, &screen_name))
      ShowScreen(screen_name.c_str(), NULL);
  }
}

void ErrorScreenHandler::FixCaptivePortal() {
  if (!native_window_delegate_)
    return;
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

void ErrorScreenHandler::ShowProxyError() {
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.showProxyError");
  state_ = STATE_PROXY_ERROR;
}

void ErrorScreenHandler::ShowCaptivePortalError(const std::string& network) {
  base::StringValue network_value(network);
  web_ui()->CallJavascriptFunction(
      "login.ErrorMessageScreen.showCaptivePortalError", network_value);
  state_ = STATE_CAPTIVE_PORTAL_ERROR;
}

void ErrorScreenHandler::ShowTimeoutError() {
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.showTimeoutError");
  state_ = STATE_TIMEOUT_ERROR;
}

void ErrorScreenHandler::ShowOfflineError() {
  web_ui()->CallJavascriptFunction("login.ErrorMessageScreen.showOfflineError");
  state_ = STATE_OFFLINE_ERROR;
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
  localized_strings->SetString("offlineMessageBody",
      l10n_util::GetStringUTF16(IDS_LOGIN_OFFLINE_MESSAGE));
  localized_strings->SetString("captivePortalTitle",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_TITLE));
  localized_strings->SetString("captivePortalMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL));
  localized_strings->SetString("captivePortalProxyMessage",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_PROXY));
  localized_strings->SetString("captivePortalNetworkSelect",
      l10n_util::GetStringUTF16(IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT));
  localized_strings->SetString("proxyMessageText",
      l10n_util::GetStringUTF16(IDS_LOGIN_PROXY_ERROR_MESSAGE));
}

void ErrorScreenHandler::Initialize() {
  if (!page_is_ready())
    return;
  if (show_on_init_) {
    Show(parent_screen_, NULL);
    show_on_init_ = false;
  }
}

gfx::NativeWindow ErrorScreenHandler::GetNativeWindow() {
  if (native_window_delegate_)
    return native_window_delegate_->GetNativeWindow();
  return NULL;
}

}  // namespace chromeos
