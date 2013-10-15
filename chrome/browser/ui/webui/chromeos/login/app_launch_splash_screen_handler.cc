// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/browser_resources.h"
#include "grit/chrome_unscaled_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

const char kJsScreenPath[] = "login.AppLaunchSplashScreen";

} // namespace

namespace chromeos {

AppLaunchSplashScreenHandler::AppLaunchSplashScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false),
      state_(APP_LAUNCH_STATE_LOADING_AUTH_FILE) {
}

AppLaunchSplashScreenHandler::~AppLaunchSplashScreenHandler() {
}

void AppLaunchSplashScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {

  builder->Add("appStartMessage", IDS_APP_START_NETWORK_WAIT_MESSAGE);
  builder->Add("configureNetwork", IDS_APP_START_CONFIGURE_NETWORK);

  const string16 product_os_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME);
  builder->Add(
      "shortcutInfo",
      l10n_util::GetStringFUTF16(IDS_APP_START_BAILOUT_SHORTCUT_FORMAT,
                                 product_os_name));

  builder->Add(
      "productName",
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME));
}

void AppLaunchSplashScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show(app_id_);
  }
}

void AppLaunchSplashScreenHandler::Show(const std::string& app_id) {
  app_id_ = app_id;
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  base::DictionaryValue data;
  data.SetBoolean("shortcutEnabled",
                  !KioskAppManager::Get()->GetDisableBailoutShortcut());

  // |data| will take ownership of |app_info|.
  base::DictionaryValue *app_info = new base::DictionaryValue();
  PopulateAppInfo(app_info);
  data.Set("appInfo", app_info);

  SetLaunchText(l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
  ShowScreen(OobeUI::kScreenAppLaunchSplash, &data);
}

void AppLaunchSplashScreenHandler::RegisterMessages() {
  AddCallback("configureNetwork",
              &AppLaunchSplashScreenHandler::HandleConfigureNetwork);
  AddCallback("cancelAppLaunch",
              &AppLaunchSplashScreenHandler::HandleCancelAppLaunch);
}

void AppLaunchSplashScreenHandler::PrepareToShow() {
}

void AppLaunchSplashScreenHandler::Hide() {
}

void AppLaunchSplashScreenHandler::ToggleNetworkConfig(bool visible) {
  CallJS("toggleNetworkConfig", visible);
}

void AppLaunchSplashScreenHandler::UpdateAppLaunchState(AppLaunchState state) {
  if (state == state_)
    return;

  state_ = state;
  if (page_is_ready()) {
    SetLaunchText(
        l10n_util::GetStringUTF8(GetProgressMessageFromState(state_)));
  }
}

void AppLaunchSplashScreenHandler::SetDelegate(
    AppLaunchSplashScreenHandler::Delegate* delegate) {
  delegate_ = delegate;
}

void AppLaunchSplashScreenHandler::PopulateAppInfo(
    base::DictionaryValue* out_info) {
  KioskAppManager::App app;
  KioskAppManager::Get()->GetApp(app_id_, &app);

  if (app.name.empty())
    app.name = l10n_util::GetStringUTF8(IDS_SHORT_PRODUCT_NAME);

  if (app.icon.isNull()) {
    app.icon = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PRODUCT_LOGO_128);
  }

  out_info->SetString("name", app.name);
  out_info->SetString("iconURL", webui::GetBitmapDataUrl(*app.icon.bitmap()));
}

void AppLaunchSplashScreenHandler::SetLaunchText(const std::string& text) {
  CallJS("updateMessage", text);
}

int AppLaunchSplashScreenHandler::GetProgressMessageFromState(
    AppLaunchState state) {
  switch (state) {
    case APP_LAUNCH_STATE_LOADING_AUTH_FILE:
    case APP_LAUNCH_STATE_LOADING_TOKEN_SERVICE:
      // TODO(zelidrag): Add better string for this one than "Please wait..."
      return IDS_SYNC_SETUP_SPINNER_TITLE;
    case APP_LAUNCH_STATE_PREPARING_NETWORK:
      return IDS_APP_START_NETWORK_WAIT_MESSAGE;
    case APP_LAUNCH_STATE_INSTALLING_APPLICATION:
      return IDS_APP_START_APP_WAIT_MESSAGE;
  }
  return IDS_APP_START_NETWORK_WAIT_MESSAGE;
}

void AppLaunchSplashScreenHandler::HandleConfigureNetwork() {
  if (delegate_)
    delegate_->OnConfigureNetwork();
  else
    LOG(WARNING) << "No delegate set to handle network configuration.";
}

void AppLaunchSplashScreenHandler::HandleCancelAppLaunch() {
  if (delegate_)
    delegate_->OnCancelAppLaunch();
  else
    LOG(WARNING) << "No delegate set to handle cancel app launch";
}

}  // namespace chromeos
