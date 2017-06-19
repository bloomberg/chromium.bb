// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/system/devicetype_utils.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/webui_screen_locker.h"
#include "chrome/browser/chromeos/login/lock_screen_utils.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_view.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/ui/login_feedback.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/system_clock.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/color_utils.h"

namespace {

// Max number of users to show.
const size_t kMaxUsers = 18;

// Timeout to delay first notification about offline state for a
// current network.
const int kOfflineTimeoutSec = 5;

// Timeout used to prevent infinite connecting to a flaky network.
const int kConnectingTimeoutSec = 60;

// Max number of Gaia Reload to Show Proxy Auth Dialog.
const int kMaxGaiaReloadForProxyAuthDialog = 3;

// Type of the login screen UI that is currently presented to user.
const char kSourceGaiaSignin[] = "gaia-signin";
const char kSourceAccountPicker[] = "account-picker";

// Constants for lock screen apps activity state values:
const char kNoLockScreenApps[] = "LOCK_SCREEN_APPS_STATE.NONE";
const char kBackgroundLockScreenApps[] = "LOCK_SCREEN_APPS_STATE.BACKGROUND";
const char kForegroundLockScreenApps[] = "LOCK_SCREEN_APPS_STATE.FOREGROUND";

// The alpha value for the signin screen background.
// TODO(crbug.com/732566): Move all constants related to views-based signin
// screen to a separate file.
constexpr int kLoginTranslucentAlpha = 76;

class CallOnReturn {
 public:
  explicit CallOnReturn(const base::Closure& callback)
      : callback_(callback), call_scheduled_(false) {}

  ~CallOnReturn() {
    if (call_scheduled_ && !callback_.is_null())
      callback_.Run();
  }

  void CancelScheduledCall() { call_scheduled_ = false; }
  void ScheduleCall() { call_scheduled_ = true; }

 private:
  base::Closure callback_;
  bool call_scheduled_;

  DISALLOW_COPY_AND_ASSIGN(CallOnReturn);
};

}  // namespace

namespace chromeos {

namespace {

bool IsOnline(NetworkStateInformer::State state,
              NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::ONLINE &&
         reason != NetworkError::ERROR_REASON_PORTAL_DETECTED &&
         reason != NetworkError::ERROR_REASON_LOADING_TIMEOUT;
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           NetworkError::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
         reason == NetworkError::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  NetworkError::ErrorReason reason,
                  net::Error frame_error) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
         reason == NetworkError::ERROR_REASON_PROXY_AUTH_CANCELLED ||
         reason == NetworkError::ERROR_REASON_PROXY_CONNECTION_FAILED ||
         (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
          (frame_error == net::ERR_PROXY_CONNECTION_FAILED ||
           frame_error == net::ERR_TUNNEL_CONNECTION_FAILED));
}

bool IsSigninScreen(const OobeScreen screen) {
  return screen == OobeScreen::SCREEN_GAIA_SIGNIN ||
         screen == OobeScreen::SCREEN_ACCOUNT_PICKER;
}

bool IsSigninScreenError(NetworkError::ErrorState error_state) {
  return error_state == NetworkError::ERROR_STATE_PORTAL ||
         error_state == NetworkError::ERROR_STATE_OFFLINE ||
         error_state == NetworkError::ERROR_STATE_PROXY ||
         error_state == NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT;
}

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!network)
    return std::string();
  return network->name();
}

}  // namespace

// LoginScreenContext implementation ------------------------------------------

LoginScreenContext::LoginScreenContext() {
  Init();
}

LoginScreenContext::LoginScreenContext(const base::ListValue* args) {
  Init();

  if (!args || args->GetSize() == 0)
    return;
  std::string email;
  if (args->GetString(0, &email))
    email_ = email;
}

void LoginScreenContext::Init() {
  oobe_ui_ = false;
}

// SigninScreenHandler implementation ------------------------------------------

SigninScreenHandler::SigninScreenHandler(
    const scoped_refptr<NetworkStateInformer>& network_state_informer,
    ErrorScreen* error_screen,
    CoreOobeView* core_oobe_view,
    GaiaScreenHandler* gaia_screen_handler,
    JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container),
      network_state_informer_(network_state_informer),
      error_screen_(error_screen),
      core_oobe_view_(core_oobe_view),
      caps_lock_enabled_(chromeos::input_method::InputMethodManager::Get()
                             ->GetImeKeyboard()
                             ->CapsLockIsEnabled()),
      proxy_auth_dialog_reload_times_(kMaxGaiaReloadForProxyAuthDialog),
      gaia_screen_handler_(gaia_screen_handler),
      touch_view_binding_(this),
      histogram_helper_(new ErrorScreensHistogramHelper("Signin")),
      lock_screen_apps_observer_(this),
      weak_factory_(this) {
  DCHECK(network_state_informer_.get());
  DCHECK(error_screen_);
  DCHECK(core_oobe_view_);
  DCHECK(js_calls_container);
  gaia_screen_handler_->set_signin_screen_handler(this);
  network_state_informer_->AddObserver(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_NEEDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);

  chromeos::input_method::ImeKeyboard* keyboard =
      chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->AddObserver(this);
  allowed_input_methods_subscription_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kDeviceLoginScreenInputMethods,
          base::Bind(&SigninScreenHandler::OnAllowedInputMethodsChanged,
                     base::Unretained(this)));

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &touch_view_manager_ptr_);
  ash::mojom::TouchViewObserverPtr observer;
  touch_view_binding_.Bind(mojo::MakeRequest(&observer));
  touch_view_manager_ptr_->AddObserver(std::move(observer));
  if (ScreenLocker::default_screen_locker() &&
      lock_screen_apps::StateController::IsEnabled()) {
    lock_screen_apps_observer_.Add(lock_screen_apps::StateController::Get());
  }
  if (WallpaperManager::HasInstance())
    WallpaperManager::Get()->AddObserver(this);
}

SigninScreenHandler::~SigninScreenHandler() {
  OobeUI* oobe_ui = GetOobeUI();
  if (oobe_ui && oobe_ui_observer_added_)
    oobe_ui->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
  chromeos::input_method::ImeKeyboard* keyboard =
      chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
  if (keyboard)
    keyboard->RemoveObserver(this);
  lock_screen_utils::StopEnforcingPolicyInputMethods();
  weak_factory_.InvalidateWeakPtrs();
  if (delegate_)
    delegate_->SetWebUIHandler(nullptr);
  network_state_informer_->RemoveObserver(this);
  proximity_auth::ScreenlockBridge::Get()->SetLockHandler(nullptr);
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(EmptyAccountId());
  if (WallpaperManager::HasInstance())
    WallpaperManager::Get()->RemoveObserver(this);
}

void SigninScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // Format numbers to be used on the pin keyboard.
  for (int j = 0; j <= 9; j++) {
    builder->Add("pinKeyboard" + base::IntToString(j),
                 base::FormatNumber(int64_t{j}));
  }

  builder->Add("passwordHint", IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT);
  builder->Add("pinKeyboardPlaceholderPin",
               IDS_PIN_KEYBOARD_HINT_TEXT_PIN);
  builder->Add("pinKeyboardPlaceholderPinPassword",
               IDS_PIN_KEYBOARD_HINT_TEXT_PIN_PASSWORD);
  builder->Add("pinKeyboardDeleteAccessibleName",
               IDS_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME);
  builder->Add("fingerprintHint", IDS_FINGERPRINT_HINT_TEXT);
  builder->Add("fingerprintIconMessage", IDS_FINGERPRINT_ICON_MESSAGE);
  builder->Add("fingerprintSigningin", IDS_FINGERPRINT_LOGIN_TEXT);
  builder->Add("fingerprintSigninFailed", IDS_FINGERPRINT_LOGIN_FAILED_TEXT);
  builder->Add("signingIn", IDS_LOGIN_POD_SIGNING_IN);
  builder->Add("podMenuButtonAccessibleName",
               IDS_LOGIN_POD_MENU_BUTTON_ACCESSIBLE_NAME);
  builder->Add("podMenuRemoveItemAccessibleName",
               IDS_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME);
  builder->Add("passwordFieldAccessibleName",
               IDS_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME);
  builder->Add("submitButtonAccessibleName",
               IDS_LOGIN_POD_SUBMIT_BUTTON_ACCESSIBLE_NAME);
  builder->Add("signedIn", IDS_SCREEN_LOCK_ACTIVE_USER);
  builder->Add("launchAppButton", IDS_LAUNCH_APP_BUTTON);
  builder->Add("restart", IDS_RESTART_BUTTON);
  builder->Add("shutDown", IDS_SHUTDOWN_BUTTON);
  builder->Add("addUser", IDS_ADD_USER_BUTTON);
  builder->Add("browseAsGuest", IDS_BROWSE_AS_GUEST_BUTTON);
  builder->Add("moreOptions", IDS_MORE_OPTIONS_BUTTON);
  builder->Add("addSupervisedUser",
               IDS_CREATE_LEGACY_SUPERVISED_USER_MENU_LABEL);
  builder->Add("cancel", IDS_CANCEL);
  builder->Add("signOutUser", IDS_SCREEN_LOCK_SIGN_OUT);
  builder->Add("unlockUser", IDS_SCREEN_LOCK_UNLOCK_USER_BUTTON);
  builder->Add("offlineLogin", IDS_OFFLINE_LOGIN_HTML);
  builder->Add("ownerUserPattern", IDS_LOGIN_POD_OWNER_USER);
  builder->Add("removeUser", IDS_LOGIN_POD_REMOVE_USER);
  builder->Add("errorTpmFailureTitle", IDS_LOGIN_ERROR_TPM_FAILURE_TITLE);
  builder->Add("errorTpmFailureReboot", IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT);
  builder->Add("errorTpmFailureRebootButton",
               IDS_LOGIN_ERROR_TPM_FAILURE_REBOOT_BUTTON);

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  builder->Add("disabledAddUserTooltip",
               connector->IsEnterpriseManaged()
                   ? IDS_DISABLED_ADD_USER_TOOLTIP_ENTERPRISE
                   : IDS_DISABLED_ADD_USER_TOOLTIP);

  builder->Add("supervisedUserExpiredTokenWarning",
               IDS_SUPERVISED_USER_EXPIRED_TOKEN_WARNING);
  builder->Add("signinBannerText", IDS_LOGIN_USER_ADDING_BANNER);

  // Multi-profiles related strings.
  builder->Add("multiProfilesRestrictedPolicyTitle",
               IDS_MULTI_PROFILES_RESTRICTED_POLICY_TITLE);
  builder->Add("multiProfilesNotAllowedPolicyMsg",
               IDS_MULTI_PROFILES_NOT_ALLOWED_POLICY_MSG);
  builder->Add("multiProfilesPrimaryOnlyPolicyMsg",
               IDS_MULTI_PROFILES_PRIMARY_ONLY_POLICY_MSG);
  builder->Add("multiProfilesOwnerPrimaryOnlyMsg",
               IDS_MULTI_PROFILES_OWNER_PRIMARY_ONLY_MSG);

  // Strings used by password changed dialog.
  builder->Add("oldPasswordHint", IDS_LOGIN_PASSWORD_CHANGED_OLD_PASSWORD_HINT);
  builder->Add("oldPasswordIncorrect",
               IDS_LOGIN_PASSWORD_CHANGED_INCORRECT_OLD_PASSWORD);
  builder->Add("proceedAnywayButton",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY_BUTTON);
  builder->Add("nextButtonText", IDS_OFFLINE_LOGIN_NEXT_BUTTON_TEXT);
  builder->Add("forgotOldPasswordButtonText",
               IDS_LOGIN_PASSWORD_CHANGED_FORGOT_PASSWORD);
  builder->AddF("passwordChangedTitle", IDS_LOGIN_PASSWORD_CHANGED_TITLE,
                ash::GetChromeOSDeviceName());
  builder->Add("passwordChangedProceedAnywayTitle",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY);
  builder->Add("passwordChangedTryAgain", IDS_LOGIN_PASSWORD_CHANGED_TRY_AGAIN);
  builder->Add("publicAccountInfoFormat", IDS_LOGIN_PUBLIC_ACCOUNT_INFO_FORMAT);
  builder->Add("publicAccountReminder",
               IDS_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER);
  builder->Add("publicSessionLanguageAndInput",
               IDS_LOGIN_PUBLIC_SESSION_LANGUAGE_AND_INPUT);
  builder->Add("publicAccountEnter", IDS_LOGIN_PUBLIC_ACCOUNT_ENTER);
  builder->Add("publicAccountEnterAccessibleName",
               IDS_LOGIN_PUBLIC_ACCOUNT_ENTER_ACCESSIBLE_NAME);
  builder->Add("publicAccountMonitoringWarning",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_WARNING);
  builder->Add("publicAccountLearnMore", IDS_LOGIN_PUBLIC_ACCOUNT_LEARN_MORE);
  builder->Add("publicAccountMonitoringInfo",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO);
  builder->Add("publicAccountMonitoringInfoItem1",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_1);
  builder->Add("publicAccountMonitoringInfoItem2",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_2);
  builder->Add("publicAccountMonitoringInfoItem3",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_3);
  builder->Add("publicAccountMonitoringInfoItem4",
               IDS_LOGIN_PUBLIC_ACCOUNT_MONITORING_INFO_ITEM_4);
  builder->Add("publicSessionSelectLanguage", IDS_LANGUAGE_SELECTION_SELECT);
  builder->Add("publicSessionSelectKeyboard", IDS_KEYBOARD_SELECTION_SELECT);
  builder->Add("removeUserWarningTextNonSyncNoStats", base::string16());
  builder->Add("removeUserWarningTextNonSyncCalculating", base::string16());
  builder->Add("removeUserWarningTextHistory", base::string16());
  builder->Add("removeUserWarningTextPasswords", base::string16());
  builder->Add("removeUserWarningTextBookmarks", base::string16());
  builder->Add("removeUserWarningTextAutofill", base::string16());
  builder->Add("removeUserWarningTextCalculating", base::string16());
  builder->Add("removeUserWarningTextSyncNoStats", base::string16());
  builder->Add("removeUserWarningTextSyncCalculating", base::string16());
  builder->AddF("removeLegacySupervisedUserWarningText",
               IDS_LOGIN_POD_LEGACY_SUPERVISED_USER_REMOVE_WARNING,
               base::UTF8ToUTF16(
                   chrome::kLegacySupervisedUserManagementDisplayURL));
  builder->Add("removeNonOwnerUserWarningText",
               IDS_LOGIN_POD_NON_OWNER_USER_REMOVE_WARNING);
  builder->Add("removeUserWarningButtonTitle",
               IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON);
  builder->Add("samlNotice", IDS_LOGIN_SAML_NOTICE);
  builder->Add("samlNoticeWithVideo", IDS_LOGIN_SAML_NOTICE_WITH_VIDEO);
  builder->AddF("confirmPasswordTitle", IDS_LOGIN_CONFIRM_PASSWORD_TITLE,
                ash::GetChromeOSDeviceName());
  builder->Add("manualPasswordTitle", IDS_LOGIN_MANUAL_PASSWORD_TITLE);
  builder->Add("manualPasswordInputLabel",
               IDS_LOGIN_MANUAL_PASSWORD_INPUT_LABEL);
  builder->Add("manualPasswordMismatch",
               IDS_LOGIN_MANUAL_PASSWORD_MISMATCH);
  builder->Add("confirmPasswordLabel", IDS_LOGIN_CONFIRM_PASSWORD_LABEL);
  builder->Add("confirmPasswordIncorrectPassword",
               IDS_LOGIN_CONFIRM_PASSWORD_INCORRECT_PASSWORD);
  builder->Add("accountSetupCancelDialogTitle",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_TITLE);
  builder->Add("accountSetupCancelDialogNo",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_NO);
  builder->Add("accountSetupCancelDialogYes",
               IDS_LOGIN_ACCOUNT_SETUP_CANCEL_DIALOG_YES);

  builder->Add("fatalEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR);
  builder->Add("insecureURLEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_INSECURE_URL_ERROR);

  builder->Add("unrecoverableCryptohomeErrorMessageTitle",
               IDS_LOGIN_UNRECOVERABLE_CRYPTOHOME_ERROR_TITLE);
  builder->Add("unrecoverableCryptohomeErrorMessage",
               IDS_LOGIN_UNRECOVERABLE_CRYPTOHOME_ERROR_MESSAGE);
  builder->Add("unrecoverableCryptohomeErrorContinue",
               IDS_LOGIN_UNRECOVERABLE_CRYPTOHOME_ERROR_CONTINUE);
  builder->Add("unrecoverableCryptohomeErrorRecreatingProfile",
               IDS_LOGIN_UNRECOVERABLE_CRYPTOHOME_ERROR_WAIT_MESSAGE);

  builder->Add("adEnterOldPasswordHint", IDS_AD_PASSWORD_CHANGE_OLD_PASSWORD);
  builder->Add("adEnterNewPasswordHint", IDS_AD_PASSWORD_CHANGE_NEW_PASSWORD);
  builder->Add("adRepeatNewPasswordHint",
               IDS_AD_PASSWORD_CHANGE_REPEAT_NEW_PASSWORD);
  builder->Add("adPasswordChangeMessage", IDS_AD_PASSWORD_CHANGE_MESSAGE);
  builder->Add("adOldPasswordError", IDS_AD_PASSWORD_CHANGE_INVALID_PASSWORD);
  builder->Add("adNewPasswordError", IDS_AD_PASSWORD_CHANGE_PASSWORDS_MISMATCH);
}

void SigninScreenHandler::RegisterMessages() {
  AddCallback("authenticateUser", &SigninScreenHandler::HandleAuthenticateUser);
  AddCallback("launchIncognito", &SigninScreenHandler::HandleLaunchIncognito);
  AddCallback("showSupervisedUserCreationScreen",
              &SigninScreenHandler::HandleShowSupervisedUserCreationScreen);
  AddCallback("launchPublicSession",
              &SigninScreenHandler::HandleLaunchPublicSession);
  AddRawCallback("offlineLogin", &SigninScreenHandler::HandleOfflineLogin);
  AddCallback("rebootSystem", &SigninScreenHandler::HandleRebootSystem);
  AddRawCallback("showAddUser", &SigninScreenHandler::HandleShowAddUser);
  AddCallback("shutdownSystem", &SigninScreenHandler::HandleShutdownSystem);
  AddCallback("loadWallpaper", &SigninScreenHandler::HandleLoadWallpaper);
  AddCallback("removeUser", &SigninScreenHandler::HandleRemoveUser);
  AddCallback("toggleEnrollmentScreen",
              &SigninScreenHandler::HandleToggleEnrollmentScreen);
  AddCallback("toggleEnableDebuggingScreen",
              &SigninScreenHandler::HandleToggleEnableDebuggingScreen);
  AddCallback("toggleKioskEnableScreen",
              &SigninScreenHandler::HandleToggleKioskEnableScreen);
  AddCallback("accountPickerReady",
              &SigninScreenHandler::HandleAccountPickerReady);
  AddCallback("wallpaperReady", &SigninScreenHandler::HandleWallpaperReady);
  AddCallback("signOutUser", &SigninScreenHandler::HandleSignOutUser);
  AddCallback("openProxySettings",
              &SigninScreenHandler::HandleOpenProxySettings);
  AddCallback("loginVisible", &SigninScreenHandler::HandleLoginVisible);
  AddCallback("cancelPasswordChangedFlow",
              &SigninScreenHandler::HandleCancelPasswordChangedFlow);
  AddCallback("cancelUserAdding", &SigninScreenHandler::HandleCancelUserAdding);
  AddCallback("migrateUserData", &SigninScreenHandler::HandleMigrateUserData);
  AddCallback("resyncUserData", &SigninScreenHandler::HandleResyncUserData);
  AddCallback("loginUIStateChanged",
              &SigninScreenHandler::HandleLoginUIStateChanged);
  AddCallback("unlockOnLoginSuccess",
              &SigninScreenHandler::HandleUnlockOnLoginSuccess);
  AddCallback("showLoadingTimeoutError",
              &SigninScreenHandler::HandleShowLoadingTimeoutError);
  AddCallback("focusPod", &SigninScreenHandler::HandleFocusPod);
  AddCallback("noPodFocused", &SigninScreenHandler::HandleNoPodFocused);
  AddCallback("getPublicSessionKeyboardLayouts",
              &SigninScreenHandler::HandleGetPublicSessionKeyboardLayouts);
  AddCallback("getTouchViewState",
              &SigninScreenHandler::HandleGetTouchViewState);
  AddCallback("logRemoveUserWarningShown",
              &SigninScreenHandler::HandleLogRemoveUserWarningShown);
  AddCallback("firstIncorrectPasswordAttempt",
              &SigninScreenHandler::HandleFirstIncorrectPasswordAttempt);
  AddCallback("maxIncorrectPasswordAttempts",
              &SigninScreenHandler::HandleMaxIncorrectPasswordAttempts);
  AddCallback("sendFeedbackAndResyncUserData",
              &SigninScreenHandler::HandleSendFeedbackAndResyncUserData);

  // This message is sent by the kiosk app menu, but is handled here
  // so we can tell the delegate to launch the app.
  AddCallback("launchKioskApp", &SigninScreenHandler::HandleLaunchKioskApp);
  AddCallback("launchArcKioskApp",
              &SigninScreenHandler::HandleLaunchArcKioskApp);
  AddCallback("setLockScreenAppsState",
              &SigninScreenHandler::HandleSetLockScreenAppsState);
}

void SigninScreenHandler::Show(const LoginScreenContext& context) {
  CHECK(delegate_);

  // Just initialize internal fields from context and call ShowImpl().
  oobe_ui_ = context.oobe_ui();

  std::string email;
  email = context.email();
  gaia_screen_handler_->set_populated_email(email);
  ShowImpl();
  histogram_helper_->OnScreenShow();
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_)
    delegate_->SetWebUIHandler(this);
}

void SigninScreenHandler::SetNativeWindowDelegate(
    NativeWindowDelegate* native_window_delegate) {
  native_window_delegate_ = native_window_delegate;
}

void SigninScreenHandler::OnNetworkReady() {
  VLOG(1) << "OnNetworkReady() call.";
  gaia_screen_handler_->MaybePreloadAuthExtension();
}

void SigninScreenHandler::UpdateState(NetworkError::ErrorReason reason) {
  // ERROR_REASON_FRAME_ERROR is an explicit signal from GAIA frame so it shoud
  // force network error UI update.
  bool force_update = reason == NetworkError::ERROR_REASON_FRAME_ERROR;
  UpdateStateInternal(reason, force_update);
}

void SigninScreenHandler::SetFocusPODCallbackForTesting(
    base::Closure callback) {
  test_focus_pod_callback_ = callback;
}

void SigninScreenHandler::ZeroOfflineTimeoutForTesting() {
  zero_offline_timeout_for_test_ = true;
}

bool SigninScreenHandler::GetKeyboardRemappedPrefValue(
    const std::string& pref_name,
    int* value) {
  return focused_pod_account_id_ && focused_pod_account_id_->is_valid() &&
         user_manager::known_user::GetIntegerPref(*focused_pod_account_id_,
                                                  pref_name, value);
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::ShowImpl() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  if (!ime_state_.get())
    ime_state_ = input_method::InputMethodManager::Get()->GetActiveIMEState();

  if (!oobe_ui_observer_added_) {
    oobe_ui_observer_added_ = true;
    GetOobeUI()->AddObserver(this);
  }

  if (oobe_ui_) {
    // Shows new user sign-in for OOBE.
    OnShowAddUser();
  } else {
    // Populates account picker. Animation is turned off for now until we
    // figure out how to make it fast enough. This will call LoadUsers.
    delegate_->HandleGetUsers();

    // Reset Caps Lock state when login screen is shown.
    input_method::InputMethodManager::Get()
        ->GetImeKeyboard()
        ->SetCapsLockEnabled(false);

    base::DictionaryValue params;
    params.SetBoolean("disableAddUser", AllWhitelistedUsersPresent());
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, &params);
  }

  // Enable pin for any users who can use it.
  if (user_manager::UserManager::IsInitialized()) {
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetLoggedInUsers()) {
      UpdatePinKeyboardState(user->GetAccountId());
    }
  }
}

void SigninScreenHandler::UpdateUIState(UIState ui_state,
                                        base::DictionaryValue* params) {
  switch (ui_state) {
    case UI_STATE_GAIA_SIGNIN:
      ui_state_ = UI_STATE_GAIA_SIGNIN;
      ShowScreenWithData(OobeScreen::SCREEN_GAIA_SIGNIN, params);
      break;
    case UI_STATE_ACCOUNT_PICKER:
      ui_state_ = UI_STATE_ACCOUNT_PICKER;
      gaia_screen_handler_->CancelShowGaiaAsync();
      ShowScreenWithData(OobeScreen::SCREEN_ACCOUNT_PICKER, params);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// TODO(antrim@): split this method into small parts.
// TODO(antrim@): move this logic to GaiaScreenHandler.
void SigninScreenHandler::UpdateStateInternal(NetworkError::ErrorReason reason,
                                              bool force_update) {
  // Do nothing once user has signed in or sign in is in progress.
  // TODO(antrim): We will end up here when processing network state
  // notification but no ShowSigninScreen() was called so delegate_ will be
  // nullptr. Network state processing logic does not belong here.
  if (delegate_ &&
      (delegate_->IsUserSigninCompleted() || delegate_->IsSigninInProgress())) {
    return;
  }

  NetworkStateInformer::State state = network_state_informer_->state();
  const std::string network_path = network_state_informer_->network_path();
  const std::string network_name = GetNetworkName(network_path);

  // Skip "update" notification about OFFLINE state from
  // NetworkStateInformer if previous notification already was
  // delayed.
  if ((state == NetworkStateInformer::OFFLINE || has_pending_auth_ui_) &&
      !force_update && !update_state_closure_.IsCancelled()) {
    return;
  }

  update_state_closure_.Cancel();

  if ((state == NetworkStateInformer::OFFLINE && !force_update) ||
      has_pending_auth_ui_) {
    update_state_closure_.Reset(
        base::Bind(&SigninScreenHandler::UpdateStateInternal,
                   weak_factory_.GetWeakPtr(),
                   reason,
                   true));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, update_state_closure_.callback(),
        base::TimeDelta::FromSeconds(
            zero_offline_timeout_for_test_ ? 0 : kOfflineTimeoutSec));
    return;
  }

  // Don't show or hide error screen if we're in connecting state.
  if (state == NetworkStateInformer::CONNECTING && !force_update) {
    if (connecting_closure_.IsCancelled()) {
      // First notification about CONNECTING state.
      connecting_closure_.Reset(
          base::Bind(&SigninScreenHandler::UpdateStateInternal,
                     weak_factory_.GetWeakPtr(),
                     reason,
                     true));
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, connecting_closure_.callback(),
          base::TimeDelta::FromSeconds(kConnectingTimeoutSec));
    }
    return;
  }
  connecting_closure_.Cancel();

  const bool is_online = IsOnline(state, reason);
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);
  const bool is_gaia_error =
      FrameError() != net::OK && FrameError() != net::ERR_NETWORK_CHANGED;
  const bool is_gaia_signin = IsGaiaVisible() || IsGaiaHiddenByError();
  const bool offline_login_active =
      gaia_screen_handler_->offline_login_is_active();
  const bool error_screen_should_overlay =
      !offline_login_active && IsGaiaVisible();
  const bool from_not_online_to_online_transition =
      is_online && last_network_state_ != NetworkStateInformer::ONLINE;
  last_network_state_ = state;
  proxy_auth_dialog_need_reload_ =
      (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED) &&
      (state == NetworkStateInformer::PROXY_AUTH_REQUIRED) &&
      (proxy_auth_dialog_reload_times_ > 0);

  CallOnReturn reload_gaia(base::Bind(
      &SigninScreenHandler::ReloadGaia, weak_factory_.GetWeakPtr(), true));

  if (is_online || !is_behind_captive_portal)
    error_screen_->HideCaptivePortal();

  // Hide offline message (if needed) and return if current screen is
  // not a Gaia frame.
  if (!is_gaia_signin) {
    if (!IsSigninScreenHiddenByError())
      HideOfflineMessage(state, reason);
    return;
  }

  // Use the online login page if the user has not used the machine for awhile.
  if (offline_login_active)
    gaia_screen_handler_->MonitorOfflineIdle(is_online);

  // Reload frame if network state is changed from {!ONLINE} -> ONLINE state.
  if (reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED &&
      from_not_online_to_online_transition) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frame load since network has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_PROXY_CONFIG_CHANGED &&
      error_screen_should_overlay) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry frameload since proxy settings has been changed.";
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (reason == NetworkError::ERROR_REASON_FRAME_ERROR &&
      reason != gaia_reload_reason_ &&
      !IsProxyError(state, reason, FrameError())) {
    LOG(WARNING) << "Retry frame load due to reason: "
                 << NetworkError::ErrorReasonString(reason);
    gaia_reload_reason_ = reason;
    reload_gaia.ScheduleCall();
  }

  if (is_gaia_loading_timeout) {
    LOG(WARNING) << "Retry frame load due to loading timeout.";
    LOG(ERROR) << "UpdateStateInternal reload 4";
    reload_gaia.ScheduleCall();
  }

  if (proxy_auth_dialog_need_reload_) {
    --proxy_auth_dialog_reload_times_;
    LOG(WARNING) << "Retry frame load to show proxy auth dialog";
    reload_gaia.ScheduleCall();
  }

  if ((!is_online || is_gaia_loading_timeout || is_gaia_error) &&
      !offline_login_active) {
    SetupAndShowOfflineMessage(state, reason);
  } else {
    HideOfflineMessage(state, reason);

    // Cancel scheduled GAIA reload (if any) to prevent double reloads.
    reload_gaia.CancelScheduledCall();
  }
}

void SigninScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer::State state,
    NetworkError::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = IsProxyError(state, reason, FrameError());
  const bool is_gaia_loading_timeout =
      (reason == NetworkError::ERROR_REASON_LOADING_TIMEOUT);

  if (is_proxy_error) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PROXY,
                                 std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsGaiaVisible() ||
        (error_screen_->GetErrorState() != NetworkError::ERROR_STATE_PORTAL)) {
      error_screen_->FixCaptivePortal();
    }
    const std::string network_name = GetNetworkName(network_path);
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_PORTAL,
                                 network_name);
  } else if (is_gaia_loading_timeout) {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT,
                                 std::string());
  } else {
    error_screen_->SetErrorState(NetworkError::ERROR_STATE_OFFLINE,
                                 std::string());
  }

  const bool guest_signin_allowed =
      IsGuestSigninAllowed() &&
      IsSigninScreenError(error_screen_->GetErrorState());
  error_screen_->AllowGuestSignin(guest_signin_allowed);

  const bool offline_login_allowed =
      IsSigninScreenError(error_screen_->GetErrorState()) &&
      error_screen_->GetErrorState() !=
          NetworkError::ERROR_STATE_AUTH_EXT_TIMEOUT;
  error_screen_->AllowOfflineLogin(offline_login_allowed);

  if (GetCurrentScreen() != OobeScreen::SCREEN_ERROR_MESSAGE) {
    error_screen_->SetUIState(NetworkError::UI_STATE_SIGNIN);
    error_screen_->SetParentScreen(OobeScreen::SCREEN_GAIA_SIGNIN);
    error_screen_->Show();
    histogram_helper_->OnErrorShow(error_screen_->GetErrorState());
  }
}

void SigninScreenHandler::HideOfflineMessage(NetworkStateInformer::State state,
                                             NetworkError::ErrorReason reason) {
  if (!IsSigninScreenHiddenByError())
    return;

  gaia_reload_reason_ = NetworkError::ERROR_REASON_NONE;

  error_screen_->Hide();
  histogram_helper_->OnErrorHide();

  // Forces a reload for Gaia screen on hiding error message.
  if (IsGaiaVisible() || IsGaiaHiddenByError())
    ReloadGaia(reason == NetworkError::ERROR_REASON_NETWORK_STATE_CHANGED);
}

void SigninScreenHandler::ReloadGaia(bool force_reload) {
  gaia_screen_handler_->ReloadGaia(force_reload);
}

void SigninScreenHandler::SetSigninScreenColors(SkColor dark_muted_color) {
  // The dark muted color should have 100% opacity.
  dark_muted_color = SkColorSetA(dark_muted_color, 0xFF);
  SkColor base_color = color_utils::GetResultingPaintColor(
      SkColorSetA(ash::kShelfDefaultBaseColor,
                  ash::kShelfTranslucentColorDarkenAlpha),
      dark_muted_color);
  SkColor background_color =
      SkColorSetA(base_color, ash::kShelfTranslucentAlpha);
  SkColor scroll_color = SkColorSetA(base_color, kLoginTranslucentAlpha);
  CallJS("login.AccountPickerScreen.setOverlayColors",
         color_utils::SkColorToRgbaString(dark_muted_color),
         color_utils::SkColorToRgbaString(scroll_color),
         color_utils::SkColorToRgbaString(background_color));
}

void SigninScreenHandler::Initialize() {
  // Preload PIN keyboard if any of the users can authenticate via PIN.
  if (user_manager::UserManager::IsInitialized()) {
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetUnlockUsers()) {
      chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
          chromeos::quick_unlock::QuickUnlockFactory::GetForUser(user);
      if (quick_unlock_storage &&
          quick_unlock_storage->IsPinAuthenticationAvailable()) {
        CallJS("cr.ui.Oobe.preloadPinKeyboard");
        break;
      }
    }
  }

  // |delegate_| is null when we are preloading the lock screen.
  if (delegate_ && show_on_init_) {
    show_on_init_ = false;
    ShowImpl();
  }
}

gfx::NativeWindow SigninScreenHandler::GetNativeWindow() {
  if (native_window_delegate_)
    return native_window_delegate_->GetNativeWindow();
  return nullptr;
}

void SigninScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUsersLastInputMethod);
}

void SigninScreenHandler::OnCurrentScreenChanged(OobeScreen current_screen,
                                                 OobeScreen new_screen) {
  if (new_screen == OobeScreen::SCREEN_ACCOUNT_PICKER) {
    // Restore active IME state if returning to user pod row screen.
    input_method::InputMethodManager::Get()->SetState(ime_state_);
  }
}

void SigninScreenHandler::OnWallpaperColorsChanged() {
  base::Optional<SkColor> color = WallpaperManager::Get()->prominent_color();
  // If color extraction fails, use transparent as default.
  if (!color.has_value())
    color = SK_ColorTRANSPARENT;
  SetSigninScreenColors(color.value());
}

void SigninScreenHandler::OnWallpaperAnimationFinished(
    const AccountId& account_id) {}

void SigninScreenHandler::ClearAndEnablePassword() {
  core_oobe_view_->ResetSignInUI(false);
}

void SigninScreenHandler::ClearUserPodPassword() {
  core_oobe_view_->ClearUserPodPassword();
}

void SigninScreenHandler::RefocusCurrentPod() {
  core_oobe_view_->RefocusCurrentPod();
}

void SigninScreenHandler::UpdatePinKeyboardState(const AccountId& account_id) {
  chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      chromeos::quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  if (!quick_unlock_storage)
    return;

  bool is_enabled = quick_unlock_storage->IsPinAuthenticationAvailable();
  CallJS("login.AccountPickerScreen.setPinEnabledForUser", account_id,
         is_enabled);
}

void SigninScreenHandler::OnUserRemoved(const AccountId& account_id,
                                        bool last_user_removed) {
  CallJS("login.AccountPickerScreen.removeUser", account_id);
  if (last_user_removed)
    OnShowAddUser();
}

void SigninScreenHandler::OnUserImageChanged(const user_manager::User& user) {
  if (page_is_ready()) {
    CallJSOrDefer("login.AccountPickerScreen.updateUserImage",
                  user.GetAccountId());
  }
}

void SigninScreenHandler::OnPreferencesChanged() {
  // Make sure that one of the login UI is fully functional now, otherwise
  // preferences update would be picked up next time it will be shown.
  if (!webui_visible_) {
    LOG(WARNING) << "Login UI is not active - postponed prefs change.";
    preferences_changed_delayed_ = true;
    return;
  }

  preferences_changed_delayed_ = false;

  if (!delegate_)
    return;

  // Send the updated user list to the UI.
  delegate_->HandleGetUsers();
  if (GetCurrentScreen() == OobeScreen::SCREEN_ACCOUNT_PICKER &&
      delegate_->ShowUsersHasChanged() &&
      !delegate_->IsShowUsers()) {
    // We are at the account picker screen and the POD setting has changed
    // to be disabled. We need to show the add user page.
    HandleShowAddUser(nullptr);
    return;
  }

  if (delegate_->AllowNewUserChanged() || ui_state_ == UI_STATE_UNKNOWN) {
    // We need to reload GAIA if UI_STATE_UNKNOWN or the allow new user setting
    // has changed so that reloaded GAIA shows/hides the option to create a new
    // account.
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, nullptr);
    UpdateAddButtonStatus();
  }
}

void SigninScreenHandler::ResetSigninScreenHandlerDelegate() {
  SetDelegate(nullptr);
}

void SigninScreenHandler::ShowError(int login_attempts,
                                    const std::string& error_text,
                                    const std::string& help_link_text,
                                    HelpAppLauncher::HelpTopic help_topic_id) {
  core_oobe_view_->ShowSignInError(login_attempts, error_text, help_link_text,
                                   help_topic_id);
}

void SigninScreenHandler::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  switch (error_id) {
    case LoginDisplay::TPM_ERROR:
      core_oobe_view_->ShowTpmError();
      break;
    default:
      NOTREACHED() << "Unknown sign in error";
      break;
  }
}

void SigninScreenHandler::ShowSigninUI(const std::string& email) {
  core_oobe_view_->ShowSignInUI(email);
}

void SigninScreenHandler::ShowPasswordChangedDialog(bool show_password_error,
                                                    const std::string& email) {
  core_oobe_view_->ShowPasswordChangedScreen(show_password_error, email);
}

void SigninScreenHandler::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  gaia_screen_handler_->ShowSigninScreenForTest(username, password);
}

void SigninScreenHandler::ShowWhitelistCheckFailedError() {
  gaia_screen_handler_->ShowWhitelistCheckFailedError();
}

void SigninScreenHandler::ShowUnrecoverableCrypthomeErrorDialog() {
  CallJS("login.UnrecoverableCryptohomeErrorScreen.show");
}

void SigninScreenHandler::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_AUTH_NEEDED: {
      has_pending_auth_ui_ = true;
      break;
    }
    case chrome::NOTIFICATION_AUTH_SUPPLIED:
      has_pending_auth_ui_ = false;
      // Reload auth extension as proxy credentials are supplied.
      if (!IsSigninScreenHiddenByError() && ui_state_ == UI_STATE_GAIA_SIGNIN)
        ReloadGaia(true);
      update_state_closure_.Cancel();
      break;
    case chrome::NOTIFICATION_AUTH_CANCELLED: {
      // Don't reload auth extension if proxy auth dialog was cancelled.
      has_pending_auth_ui_ = false;
      update_state_closure_.Cancel();
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void SigninScreenHandler::SuspendDone(const base::TimeDelta& sleep_duration) {
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetUnlockUsers()) {
    UpdatePinKeyboardState(user->GetAccountId());
  }
}

void SigninScreenHandler::OnTouchViewToggled(bool enabled) {
  touch_view_enabled_ = enabled;
  CallJSOrDefer("login.AccountPickerScreen.setTouchViewState", enabled);
}

void SigninScreenHandler::OnLockScreenNoteStateChanged(
    ash::mojom::TrayActionState state) {
  std::string lock_screen_apps_state;
  switch (state) {
    case ash::mojom::TrayActionState::kActive:
      lock_screen_apps_state = kForegroundLockScreenApps;
      break;
    case ash::mojom::TrayActionState::kBackground:
      lock_screen_apps_state = kBackgroundLockScreenApps;
      break;
    case ash::mojom::TrayActionState::kAvailable:
    case ash::mojom::TrayActionState::kNotAvailable:
    case ash::mojom::TrayActionState::kLaunching:
      lock_screen_apps_state = kNoLockScreenApps;
      break;
  }
  CallJSOrDefer("login.AccountPickerScreen.setLockScreenAppsState",
                lock_screen_apps_state);
}

bool SigninScreenHandler::ShouldLoadGaia() const {
  // Fetching of the extension is not started before account picker page is
  // loaded because it can affect the loading speed.
  // Do not load the extension for the screen locker, see crosbug.com/25018.
  return !ScreenLocker::default_screen_locker() &&
         is_account_picker_showing_first_time_;
}

void SigninScreenHandler::UpdateAddButtonStatus() {
  CallJS("cr.ui.login.DisplayManager.updateAddUserButtonStatus",
         AllWhitelistedUsersPresent());
}

void SigninScreenHandler::HandleAuthenticateUser(const AccountId& account_id,
                                                 const std::string& password,
                                                 bool authenticated_by_pin) {
  if (!delegate_)
    return;
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));
  chromeos::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      chromeos::quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  // If pin storage is unavailable, authenticated by PIN must be false.
  DCHECK(!quick_unlock_storage ||
         quick_unlock_storage->IsPinAuthenticationAvailable() ||
         !authenticated_by_pin);

  UserContext user_context(account_id);
  user_context.SetKey(Key(password));
  user_context.SetIsUsingPin(authenticated_by_pin);
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY)
    user_context.SetUserType(user_manager::USER_TYPE_ACTIVE_DIRECTORY);
  delegate_->Login(user_context, SigninSpecifics());

  UpdatePinKeyboardState(account_id);
}

void SigninScreenHandler::HandleLaunchIncognito() {
  UserContext context(user_manager::USER_TYPE_GUEST, EmptyAccountId());
  if (delegate_)
    delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleShowSupervisedUserCreationScreen() {
  if (!user_manager::UserManager::Get()->AreSupervisedUsersAllowed()) {
    LOG(ERROR) << "Managed users not allowed.";
    return;
  }
  LoginDisplayHost::default_host()->StartWizard(
      OobeScreen::SCREEN_CREATE_SUPERVISED_USER_FLOW);
}

void SigninScreenHandler::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  if (!delegate_)
    return;

  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, account_id);
  context.SetPublicSessionLocale(locale),
  context.SetPublicSessionInputMethod(input_method);
  delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleOfflineLogin(const base::ListValue* args) {
  if (!delegate_) {
    NOTREACHED();
    return;
  }
  std::string email;
  args->GetString(0, &email);

  gaia_screen_handler_->set_populated_email(email);
  gaia_screen_handler_->LoadAuthExtension(true /* force */, true /* offline */);
  UpdateUIState(UI_STATE_GAIA_SIGNIN, nullptr);
}

void SigninScreenHandler::HandleShutdownSystem() {
  ash::Shell::Get()->lock_state_controller()->RequestShutdown();
}

void SigninScreenHandler::HandleLoadWallpaper(const AccountId& account_id) {
  if (delegate_)
    delegate_->LoadWallpaper(account_id);
}

void SigninScreenHandler::HandleRebootSystem() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void SigninScreenHandler::HandleRemoveUser(const AccountId& account_id) {
  if (delegate_ &&
      (delegate_->IsUserSigninCompleted() || delegate_->IsSigninInProgress())) {
    return;
  }

  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);

  if (!delegate_)
    return;
  delegate_->RemoveUser(account_id);
  UpdateAddButtonStatus();
}

void SigninScreenHandler::HandleShowAddUser(const base::ListValue* args) {
  TRACE_EVENT_ASYNC_STEP_INTO0("ui",
                               "ShowLoginWebUI",
                               LoginDisplayHostImpl::kShowLoginWebUIid,
                               "ShowAddUser");
  std::string email;
  // |args| can be null if it's OOBE.
  if (args)
    args->GetString(0, &email);
  gaia_screen_handler_->set_populated_email(email);
  if (!email.empty())
    SendReauthReason(AccountId::FromUserEmail(email));
  OnShowAddUser();
}

void SigninScreenHandler::HandleToggleEnrollmentScreen() {
  if (delegate_)
    delegate_->ShowEnterpriseEnrollmentScreen();
}

void SigninScreenHandler::HandleToggleEnableDebuggingScreen() {
  if (delegate_)
    delegate_->ShowEnableDebuggingScreen();
}

void SigninScreenHandler::HandleToggleKioskEnableScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (delegate_ && !connector->IsEnterpriseManaged() &&
      KioskAppManager::IsConsumerKioskEnabled() &&
      LoginDisplayHost::default_host()) {
    delegate_->ShowKioskEnableScreen();
  }
}

void SigninScreenHandler::HandleToggleKioskAutolaunchScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (delegate_ && !connector->IsEnterpriseManaged())
    delegate_->ShowKioskAutolaunchScreen();
}

void SigninScreenHandler::LoadUsers(const user_manager::UserList& users,
                                    const base::ListValue& users_list) {
  CallJSOrDefer("login.AccountPickerScreen.loadUsers", users_list,
                delegate_->IsShowGuest());
}

void SigninScreenHandler::HandleAccountPickerReady() {
  VLOG(0) << "Login WebUI >> AccountPickerReady";

  if (delegate_ && !ScreenLocker::default_screen_locker() &&
      !chromeos::IsMachineHWIDCorrect() &&
      !oobe_ui_) {
    delegate_->ShowWrongHWIDScreen();
    return;
  }

  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kFactoryResetRequested)) {
    if (core_oobe_view_)
      core_oobe_view_->ShowDeviceResetScreen();

    return;
  } else if (prefs->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
    if (core_oobe_view_)
      core_oobe_view_->ShowEnableDebuggingScreen();

    return;
  }

  is_account_picker_showing_first_time_ = true;

  if (ScreenLocker::default_screen_locker() &&
      lock_screen_apps::StateController::IsEnabled()) {
    OnLockScreenNoteStateChanged(
        lock_screen_apps::StateController::Get()->GetLockScreenNoteState());
  }
  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void SigninScreenHandler::HandleWallpaperReady() {
  if (ScreenLocker::default_screen_locker()) {
    ScreenLocker::default_screen_locker()
        ->delegate()
        ->OnLockBackgroundDisplayed();
  }
}

void SigninScreenHandler::HandleSignOutUser() {
  if (delegate_)
    delegate_->Signout();
}

void SigninScreenHandler::HandleOpenProxySettings() {
  LoginDisplayHost::default_host()->OpenProxySettings();
}

void SigninScreenHandler::HandleLoginVisible(const std::string& source) {
  VLOG(1) << "Login WebUI >> loginVisible, src: " << source << ", "
          << "webui_visible_: " << webui_visible_;
  if (!webui_visible_) {
    // There might be multiple messages from OOBE UI so send notifications after
    // the first one only.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
    TRACE_EVENT_ASYNC_END0(
        "ui", "ShowLoginWebUI", LoginDisplayHostImpl::kShowLoginWebUIid);
  }
  webui_visible_ = true;
  if (preferences_changed_delayed_)
    OnPreferencesChanged();
  OnAllowedInputMethodsChanged();
}

void SigninScreenHandler::HandleCancelPasswordChangedFlow(
    const AccountId& account_id) {
  if (account_id.is_valid()) {
    RecordReauthReason(account_id, ReauthReason::PASSWORD_UPDATE_SKIPPED);
  }
  gaia_screen_handler_->StartClearingCookies(
      base::Bind(&SigninScreenHandler::CancelPasswordChangedFlowInternal,
                 weak_factory_.GetWeakPtr()));
}

void SigninScreenHandler::HandleCancelUserAdding() {
  if (delegate_)
    delegate_->CancelUserAdding();
}

void SigninScreenHandler::HandleMigrateUserData(
    const std::string& old_password) {
  if (delegate_)
    delegate_->MigrateUserData(old_password);
}

void SigninScreenHandler::HandleResyncUserData() {
  if (delegate_)
    delegate_->ResyncUserData();
}

void SigninScreenHandler::HandleLoginUIStateChanged(const std::string& source,
                                                    bool active) {
  VLOG(0) << "Login WebUI >> active: " << active << ", "
            << "source: " << source;

  if (!KioskAppManager::Get()->GetAutoLaunchApp().empty() &&
      KioskAppManager::Get()->IsAutoLaunchRequested()) {
    VLOG(0) << "Showing auto-launch warning";
    // On slow devices, the wallpaper animation is not shown initially, so we
    // must explicitly load the wallpaper. This is also the case for the
    // account-picker and gaia-signin UI states.
    delegate_->LoadSigninWallpaper();
    HandleToggleKioskAutolaunchScreen();
    return;
  }

  if (source == kSourceGaiaSignin) {
    ui_state_ = UI_STATE_GAIA_SIGNIN;
  } else if (source == kSourceAccountPicker) {
    ui_state_ = UI_STATE_ACCOUNT_PICKER;
  } else {
    NOTREACHED();
    return;
  }
}

void SigninScreenHandler::HandleUnlockOnLoginSuccess() {
  DCHECK(user_manager::UserManager::Get()->IsUserLoggedIn());
  if (ScreenLocker::default_screen_locker())
    ScreenLocker::default_screen_locker()->UnlockOnLoginSuccess();
}

void SigninScreenHandler::HandleShowLoadingTimeoutError() {
  UpdateState(NetworkError::ERROR_REASON_LOADING_TIMEOUT);
}

void SigninScreenHandler::HandleFocusPod(const AccountId& account_id) {
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id);
  if (delegate_)
    delegate_->CheckUserStatus(account_id);
  if (!test_focus_pod_callback_.is_null())
    test_focus_pod_callback_.Run();

  focused_pod_account_id_ = base::MakeUnique<AccountId>(account_id);

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // |user| may be nullptr in kiosk mode or unit tests.
  if (user && user->is_logged_in() && !user->is_active()) {
    SessionControllerClient::DoSwitchActiveUser(account_id);
  } else {
    lock_screen_utils::SetUserInputMethod(account_id.GetUserEmail(),
                                          ime_state_.get());
    lock_screen_utils::SetKeyboardSettings(account_id);
    WallpaperManager::Get()->SetUserWallpaperDelayed(account_id);

    bool use_24hour_clock = false;
    if (user_manager::known_user::GetBooleanPref(
            account_id, prefs::kUse24HourClock, &use_24hour_clock)) {
      g_browser_process->platform_part()
          ->GetSystemClock()
          ->SetLastFocusedPodHourClockType(
              use_24hour_clock ? base::k24HourClock : base::k12HourClock);
    }
  }
}

void SigninScreenHandler::HandleNoPodFocused() {
  focused_pod_account_id_.reset();
  lock_screen_utils::EnforcePolicyInputMethods(std::string());
}

void SigninScreenHandler::HandleGetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  GetKeyboardLayoutsForLocale(
      base::Bind(&SigninScreenHandler::SendPublicSessionKeyboardLayouts,
                 weak_factory_.GetWeakPtr(), account_id, locale),
      locale);
}

void SigninScreenHandler::SendPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    std::unique_ptr<base::ListValue> keyboard_layouts) {
  CallJS("login.AccountPickerScreen.setPublicSessionKeyboardLayouts",
         account_id, locale, *keyboard_layouts);
}

void SigninScreenHandler::HandleLaunchKioskApp(const AccountId& app_account_id,
                                               bool diagnostic_mode) {
  UserContext context(user_manager::USER_TYPE_KIOSK_APP, app_account_id);
  SigninSpecifics specifics;
  specifics.kiosk_diagnostic_mode = diagnostic_mode;
  if (delegate_)
    delegate_->Login(context, specifics);
}

void SigninScreenHandler::HandleLaunchArcKioskApp(
    const AccountId& app_account_id) {
  UserContext context(user_manager::USER_TYPE_ARC_KIOSK_APP, app_account_id);
  if (delegate_)
    delegate_->Login(context, SigninSpecifics());
}

void SigninScreenHandler::HandleGetTouchViewState() {
  CallJS("login.AccountPickerScreen.setTouchViewState", touch_view_enabled_);
}

void SigninScreenHandler::HandleLogRemoveUserWarningShown() {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

void SigninScreenHandler::HandleFirstIncorrectPasswordAttempt(
    const AccountId& account_id) {
  // TODO(ginkage): Fix this case once crbug.com/469987 is ready.
  /*
    if (user_manager::known_user::IsUsingSAML(email))
      RecordReauthReason(email, ReauthReason::INCORRECT_SAML_PASSWORD_ENTERED);
  */
}

void SigninScreenHandler::HandleMaxIncorrectPasswordAttempts(
    const AccountId& account_id) {
  RecordReauthReason(account_id, ReauthReason::INCORRECT_PASSWORD_ENTERED);
}

void SigninScreenHandler::HandleSendFeedbackAndResyncUserData() {
  const std::string description = base::StringPrintf(
      "Auto generated feedback for http://crbug.com/547857.\n"
      "(uniquifier:%s)",
      base::Int64ToString(base::Time::Now().ToInternalValue()).c_str());

  login_feedback_.reset(new LoginFeedback(Profile::FromWebUI(web_ui())));
  login_feedback_->Request(description,
                           base::Bind(&SigninScreenHandler::OnFeedbackFinished,
                                      weak_factory_.GetWeakPtr()));
}

void SigninScreenHandler::HandleSetLockScreenAppsState(
    const std::string& state) {
  lock_screen_apps::StateController* state_controller =
      lock_screen_apps::StateController::Get();

  if (state == kBackgroundLockScreenApps)
    state_controller->MoveToBackground();
  else if (state == kForegroundLockScreenApps)
    state_controller->MoveToForeground();
}

bool SigninScreenHandler::AllWhitelistedUsersPresent() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return false;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::UserList& users = user_manager->GetUsers();
  if (!delegate_ || users.size() > kMaxUsers) {
    return false;
  }
  const base::ListValue* whitelist = nullptr;
  if (!cros_settings->GetList(kAccountsPrefUsers, &whitelist) || !whitelist)
    return false;
  for (size_t i = 0; i < whitelist->GetSize(); ++i) {
    std::string whitelisted_user;
    // NB: Wildcards in the whitelist are also detected as not present here.
    if (!whitelist->GetString(i, &whitelisted_user) ||
        !user_manager->IsKnownUser(
            AccountId::FromUserEmail(whitelisted_user))) {
      return false;
    }
  }
  return true;
}

void SigninScreenHandler::CancelPasswordChangedFlowInternal() {
  if (delegate_) {
    ShowImpl();
    delegate_->CancelPasswordChangedFlow();
  }
}

bool SigninScreenHandler::IsGaiaVisible() const {
  return IsSigninScreen(GetCurrentScreen()) &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
}

bool SigninScreenHandler::IsGaiaHiddenByError() const {
  return IsSigninScreenHiddenByError() &&
      ui_state_ == UI_STATE_GAIA_SIGNIN;
}

bool SigninScreenHandler::IsSigninScreenHiddenByError() const {
  return (GetCurrentScreen() == OobeScreen::SCREEN_ERROR_MESSAGE) &&
         (IsSigninScreen(error_screen_->GetParentScreen()));
}

bool SigninScreenHandler::IsGuestSigninAllowed() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings)
    return false;
  bool allow_guest;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  return allow_guest;
}

void SigninScreenHandler::OnShowAddUser() {
  is_account_picker_showing_first_time_ = false;
  lock_screen_utils::EnforcePolicyInputMethods(std::string());
  gaia_screen_handler_->ShowGaiaAsync();
}

net::Error SigninScreenHandler::FrameError() const {
  return gaia_screen_handler_->frame_error();
}

void SigninScreenHandler::OnCapsLockChanged(bool enabled) {
  caps_lock_enabled_ = enabled;
  if (page_is_ready())
    CallJS("login.AccountPickerScreen.setCapsLockState", caps_lock_enabled_);
}

void SigninScreenHandler::OnFeedbackFinished() {
  CallJS("login.UnrecoverableCryptohomeErrorScreen.resumeAfterFeedbackUI");

  // Recreate user's cryptohome after the feedback is attempted.
  HandleResyncUserData();
}

void SigninScreenHandler::OnAllowedInputMethodsChanged() {
  if (!webui_visible_)
    return;

  if (focused_pod_account_id_) {
    std::string user_input_method = lock_screen_utils::GetUserLastInputMethod(
        focused_pod_account_id_->GetUserEmail());
    lock_screen_utils::EnforcePolicyInputMethods(user_input_method);
  } else {
    lock_screen_utils::EnforcePolicyInputMethods(std::string());
  }
}

}  // namespace chromeos
