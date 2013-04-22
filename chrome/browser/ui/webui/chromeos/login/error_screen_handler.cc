// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/common/chrome_notification_types.h"
#include "grit/generated_resources.h"

namespace chromeos {

namespace {

void EnableLazyDetection() {
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (NetworkPortalDetector::IsEnabledInCommandLine() && detector)
    detector->EnableLazyDetection();
}

void DisableLazyDetection() {
  NetworkPortalDetector* detector = NetworkPortalDetector::GetInstance();
  if (NetworkPortalDetector::IsEnabledInCommandLine() && detector)
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
  CallJS("login.ErrorMessageScreen.setUIState", static_cast<int>(ui_state_));
}

void ErrorScreenHandler::SetErrorState(ErrorScreen::ErrorState error_state,
                                       const std::string& network) {
  error_state_ = error_state;
  CallJS("login.ErrorMessageScreen.setErrorState",
         static_cast<int>(error_state_), network);
}

void ErrorScreenHandler::AllowGuestSignin(bool allowed) {
  CallJS("login.ErrorMessageScreen.allowGuestSignin", allowed);
}

void ErrorScreenHandler::AllowOfflineLogin(bool allowed) {
  CallJS("login.ErrorMessageScreen.allowOfflineLogin", allowed);
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

void ErrorScreenHandler::HandleShowCaptivePortal() {
  ShowCaptivePortal();
}

void ErrorScreenHandler::HandleHideCaptivePortal() {
  HideCaptivePortal();
}

void ErrorScreenHandler::RegisterMessages() {
  AddCallback("showCaptivePortal",
              &ErrorScreenHandler::HandleShowCaptivePortal);
  AddCallback("hideCaptivePortal",
              &ErrorScreenHandler::HandleHideCaptivePortal);
}

void ErrorScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("proxyErrorTitle", IDS_LOGIN_PROXY_ERROR_TITLE);
  builder->Add("proxyErrorTitle", IDS_LOGIN_PROXY_ERROR_TITLE);
  builder->Add("signinOfflineMessageBody", IDS_LOGIN_OFFLINE_MESSAGE);
  builder->Add("captivePortalTitle", IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_TITLE);
  builder->Add("captivePortalMessage", IDS_LOGIN_MAYBE_CAPTIVE_PORTAL);
  builder->Add("captivePortalProxyMessage",
               IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_PROXY);
  builder->Add("captivePortalNetworkSelect",
               IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT);
  builder->Add("signinProxyMessageText", IDS_LOGIN_PROXY_ERROR_MESSAGE);
  builder->Add("updateOfflineMessageBody", IDS_UPDATE_OFFLINE_MESSAGE);
  builder->Add("updateProxyMessageText", IDS_UPDATE_PROXY_ERROR_MESSAGE);
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
