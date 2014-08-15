// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"
#include "chrome/browser/chromeos/app_mode/certificate_manager_dialog.h"
#include "chrome/browser/chromeos/login/ui/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "chromeos/network/portal_detector/network_portal_detector_strategy.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_system.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"

namespace {

const char kJsScreenPath[] = "login.ErrorMessageScreen";

}  // namespace

namespace chromeos {

ErrorScreenHandler::ErrorScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer)
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      network_state_informer_(network_state_informer),
      show_on_init_(false),
      weak_ptr_factory_(this) {
  DCHECK(network_state_informer_.get());
}

ErrorScreenHandler::~ErrorScreenHandler() {}

void ErrorScreenHandler::SetDelegate(ErrorScreenActorDelegate* delegate) {
  delegate_ = delegate;
}

void ErrorScreenHandler::Show(OobeDisplay::Screen parent_screen,
                              base::DictionaryValue* params,
                              const base::Closure& on_hide) {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  parent_screen_ = parent_screen;
  on_hide_.reset(new base::Closure(on_hide));
  ShowScreen(OobeUI::kScreenErrorMessage, params);
  NetworkErrorShown();
  NetworkPortalDetector::Get()->SetStrategy(
      PortalDetectorStrategy::STRATEGY_ID_ERROR_SCREEN);
  if (delegate_)
    delegate_->OnErrorShow();
  LOG(WARNING) << "Offline message is displayed";
}

void ErrorScreenHandler::CheckAndShowScreen() {
  std::string screen_name;
  if (GetScreenName(parent_screen(), &screen_name))
    ShowScreen(screen_name.c_str(), NULL);
}

void ErrorScreenHandler::Show(OobeDisplay::Screen parent_screen,
                              base::DictionaryValue* params) {
  Show(parent_screen,
       params,
       base::Bind(&ErrorScreenHandler::CheckAndShowScreen,
                  weak_ptr_factory_.GetWeakPtr()));
}

void ErrorScreenHandler::Hide() {
  if (parent_screen_ == OobeUI::SCREEN_UNKNOWN)
    return;
  if (on_hide_)
    on_hide_->Run();
  NetworkPortalDetector::Get()->SetStrategy(
      PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN);
  if (delegate_)
    delegate_->OnErrorHide();
  LOG(WARNING) << "Offline message is hidden";
}

void ErrorScreenHandler::FixCaptivePortal() {
  if (!captive_portal_window_proxy_.get()) {
    content::WebContents* web_contents =
        LoginDisplayHostImpl::default_host()->GetWebUILoginView()->
            GetWebContents();
    captive_portal_window_proxy_.reset(
        new CaptivePortalWindowProxy(network_state_informer_.get(),
                                     web_contents));
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
  show_connecting_indicator_ = false;
  ui_state_ = ui_state;
  if (page_is_ready())
    CallJS("setUIState", static_cast<int>(ui_state_));
}

void ErrorScreenHandler::SetErrorState(ErrorScreen::ErrorState error_state,
                                       const std::string& network) {
  error_state_ = error_state;
  network_ = network;
  if (page_is_ready())
    CallJS("setErrorState", static_cast<int>(error_state_), network);
}

void ErrorScreenHandler::AllowGuestSignin(bool allowed) {
  guest_signin_allowed_ = allowed;
  if (page_is_ready())
    CallJS("allowGuestSignin", allowed);
}

void ErrorScreenHandler::AllowOfflineLogin(bool allowed) {
  offline_login_allowed_ = allowed;
  if (page_is_ready())
    CallJS("allowOfflineLogin", allowed);
}

void ErrorScreenHandler::ShowConnectingIndicator(bool show) {
  show_connecting_indicator_ = show;
  if (page_is_ready())
    CallJS("showConnectingIndicator", show);
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

void ErrorScreenHandler::HandleLocalStateErrorPowerwashButtonClicked() {
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->
      StartDeviceWipe();
}

void ErrorScreenHandler::HandleRebootButtonClicked() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void ErrorScreenHandler::HandleDiagnoseButtonClicked() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  std::string extension_id =
      extension_service->component_loader()->Add(
          IDR_CONNECTIVITY_DIAGNOSTICS_MANIFEST,
          base::FilePath(extension_misc::kConnectivityDiagnosticsKioskPath));

  const extensions::Extension* extension = extension_service->
      GetExtensionById(extension_id, true);
  OpenApplication(AppLaunchParams(profile, extension,
                                  extensions::LAUNCH_CONTAINER_WINDOW,
                                  NEW_WINDOW));
  InitAppSession(profile, extension_id);

  user_manager::UserManager::Get()->SessionStarted();

  LoginDisplayHostImpl::default_host()->Finalize();
}

void ErrorScreenHandler::HandleConfigureCerts() {
  CertificateManagerDialog* dialog =
      new CertificateManagerDialog(ProfileManager::GetActiveUserProfile(),
                                   NULL,
                                   GetNativeWindow());
  dialog->Show();
}

void ErrorScreenHandler::HandleLaunchOobeGuestSession() {
  if (delegate_)
    delegate_->OnLaunchOobeGuestSession();
}

void ErrorScreenHandler::RegisterMessages() {
  AddCallback("showCaptivePortal",
              &ErrorScreenHandler::HandleShowCaptivePortal);
  AddCallback("hideCaptivePortal",
              &ErrorScreenHandler::HandleHideCaptivePortal);
  AddCallback("localStateErrorPowerwashButtonClicked",
              &ErrorScreenHandler::HandleLocalStateErrorPowerwashButtonClicked);
  AddCallback("rebootButtonClicked",
              &ErrorScreenHandler::HandleRebootButtonClicked);
  AddCallback("diagnoseButtonClicked",
              &ErrorScreenHandler::HandleDiagnoseButtonClicked);
  AddCallback("configureCertsClicked",
              &ErrorScreenHandler::HandleConfigureCerts);
  AddCallback("launchOobeGuestSession",
              &ErrorScreenHandler::HandleLaunchOobeGuestSession);
  AddCallback("rollbackOkButtonClicked",
              &ErrorScreenHandler::HandleRebootButtonClicked);
}

void ErrorScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("loginErrorTitle", IDS_LOGIN_ERROR_TITLE);
  builder->Add("rollbackErrorTitle", IDS_RESET_SCREEN_REVERT_ERROR);
  builder->Add("signinOfflineMessageBody", IDS_LOGIN_OFFLINE_MESSAGE);
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
  builder->Add("updateOfflineMessageBody", IDS_UPDATE_OFFLINE_MESSAGE);
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
    base::DictionaryValue params;
    params.SetInteger("uiState", static_cast<int>(ui_state_));
    params.SetInteger("errorState", static_cast<int>(error_state_));
    params.SetString("network", network_);
    params.SetBoolean("guestSigninAllowed", guest_signin_allowed_);
    params.SetBoolean("offlineLoginAllowed", offline_login_allowed_);
    params.SetBoolean("showConnectingIndicator", show_connecting_indicator_);
    Show(parent_screen_, &params);
    show_on_init_ = false;
  }
}

}  // namespace chromeos
