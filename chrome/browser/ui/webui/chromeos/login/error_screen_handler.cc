// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"

#include "ash/system/chromeos/devicetype_utils.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/screens/network_error_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const char kJsScreenPath[] = "login.ErrorMessageScreen";

}  // namespace

namespace chromeos {

ErrorScreenHandler::ErrorScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      model_(nullptr),
      show_on_init_(false),
      showing_(false),
      weak_ptr_factory_(this) {
}

ErrorScreenHandler::~ErrorScreenHandler() {
  if (model_)
    model_->OnViewDestroyed(this);
}

void ErrorScreenHandler::PrepareToShow() {
}

void ErrorScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  BaseScreenHandler::ShowScreen(OobeUI::kScreenErrorMessage, NULL);
  if (model_)
    model_->OnShow();
  showing_ = true;
}

void ErrorScreenHandler::Hide() {
  showing_ = false;
  if (model_)
   model_->OnHide();
}

void ErrorScreenHandler::Bind(NetworkErrorModel& model) {
  model_ = &model;
  BaseScreenHandler::SetBaseScreen(model_);
}

void ErrorScreenHandler::Unbind() {
  model_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void ErrorScreenHandler::ShowScreen(OobeUI::Screen screen) {
  std::string screen_name;
  if (GetScreenName(screen, &screen_name))
    BaseScreenHandler::ShowScreen(screen_name.c_str(), NULL);
}

bool ErrorScreenHandler::GetScreenName(OobeUI::Screen screen,
                                       std::string* name) const {
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (!oobe_ui)
    return false;
  *name = oobe_ui->GetScreenName(screen);
  return true;
}

void ErrorScreenHandler::HandleHideCaptivePortal() {
  if (model_)
    model_->HideCaptivePortal();
}

void ErrorScreenHandler::RegisterMessages() {
  AddCallback("hideCaptivePortal",
              &ErrorScreenHandler::HandleHideCaptivePortal);
  BaseScreenHandler::RegisterMessages();
}

void ErrorScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("deviceType", ash::GetChromeOSDeviceName());
  builder->Add("loginErrorTitle", IDS_LOGIN_ERROR_TITLE);
  builder->Add("rollbackErrorTitle", IDS_RESET_SCREEN_REVERT_ERROR);
  builder->Add("signinOfflineMessageBody",
               ash::SubstituteChromeOSDeviceType(IDS_LOGIN_OFFLINE_MESSAGE));
  builder->Add("kioskOfflineMessageBody", IDS_KIOSK_OFFLINE_MESSAGE);
  builder->Add("kioskOnlineTitle", IDS_LOGIN_NETWORK_RESTORED_TITLE);
  builder->Add("kioskOnlineMessageBody", IDS_KIOSK_ONLINE_MESSAGE);
  builder->Add("autoEnrollmentOfflineMessageBody",
               IDS_LOGIN_AUTO_ENROLLMENT_OFFLINE_MESSAGE);
  builder->AddF("rollbackErrorMessageBody",
               IDS_RESET_SCREEN_REVERT_ERROR_EXPLANATION,
               IDS_SHORT_PRODUCT_NAME);
  builder->Add("captivePortalTitle", IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_TITLE);
  builder->Add("captivePortalMessage", IDS_LOGIN_MAYBE_CAPTIVE_PORTAL);
  builder->Add("captivePortalProxyMessage",
               IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_PROXY);
  builder->Add("captivePortalNetworkSelect",
               IDS_LOGIN_MAYBE_CAPTIVE_PORTAL_NETWORK_SELECT);
  builder->Add("signinProxyMessageText", IDS_LOGIN_PROXY_ERROR_MESSAGE);
  builder->Add("updateOfflineMessageBody",
               ash::SubstituteChromeOSDeviceType(IDS_UPDATE_OFFLINE_MESSAGE));
  builder->Add("updateProxyMessageText", IDS_UPDATE_PROXY_ERROR_MESSAGE);
  builder->AddF("localStateErrorText0", IDS_LOCAL_STATE_ERROR_TEXT_0,
                IDS_SHORT_PRODUCT_NAME);
  builder->Add("localStateErrorText1", IDS_LOCAL_STATE_ERROR_TEXT_1);
  builder->Add("localStateErrorPowerwashButton",
               IDS_LOCAL_STATE_ERROR_POWERWASH_BUTTON);
  builder->Add("connectingIndicatorText", IDS_LOGIN_CONNECTING_INDICATOR_TEXT);
  builder->Add("guestSigninFixNetwork", IDS_LOGIN_GUEST_SIGNIN_FIX_NETWORK);
  builder->Add("rebootButton", IDS_RELAUNCH_BUTTON);
  builder->Add("diagnoseButton", IDS_DIAGNOSE_BUTTON);
  builder->Add("configureCertsButton", IDS_MANAGE_CERTIFICATES);
  builder->Add("continueButton", IDS_NETWORK_SELECTION_CONTINUE_BUTTON);
  builder->Add("okButton", IDS_APP_OK);
}

void ErrorScreenHandler::Initialize() {
  if (!page_is_ready())
    return;

  if (show_on_init_) {
    // TODO(nkostylev): Check that context initial state is properly passed.
    Show();
    show_on_init_ = false;
  }
}

void ErrorScreenHandler::OnConnectToNetworkRequested() {
  if (showing_ && model_)
    model_->OnUserAction(NetworkErrorModel::kUserActionConnectRequested);
}

}  // namespace chromeos
