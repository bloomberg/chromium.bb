// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/chrome_unscaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

const char kJsScreenPath[] = "login.AppLaunchSplashScreen";

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const chromeos::NetworkState* network =
      chromeos::NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network)
    return std::string();
  return network->name();
}

}  // namespace

namespace chromeos {

AppLaunchSplashScreenHandler::AppLaunchSplashScreenHandler(
      const scoped_refptr<NetworkStateInformer>& network_state_informer,
      ErrorScreenActor* error_screen_actor)
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      show_on_init_(false),
      state_(APP_LAUNCH_STATE_LOADING_AUTH_FILE),
      network_state_informer_(network_state_informer),
      error_screen_actor_(error_screen_actor),
      online_state_(false),
      network_config_done_(false),
      network_config_requested_(false) {
  network_state_informer_->AddObserver(this);
}

AppLaunchSplashScreenHandler::~AppLaunchSplashScreenHandler() {
  network_state_informer_->RemoveObserver(this);
}

void AppLaunchSplashScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {

  builder->Add("appStartMessage", IDS_APP_START_NETWORK_WAIT_MESSAGE);
  builder->Add("configureNetwork", IDS_APP_START_CONFIGURE_NETWORK);

  const base::string16 product_os_name =
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
  AddCallback("continueAppLaunch",
              &AppLaunchSplashScreenHandler::HandleContinueAppLaunch);
  AddCallback("networkConfigRequest",
              &AppLaunchSplashScreenHandler::HandleNetworkConfigRequested);
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
  UpdateState(ErrorScreenActor::ERROR_REASON_UPDATE);
}

void AppLaunchSplashScreenHandler::SetDelegate(
    AppLaunchSplashScreenHandler::Delegate* delegate) {
  delegate_ = delegate;
}

void AppLaunchSplashScreenHandler::ShowNetworkConfigureUI() {
  NetworkStateInformer::State state = network_state_informer_->state();
  if (state == NetworkStateInformer::ONLINE) {
    online_state_ = true;
    if (!network_config_requested_) {
      delegate_->OnNetworkStateChanged(true);
      return;
    }
  }

  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name = GetNetworkName(network_path);

  error_screen_actor_->SetUIState(ErrorScreen::UI_STATE_KIOSK_MODE);
  error_screen_actor_->AllowGuestSignin(false);
  error_screen_actor_->AllowOfflineLogin(false);

  switch (state) {
    case NetworkStateInformer::CAPTIVE_PORTAL: {
      error_screen_actor_->SetErrorState(
          ErrorScreen::ERROR_STATE_PORTAL, network_name);
      error_screen_actor_->FixCaptivePortal();

      break;
    }
    case NetworkStateInformer::PROXY_AUTH_REQUIRED: {
      error_screen_actor_->SetErrorState(
          ErrorScreen::ERROR_STATE_PROXY, network_name);
      break;
    }
    case NetworkStateInformer::OFFLINE: {
      error_screen_actor_->SetErrorState(
          ErrorScreen::ERROR_STATE_OFFLINE, network_name);
      break;
    }
    case NetworkStateInformer::ONLINE: {
      error_screen_actor_->SetErrorState(
          ErrorScreen::ERROR_STATE_KIOSK_ONLINE, network_name);
      break;
    }
    default:
      error_screen_actor_->SetErrorState(
          ErrorScreen::ERROR_STATE_OFFLINE, network_name);
      NOTREACHED();
      break;
  }

  OobeUI::Screen screen = OobeUI::SCREEN_UNKNOWN;
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (oobe_ui)
    screen = oobe_ui->current_screen();

  if (screen != OobeUI::SCREEN_ERROR_MESSAGE)
    error_screen_actor_->Show(OobeDisplay::SCREEN_APP_LAUNCH_SPLASH, NULL);
}

bool AppLaunchSplashScreenHandler::IsNetworkReady() {
  return network_state_informer_->state() == NetworkStateInformer::ONLINE;
}

void AppLaunchSplashScreenHandler::OnNetworkReady() {
  // Purposely leave blank because the online case is handled in UpdateState
  // call below.
}

void AppLaunchSplashScreenHandler::UpdateState(
    ErrorScreenActor::ErrorReason reason) {
  if (!delegate_ ||
      (state_ != APP_LAUNCH_STATE_PREPARING_NETWORK &&
       state_ != APP_LAUNCH_STATE_NETWORK_WAIT_TIMEOUT)) {
    return;
  }

  bool new_online_state =
      network_state_informer_->state() == NetworkStateInformer::ONLINE;
  delegate_->OnNetworkStateChanged(new_online_state);

  online_state_ = new_online_state;
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
    case APP_LAUNCH_STATE_WAITING_APP_WINDOW:
      return IDS_APP_START_WAIT_FOR_APP_WINDOW_MESSAGE;
    case APP_LAUNCH_STATE_NETWORK_WAIT_TIMEOUT:
      return IDS_APP_START_NETWORK_WAIT_TIMEOUT_MESSAGE;
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

void AppLaunchSplashScreenHandler::HandleNetworkConfigRequested() {
  if (!delegate_ || network_config_done_)
    return;

  network_config_requested_ = true;
  delegate_->OnNetworkConfigRequested(true);
}

void AppLaunchSplashScreenHandler::HandleContinueAppLaunch() {
  DCHECK(online_state_);
  if (delegate_ && online_state_) {
    network_config_requested_ = false;
    network_config_done_ = true;
    delegate_->OnNetworkConfigRequested(false);
    Show(app_id_);
  }
}

}  // namespace chromeos
