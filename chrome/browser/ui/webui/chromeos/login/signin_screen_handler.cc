// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/hwid_checker.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/webui_login_display.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/authenticated_user_email_retriever.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/native_window_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

#if defined(USE_AURA)
#include "ash/shell.h"
#include "ash/wm/lock_state_controller.h"
#endif

using content::BrowserThread;

namespace {

// User dictionary keys.
const char kKeyUsername[] = "username";
const char kKeyDisplayName[] = "displayName";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyEnterpriseDomain[] = "enterpriseDomain";
const char kKeyPublicAccount[] = "publicAccount";
const char kKeyLocallyManagedUser[] = "locallyManagedUser";
const char kKeySignedIn[] = "signedIn";
const char kKeyCanRemove[] = "canRemove";
const char kKeyIsOwner[] = "isOwner";
const char kKeyInitialAuthType[] = "initialAuthType";
const char kKeyMultiProfilesAllowed[] = "isMultiProfilesAllowed";
const char kKeyMultiProfilesPolicy[] = "multiProfilesPolicy";

// Max number of users to show.
const size_t kMaxUsers = 18;

// Timeout to delay first notification about offline state for a
// current network.
const int kOfflineTimeoutSec = 5;

// Timeout used to prevent infinite connecting to a flaky network.
const int kConnectingTimeoutSec = 60;

// Type of the login screen UI that is currently presented to user.
const char kSourceGaiaSignin[] = "gaia-signin";
const char kSourceAccountPicker[] = "account-picker";

// The Task posted to PostTaskAndReply in StartClearingDnsCache on the IO
// thread.
void ClearDnsCache(IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (browser_shutdown::IsTryingToQuit())
    return;

  io_thread->ClearHostCache();
}

static bool Contains(const std::vector<std::string>& container,
                     const std::string& value) {
  return std::find(container.begin(), container.end(), value) !=
         container.end();
}

}  // namespace

namespace chromeos {

namespace {

bool IsOnline(NetworkStateInformer::State state,
              ErrorScreenActor::ErrorReason reason) {
  return state == NetworkStateInformer::ONLINE &&
      reason != ErrorScreenActor::ERROR_REASON_PORTAL_DETECTED &&
      reason != ErrorScreenActor::ERROR_REASON_LOADING_TIMEOUT;
}

bool IsBehindCaptivePortal(NetworkStateInformer::State state,
                           ErrorScreenActor::ErrorReason reason) {
  return state == NetworkStateInformer::CAPTIVE_PORTAL ||
      reason == ErrorScreenActor::ERROR_REASON_PORTAL_DETECTED;
}

bool IsProxyError(NetworkStateInformer::State state,
                  ErrorScreenActor::ErrorReason reason,
                  net::Error frame_error) {
  return state == NetworkStateInformer::PROXY_AUTH_REQUIRED ||
      reason == ErrorScreenActor::ERROR_REASON_PROXY_AUTH_CANCELLED ||
      reason == ErrorScreenActor::ERROR_REASON_PROXY_CONNECTION_FAILED ||
      (reason == ErrorScreenActor::ERROR_REASON_FRAME_ERROR &&
       (frame_error == net::ERR_PROXY_CONNECTION_FAILED ||
        frame_error == net::ERR_TUNNEL_CONNECTION_FAILED));
}

bool IsSigninScreen(const OobeUI::Screen screen) {
  return screen == OobeUI::SCREEN_GAIA_SIGNIN ||
      screen == OobeUI::SCREEN_ACCOUNT_PICKER;
}

bool IsSigninScreenError(ErrorScreen::ErrorState error_state) {
  return error_state == ErrorScreen::ERROR_STATE_PORTAL ||
      error_state == ErrorScreen::ERROR_STATE_OFFLINE ||
      error_state == ErrorScreen::ERROR_STATE_PROXY ||
      error_state == ErrorScreen::ERROR_STATE_AUTH_EXT_TIMEOUT;
}

// Returns network name by service path.
std::string GetNetworkName(const std::string& service_path) {
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
  if (!network)
    return std::string();
  return network->name();
}

static bool SetUserInputMethodImpl(
    const std::string& username,
    chromeos::input_method::InputMethodManager* manager) {
  PrefService* const local_state = g_browser_process->local_state();

  const base::DictionaryValue* users_lru_input_methods =
      local_state->GetDictionary(prefs::kUsersLRUInputMethod);

  if (users_lru_input_methods == NULL) {
    DLOG(WARNING) << "SetUserInputMethod('" << username
                  << "'): no kUsersLRUInputMethod";
    return false;
  }

  std::string input_method;

  if (!users_lru_input_methods->GetStringWithoutPathExpansion(username,
                                                              &input_method)) {
    DVLOG(0) << "SetUserInputMethod('" << username
               << "'): no input method for this user";
    return false;
  }

  if (input_method.empty())
    return false;

  if (!manager->IsLoginKeyboard(input_method)) {
    LOG(WARNING) << "SetUserInputMethod('" << username
                 << "'): stored user LRU input method '" << input_method
                 << "' is no longer Full Latin Keyboard Language"
                 << " (entry dropped). Use hardware default instead.";

    DictionaryPrefUpdate updater(local_state, prefs::kUsersLRUInputMethod);

    base::DictionaryValue* const users_lru_input_methods = updater.Get();
    if (users_lru_input_methods != NULL) {
      users_lru_input_methods->SetStringWithoutPathExpansion(username, "");
    }
    return false;
  }

  if (!Contains(manager->GetActiveInputMethodIds(), input_method)) {
    if (!manager->EnableInputMethod(input_method)) {
      DLOG(ERROR) << "SigninScreenHandler::SetUserInputMethod('" << username
                  << "'): user input method '" << input_method
                  << "' is not enabled and enabling failed (ignored!).";
    }
  }
  manager->ChangeInputMethod(input_method);

  return true;
}

void RecordSAMLScrapingVerificationResultInHistogram(bool success) {
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SAML.Scraping.VerificationResult", success);
}

bool ShouldForceOnlineSignIn(const User* user) {
  // Public sessions are always allowed to log in offline.
  // Supervised user are allowed to log in offline if their OAuth token status
  // is unknown or valid.
  // For all other users, force online sign in if:
  // * The flag to force online sign-in is set for the user.
  // * The user's OAuth token is invalid.
  // * The user's OAuth token status is unknown (except supervised users,
  //   see above).
  if (user->is_logged_in())
    return false;

  const User::OAuthTokenStatus token_status = user->oauth_token_status();
  const bool is_locally_managed_user =
      user->GetType() == User::USER_TYPE_LOCALLY_MANAGED;
  const bool is_public_session =
      user->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT;

  if (is_locally_managed_user &&
      token_status == User::OAUTH_TOKEN_STATUS_UNKNOWN) {
    return false;
  }

  if (is_public_session)
    return false;

  return user->force_online_signin() ||
         (token_status == User::OAUTH2_TOKEN_STATUS_INVALID) ||
         (token_status == User::OAUTH_TOKEN_STATUS_UNKNOWN);
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
    ErrorScreenActor* error_screen_actor,
    CoreOobeActor* core_oobe_actor,
    GaiaScreenHandler* gaia_screen_handler)
    : ui_state_(UI_STATE_UNKNOWN),
      delegate_(NULL),
      native_window_delegate_(NULL),
      show_on_init_(false),
      oobe_ui_(false),
      focus_stolen_(false),
      gaia_silent_load_(false),
      is_account_picker_showing_first_time_(false),
      dns_cleared_(false),
      dns_clear_task_running_(false),
      cookies_cleared_(false),
      network_state_informer_(network_state_informer),
      using_saml_api_(false),
      test_expects_complete_login_(false),
      weak_factory_(this),
      webui_visible_(false),
      preferences_changed_delayed_(false),
      error_screen_actor_(error_screen_actor),
      core_oobe_actor_(core_oobe_actor),
      is_first_update_state_call_(true),
      offline_login_active_(false),
      last_network_state_(NetworkStateInformer::UNKNOWN),
      has_pending_auth_ui_(false),
      caps_lock_enabled_(false),
      gaia_screen_handler_(gaia_screen_handler) {
  DCHECK(network_state_informer_.get());
  DCHECK(error_screen_actor_);
  DCHECK(core_oobe_actor_);
  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->SetSigninScreenHandler(this);
  network_state_informer_->AddObserver(this);
  allow_new_user_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefAllowNewUser,
      base::Bind(&SigninScreenHandler::UserSettingsChanged,
                 base::Unretained(this)));
  allow_guest_subscription_ = CrosSettings::Get()->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::Bind(&SigninScreenHandler::UserSettingsChanged,
                 base::Unretained(this)));

  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_NEEDED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_CANCELLED,
                 content::NotificationService::AllSources());
}

SigninScreenHandler::~SigninScreenHandler() {
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
  weak_factory_.InvalidateWeakPtrs();
  if (delegate_)
    delegate_->SetWebUIHandler(NULL);
  network_state_informer_->RemoveObserver(this);
}

void SigninScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("passwordHint", IDS_LOGIN_POD_EMPTY_PASSWORD_TEXT);
  builder->Add("podMenuButtonAccessibleName",
               IDS_LOGIN_POD_MENU_BUTTON_ACCESSIBLE_NAME);
  builder->Add("podMenuRemoveItemAccessibleName",
               IDS_LOGIN_POD_MENU_REMOVE_ITEM_ACCESSIBLE_NAME);
  builder->Add("passwordFieldAccessibleName",
               IDS_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME);
  builder->Add("signedIn", IDS_SCREEN_LOCK_ACTIVE_USER);
  builder->Add("signinButton", IDS_LOGIN_BUTTON);
  builder->Add("launchAppButton", IDS_LAUNCH_APP_BUTTON);
  builder->Add("shutDown", IDS_SHUTDOWN_BUTTON);
  builder->Add("addUser", IDS_ADD_USER_BUTTON);
  builder->Add("browseAsGuest", IDS_GO_INCOGNITO_BUTTON);
  builder->Add("cancel", IDS_CANCEL);
  builder->Add("signOutUser", IDS_SCREEN_LOCK_SIGN_OUT);
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

  // Strings used by password changed dialog.
  builder->Add("passwordChangedTitle", IDS_LOGIN_PASSWORD_CHANGED_TITLE);
  builder->Add("passwordChangedDesc", IDS_LOGIN_PASSWORD_CHANGED_DESC);
  builder->AddF("passwordChangedMoreInfo",
                IDS_LOGIN_PASSWORD_CHANGED_MORE_INFO,
                IDS_SHORT_PRODUCT_OS_NAME);

  builder->Add("oldPasswordHint", IDS_LOGIN_PASSWORD_CHANGED_OLD_PASSWORD_HINT);
  builder->Add("oldPasswordIncorrect",
               IDS_LOGIN_PASSWORD_CHANGED_INCORRECT_OLD_PASSWORD);
  builder->Add("passwordChangedCantRemember",
               IDS_LOGIN_PASSWORD_CHANGED_CANT_REMEMBER);
  builder->Add("passwordChangedBackButton",
               IDS_LOGIN_PASSWORD_CHANGED_BACK_BUTTON);
  builder->Add("passwordChangedsOkButton", IDS_OK);
  builder->Add("passwordChangedProceedAnyway",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY);
  builder->Add("proceedAnywayButton",
               IDS_LOGIN_PASSWORD_CHANGED_PROCEED_ANYWAY_BUTTON);
  builder->Add("publicAccountInfoFormat", IDS_LOGIN_PUBLIC_ACCOUNT_INFO_FORMAT);
  builder->Add("publicAccountReminder",
               IDS_LOGIN_PUBLIC_ACCOUNT_SIGNOUT_REMINDER);
  builder->Add("publicAccountEnter", IDS_LOGIN_PUBLIC_ACCOUNT_ENTER);
  builder->Add("publicAccountEnterAccessibleName",
               IDS_LOGIN_PUBLIC_ACCOUNT_ENTER_ACCESSIBLE_NAME);
  builder->AddF("removeUserWarningText",
               IDS_LOGIN_POD_USER_REMOVE_WARNING,
               base::UTF8ToUTF16(chrome::kSupervisedUserManagementDisplayURL));
  builder->Add("removeUserWarningButtonTitle",
               IDS_LOGIN_POD_USER_REMOVE_WARNING_BUTTON);

  builder->Add("samlNotice", IDS_LOGIN_SAML_NOTICE);

  builder->Add("confirmPasswordTitle", IDS_LOGIN_CONFIRM_PASSWORD_TITLE);
  builder->Add("confirmPasswordLabel", IDS_LOGIN_CONFIRM_PASSWORD_LABEL);
  builder->Add("confirmPasswordConfirmButton",
               IDS_LOGIN_CONFIRM_PASSWORD_CONFIRM_BUTTON);
  builder->Add("confirmPasswordText", IDS_LOGIN_CONFIRM_PASSWORD_TEXT);
  builder->Add("confirmPasswordErrorText",
               IDS_LOGIN_CONFIRM_PASSWORD_ERROR_TEXT);
  builder->Add("easyUnlockTooltip",
               IDS_LOGIN_EASY_UNLOCK_TOOLTIP);

  builder->Add("fatalEnrollmentError",
               IDS_ENTERPRISE_ENROLLMENT_AUTH_FATAL_ERROR);

  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled())
    builder->Add("demoLoginMessage", IDS_KIOSK_MODE_LOGIN_MESSAGE);

  builder->Add("runType", CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFirstExecAfterBoot) ?  "firstExecAfterBoot" :
                                        "notFirstExecAfterBoot");
}

void SigninScreenHandler::Show(const LoginScreenContext& context) {
  CHECK(delegate_);

  // Just initialize internal fields from context and call ShowImpl().
  oobe_ui_ = context.oobe_ui();
  if (!context.email().empty())
    email_ = context.email();
  else
    email_.clear();
  ShowImpl();
}

void SigninScreenHandler::ShowRetailModeLoginSpinner() {
  CallJS("showLoginSpinner");
}

void SigninScreenHandler::SetDelegate(SigninScreenHandlerDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_)
    delegate_->SetWebUIHandler(this);
  else
    auto_enrollment_progress_subscription_.reset();
}

void SigninScreenHandler::SetNativeWindowDelegate(
    NativeWindowDelegate* native_window_delegate) {
  native_window_delegate_ = native_window_delegate;
}

void SigninScreenHandler::OnNetworkReady() {
  LOG(WARNING) << "OnNetworkReady() call.";
  MaybePreloadAuthExtension();
}

void SigninScreenHandler::UpdateState(ErrorScreenActor::ErrorReason reason) {
  UpdateStateInternal(reason, false);
}

// SigninScreenHandler, private: -----------------------------------------------

void SigninScreenHandler::ShowImpl() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  if (oobe_ui_) {
    // Shows new user sign-in for OOBE.
    OnShowAddUser(email_);
  } else {
    // Populates account picker. Animation is turned off for now until we
    // figure out how to make it fast enough.
    SendUserList(false);

    // Reset Caps Lock state when login screen is shown.
    input_method::InputMethodManager::Get()
        ->GetImeKeyboard()
        ->SetCapsLockEnabled(false);

    base::DictionaryValue params;
    params.SetBoolean("disableAddUser", AllWhitelistedUsersPresent());
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, &params);
  }
}

void SigninScreenHandler::UpdateUIState(UIState ui_state,
                                        base::DictionaryValue* params) {
  switch (ui_state) {
    case UI_STATE_GAIA_SIGNIN:
      ui_state_ = UI_STATE_GAIA_SIGNIN;
      ShowScreen(OobeUI::kScreenGaiaSignin, params);
      break;
    case UI_STATE_ACCOUNT_PICKER:
      ui_state_ = UI_STATE_ACCOUNT_PICKER;
      ShowScreen(OobeUI::kScreenAccountPicker, params);
      break;
    default:
      NOTREACHED();
      break;
  }
}

// TODO (ygorshenin@): split this method into small parts.
// TODO (ygorshenin@): move this logic to GaiaScreenHandler.
void SigninScreenHandler::UpdateStateInternal(
    ErrorScreenActor::ErrorReason reason,
    bool force_update) {
  // Do nothing once user has signed in or sign in is in progress.
  // TODO(ygorshenin): We will end up here when processing network state
  // notification but no ShowSigninScreen() was called so delegate_ will be
  // NULL. Network state processing logic does not belong here.
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

  // TODO (ygorshenin@): switch log level to INFO once signin screen
  // will be tested well.
  LOG(WARNING) << "SigninScreenHandler::UpdateStateInternal(): "
               << "state=" << NetworkStateInformer::StatusString(state) << ", "
               << "network_name=" << network_name << ", "
               << "reason=" << ErrorScreenActor::ErrorReasonString(reason)
               << ", force_update=" << force_update;
  update_state_closure_.Cancel();

  if ((state == NetworkStateInformer::OFFLINE && !force_update) ||
      has_pending_auth_ui_) {
    update_state_closure_.Reset(
        base::Bind(&SigninScreenHandler::UpdateStateInternal,
                   weak_factory_.GetWeakPtr(),
                   reason,
                   true));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        update_state_closure_.callback(),
        base::TimeDelta::FromSeconds(kOfflineTimeoutSec));
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
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          connecting_closure_.callback(),
          base::TimeDelta::FromSeconds(kConnectingTimeoutSec));
    }
    return;
  }
  connecting_closure_.Cancel();

  const bool is_online = IsOnline(state, reason);
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_gaia_loading_timeout =
      (reason == ErrorScreenActor::ERROR_REASON_LOADING_TIMEOUT);
  const bool is_gaia_error =
      FrameError() != net::OK && FrameError() != net::ERR_NETWORK_CHANGED;
  const bool is_gaia_signin = IsGaiaVisible() || IsGaiaHiddenByError();
  const bool error_screen_should_overlay =
      !offline_login_active_ && IsGaiaVisible();
  const bool from_not_online_to_online_transition =
      is_online && last_network_state_ != NetworkStateInformer::ONLINE;
  last_network_state_ = state;

  if (is_online || !is_behind_captive_portal)
    error_screen_actor_->HideCaptivePortal();

  // Hide offline message (if needed) and return if current screen is
  // not a Gaia frame.
  if (!is_gaia_signin) {
    if (!IsSigninScreenHiddenByError())
      HideOfflineMessage(state, reason);
    return;
  }

  // Reload frame if network state is changed from {!ONLINE} -> ONLINE state.
  if (reason == ErrorScreenActor::ERROR_REASON_NETWORK_STATE_CHANGED &&
      from_not_online_to_online_transition) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry page load since network has been changed.";
    ReloadGaiaScreen();
  }

  if (reason == ErrorScreenActor::ERROR_REASON_PROXY_CONFIG_CHANGED &&
      error_screen_should_overlay) {
    // Schedules a immediate retry.
    LOG(WARNING) << "Retry page load since proxy settings has been changed.";
    ReloadGaiaScreen();
  }

  if (reason == ErrorScreenActor::ERROR_REASON_FRAME_ERROR &&
      !IsProxyError(state, reason, FrameError())) {
    LOG(WARNING) << "Retry page load due to reason: "
                 << ErrorScreenActor::ErrorReasonString(reason);
    ReloadGaiaScreen();
  }

  if ((!is_online || is_gaia_loading_timeout || is_gaia_error) &&
      !offline_login_active_) {
    SetupAndShowOfflineMessage(state, reason);
  } else {
    HideOfflineMessage(state, reason);
  }
}

void SigninScreenHandler::SetupAndShowOfflineMessage(
    NetworkStateInformer:: State state,
    ErrorScreenActor::ErrorReason reason) {
  const std::string network_path = network_state_informer_->network_path();
  const bool is_behind_captive_portal = IsBehindCaptivePortal(state, reason);
  const bool is_proxy_error = IsProxyError(state, reason, FrameError());
  const bool is_gaia_loading_timeout =
      (reason == ErrorScreenActor::ERROR_REASON_LOADING_TIMEOUT);

  if (is_proxy_error) {
    error_screen_actor_->SetErrorState(ErrorScreen::ERROR_STATE_PROXY,
                                       std::string());
  } else if (is_behind_captive_portal) {
    // Do not bother a user with obsessive captive portal showing. This
    // check makes captive portal being shown only once: either when error
    // screen is shown for the first time or when switching from another
    // error screen (offline, proxy).
    if (IsGaiaVisible() ||
        (error_screen_actor_->error_state() !=
         ErrorScreen::ERROR_STATE_PORTAL)) {
      error_screen_actor_->FixCaptivePortal();
    }
    const std::string network_name = GetNetworkName(network_path);
    error_screen_actor_->SetErrorState(ErrorScreen::ERROR_STATE_PORTAL,
                                       network_name);
  } else if (is_gaia_loading_timeout) {
    error_screen_actor_->SetErrorState(
        ErrorScreen::ERROR_STATE_AUTH_EXT_TIMEOUT, std::string());
  } else {
    error_screen_actor_->SetErrorState(ErrorScreen::ERROR_STATE_OFFLINE,
                                       std::string());
  }

  const bool guest_signin_allowed = IsGuestSigninAllowed() &&
      IsSigninScreenError(error_screen_actor_->error_state());
  error_screen_actor_->AllowGuestSignin(guest_signin_allowed);

  const bool offline_login_allowed = IsOfflineLoginAllowed() &&
      IsSigninScreenError(error_screen_actor_->error_state()) &&
      error_screen_actor_->error_state() !=
      ErrorScreen::ERROR_STATE_AUTH_EXT_TIMEOUT;
  error_screen_actor_->AllowOfflineLogin(offline_login_allowed);

  if (GetCurrentScreen() != OobeUI::SCREEN_ERROR_MESSAGE) {
    base::DictionaryValue params;
    const std::string network_type = network_state_informer_->network_type();
    params.SetString("lastNetworkType", network_type);
    error_screen_actor_->SetUIState(ErrorScreen::UI_STATE_SIGNIN);
    error_screen_actor_->Show(OobeUI::SCREEN_GAIA_SIGNIN, &params);
  }
}

void SigninScreenHandler::HideOfflineMessage(
    NetworkStateInformer::State state,
    ErrorScreenActor::ErrorReason reason) {
  if (!IsSigninScreenHiddenByError())
    return;

  error_screen_actor_->Hide();

  // Forces a reload for Gaia screen on hiding error message.
  if (IsGaiaVisible() || IsGaiaHiddenByError())
    ReloadGaiaScreen();
}

void SigninScreenHandler::ReloadGaiaScreen() {
  gaia_screen_handler_->ReloadGaia();
}

void SigninScreenHandler::Initialize() {
  // If delegate_ is NULL here (e.g. WebUIScreenLocker has been destroyed),
  // don't do anything, just return.
  if (!delegate_)
    return;

  // Make sure the event is processed by this before the IME.
  ash::Shell::GetInstance()->PrependPreTargetHandler(this);

  if (show_on_init_) {
    show_on_init_ = false;
    ShowImpl();
  }
}

gfx::NativeWindow SigninScreenHandler::GetNativeWindow() {
  if (native_window_delegate_)
    return native_window_delegate_->GetNativeWindow();
  return NULL;
}

void SigninScreenHandler::RegisterMessages() {
  AddCallback("usingSAMLAPI", &SigninScreenHandler::HandleUsingSAMLAPI);
  AddCallback("scrapedPasswordCount",
              &SigninScreenHandler::HandleScrapedPasswordCount);
  AddCallback("scrapedPasswordVerificationFailed",
              &SigninScreenHandler::HandleScrapedPasswordVerificationFailed);
  AddCallback("authenticateUser", &SigninScreenHandler::HandleAuthenticateUser);
  AddCallback("completeLogin", &SigninScreenHandler::HandleCompleteLogin);
  AddCallback("completeAuthentication",
              &SigninScreenHandler::HandleCompleteAuthentication);
  AddCallback("getUsers", &SigninScreenHandler::HandleGetUsers);
  AddCallback("launchDemoUser", &SigninScreenHandler::HandleLaunchDemoUser);
  AddCallback("launchIncognito", &SigninScreenHandler::HandleLaunchIncognito);
  AddCallback("showLocallyManagedUserCreationScreen",
              &SigninScreenHandler::HandleShowLocallyManagedUserCreationScreen);
  AddCallback("launchPublicAccount",
              &SigninScreenHandler::HandleLaunchPublicAccount);
  AddRawCallback("offlineLogin", &SigninScreenHandler::HandleOfflineLogin);
  AddCallback("rebootSystem", &SigninScreenHandler::HandleRebootSystem);
  AddRawCallback("showAddUser", &SigninScreenHandler::HandleShowAddUser);
  AddCallback("shutdownSystem", &SigninScreenHandler::HandleShutdownSystem);
  AddCallback("loadWallpaper", &SigninScreenHandler::HandleLoadWallpaper);
  AddCallback("removeUser", &SigninScreenHandler::HandleRemoveUser);
  AddCallback("toggleEnrollmentScreen",
              &SigninScreenHandler::HandleToggleEnrollmentScreen);
  AddCallback("toggleKioskEnableScreen",
              &SigninScreenHandler::HandleToggleKioskEnableScreen);
  AddCallback("toggleResetScreen",
              &SigninScreenHandler::HandleToggleResetScreen);
  AddCallback("createAccount", &SigninScreenHandler::HandleCreateAccount);
  AddCallback("accountPickerReady",
              &SigninScreenHandler::HandleAccountPickerReady);
  AddCallback("wallpaperReady", &SigninScreenHandler::HandleWallpaperReady);
  AddCallback("loginWebuiReady", &SigninScreenHandler::HandleLoginWebuiReady);
  AddCallback("signOutUser", &SigninScreenHandler::HandleSignOutUser);
  AddCallback("openProxySettings",
              &SigninScreenHandler::HandleOpenProxySettings);
  AddCallback("loginVisible", &SigninScreenHandler::HandleLoginVisible);
  AddCallback("cancelPasswordChangedFlow",
              &SigninScreenHandler::HandleCancelPasswordChangedFlow);
  AddCallback("cancelUserAdding",
              &SigninScreenHandler::HandleCancelUserAdding);
  AddCallback("migrateUserData", &SigninScreenHandler::HandleMigrateUserData);
  AddCallback("resyncUserData", &SigninScreenHandler::HandleResyncUserData);
  AddCallback("loginUIStateChanged",
              &SigninScreenHandler::HandleLoginUIStateChanged);
  AddCallback("unlockOnLoginSuccess",
              &SigninScreenHandler::HandleUnlockOnLoginSuccess);
  AddCallback("showLoadingTimeoutError",
              &SigninScreenHandler::HandleShowLoadingTimeoutError);
  AddCallback("updateOfflineLogin",
              &SigninScreenHandler::HandleUpdateOfflineLogin);
  AddCallback("focusPod", &SigninScreenHandler::HandleFocusPod);
  AddCallback("customButtonClicked",
              &SigninScreenHandler::HandleCustomButtonClicked);
  AddCallback("retrieveAuthenticatedUserEmail",
              &SigninScreenHandler::HandleRetrieveAuthenticatedUserEmail);

  // This message is sent by the kiosk app menu, but is handled here
  // so we can tell the delegate to launch the app.
  AddCallback("launchKioskApp", &SigninScreenHandler::HandleLaunchKioskApp);
}

void SigninScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUsersLRUInputMethod);
}

void SigninScreenHandler::HandleGetUsers() {
  SendUserList(false);
}

void SigninScreenHandler::ClearAndEnablePassword() {
  core_oobe_actor_->ResetSignInUI(false);
}

void SigninScreenHandler::ClearUserPodPassword() {
  core_oobe_actor_->ClearUserPodPassword();
}

void SigninScreenHandler::RefocusCurrentPod() {
  core_oobe_actor_->RefocusCurrentPod();
}

void SigninScreenHandler::OnLoginSuccess(const std::string& username) {
  core_oobe_actor_->OnLoginSuccess(username);
}

void SigninScreenHandler::OnUserRemoved(const std::string& username) {
  CallJS("login.AccountPickerScreen.removeUser", username);
  if (delegate_->GetUsers().empty())
    OnShowAddUser("");
}

void SigninScreenHandler::OnUserImageChanged(const User& user) {
  if (page_is_ready())
    CallJS("login.AccountPickerScreen.updateUserImage", user.email());
}

void SigninScreenHandler::OnPreferencesChanged() {
  // Make sure that one of the login UI is fully functional now, otherwise
  // preferences update would be picked up next time it will be shown.
  if (!webui_visible_) {
    LOG(WARNING) << "Login UI is not active - postponed prefs change.";
    preferences_changed_delayed_ = true;
    return;
  }

  if (delegate_ && !delegate_->IsShowUsers()) {
    HandleShowAddUser(NULL);
  } else {
    SendUserList(false);
    UpdateUIState(UI_STATE_ACCOUNT_PICKER, NULL);
  }
  preferences_changed_delayed_ = false;
}

void SigninScreenHandler::ResetSigninScreenHandlerDelegate() {
  SetDelegate(NULL);
}

void SigninScreenHandler::ShowBannerMessage(const std::string& message) {
  CallJS("login.AccountPickerScreen.showBannerMessage", message);
}

void SigninScreenHandler::ShowUserPodButton(
    const std::string& username,
    const std::string& iconURL,
    const base::Closure& click_callback) {
  user_pod_button_callback_map_[username] = click_callback;
  CallJS("login.AccountPickerScreen.showUserPodButton", username, iconURL);

  // TODO(tengs): Move this code once we move unlocking to native code.
  if (ScreenLocker::default_screen_locker()) {
    UserManager* user_manager = UserManager::Get();
    const User* user = user_manager->FindUser(username);
    if (!user)
      return;
    PrefService* profile_prefs =
        user_manager->GetProfileByUser(user)->GetPrefs();
    if (profile_prefs->GetBoolean(prefs::kEasyUnlockShowTutorial)) {
      CallJS("login.AccountPickerScreen.showEasyUnlockBubble");
      profile_prefs->SetBoolean(prefs::kEasyUnlockShowTutorial, false);
    }
  }
}

void SigninScreenHandler::HideUserPodButton(const std::string& username) {
  CallJS("login.AccountPickerScreen.hideUserPodButton", username);
}

void SigninScreenHandler::SetAuthType(const std::string& username,
                                      LoginDisplay::AuthType auth_type,
                                      const std::string& initial_value) {
  user_auth_type_map_[username] = auth_type;
  CallJS("login.AccountPickerScreen.setAuthType",
         username,
         static_cast<int>(auth_type),
         base::StringValue(initial_value));
}

LoginDisplay::AuthType SigninScreenHandler::GetAuthType(
    const std::string& username) const {
  if (user_auth_type_map_.find(username) == user_auth_type_map_.end())
    return LoginDisplay::OFFLINE_PASSWORD;
  return user_auth_type_map_.find(username)->second;
}

void SigninScreenHandler::ShowError(int login_attempts,
                                    const std::string& error_text,
                                    const std::string& help_link_text,
                                    HelpAppLauncher::HelpTopic help_topic_id) {
  core_oobe_actor_->ShowSignInError(login_attempts, error_text, help_link_text,
                                    help_topic_id);
}

void SigninScreenHandler::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  switch (error_id) {
    case LoginDisplay::TPM_ERROR:
      core_oobe_actor_->ShowTpmError();
      break;
    default:
      NOTREACHED() << "Unknown sign in error";
      break;
  }
}

void SigninScreenHandler::ShowSigninUI(const std::string& email) {
  core_oobe_actor_->ShowSignInUI(email);
}

void SigninScreenHandler::ShowControlBar(bool show) {
  core_oobe_actor_->ShowControlBar(show);
}

void SigninScreenHandler::ShowGaiaPasswordChanged(const std::string& username) {
  email_ = username;
  password_changed_for_.insert(email_);
  core_oobe_actor_->ShowSignInUI(email_);
  CallJS("login.setAuthType",
         username,
         static_cast<int>(LoginDisplay::ONLINE_SIGN_IN),
         base::StringValue(""));
}

void SigninScreenHandler::ShowPasswordChangedDialog(bool show_password_error) {
  core_oobe_actor_->ShowPasswordChangedScreen(show_password_error);
}

void SigninScreenHandler::ShowSigninScreenForCreds(
    const std::string& username,
    const std::string& password) {
  VLOG(2) << "ShowSigninScreenForCreds  for user " << username
          << ", frame_state=" << FrameState();

  test_user_ = username;
  test_pass_ = password;
  test_expects_complete_login_ = true;

  // Submit login form for test if gaia is ready. If gaia is loading, login
  // will be attempted in HandleLoginWebuiReady after gaia is ready. Otherwise,
  // reload gaia then follow the loading case.
  if (FrameState() == GaiaScreenHandler::FRAME_STATE_LOADED)
    SubmitLoginFormForTest();
  else if (FrameState() != GaiaScreenHandler::FRAME_STATE_LOADING)
    HandleShowAddUser(NULL);
}

void SigninScreenHandler::OnCookiesCleared(base::Closure on_clear_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cookies_cleared_ = true;
  on_clear_callback.Run();
}

void SigninScreenHandler::OnKeyEvent(ui::KeyEvent* key) {
  if (key->type() == ui::ET_KEY_PRESSED &&
      key->key_code() == ui::VKEY_CAPITAL) {
    caps_lock_enabled_ = !caps_lock_enabled_;
    if (page_is_ready())
      CallJS("login.AccountPickerScreen.setCapsLockState", caps_lock_enabled_);
  }
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
      if (IsSigninScreenHiddenByError()) {
        // Hide error screen and reload auth extension.
        HideOfflineMessage(network_state_informer_->state(),
                           ErrorScreenActor::ERROR_REASON_PROXY_AUTH_SUPPLIED);
      } else if (ui_state_ == UI_STATE_GAIA_SIGNIN) {
        // Reload auth extension as proxy credentials are supplied.
        ReloadGaiaScreen();
      }
      break;
    case chrome::NOTIFICATION_AUTH_CANCELLED: {
      // Don't reload auth extension if proxy auth dialog was cancelled.
      has_pending_auth_ui_ = false;
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

void SigninScreenHandler::OnDnsCleared() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dns_clear_task_running_ = false;
  dns_cleared_ = true;
  ShowSigninScreenIfReady();
}

// Update keyboard layout to least recently used by the user.
void SigninScreenHandler::SetUserInputMethod(const std::string& username) {
  UserManager* user_manager = UserManager::Get();
  if (user_manager->IsUserLoggedIn()) {
    // We are on sign-in screen inside user session (adding new user to
    // the session or on lock screen), don't switch input methods in this case.
    // TODO(dpolukhin): adding user and sign-in should be consistent
    // crbug.com/292774
    return;
  }

  chromeos::input_method::InputMethodManager* const manager =
      chromeos::input_method::InputMethodManager::Get();

  const bool succeed = SetUserInputMethodImpl(username, manager);

  // This is also a case when LRU layout is set only for a few local users,
  // thus others need to be switched to default locale.
  // Otherwise they will end up using another user's locale to log in.
  if (!succeed) {
    DVLOG(0) << "SetUserInputMethod('" << username
               << "'): failed to set user layout. Switching to default.";

    manager->SetInputMethodLoginDefault();
  }
}

void SigninScreenHandler::ShowSigninScreenIfReady() {
  LOG(WARNING) << "ShowSigninScreenIfReady() call.";

  if (!dns_cleared_ || !cookies_cleared_ || !delegate_)
    return;

  std::string active_network_path = network_state_informer_->network_path();
  if (gaia_silent_load_ &&
      (network_state_informer_->state() != NetworkStateInformer::ONLINE ||
       gaia_silent_load_network_ != active_network_path)) {
    // Network has changed. Force Gaia reload.
    gaia_silent_load_ = false;
    // Gaia page will be realoded, so focus isn't stolen anymore.
    focus_stolen_ = false;
  }

  // Note that LoadAuthExtension clears |email_|.
  if (email_.empty())
    delegate_->LoadSigninWallpaper();
  else
    delegate_->LoadWallpaper(email_);

  // Set Least Recently Used input method for the user.
  if (!email_.empty())
    SetUserInputMethod(email_);

  LoadAuthExtension(!gaia_silent_load_, false, false);
  UpdateUIState(UI_STATE_GAIA_SIGNIN, NULL);

  if (gaia_silent_load_) {
    // The variable is assigned to false because silently loaded Gaia page was
    // used.
    gaia_silent_load_ = false;
    if (focus_stolen_)
      HandleLoginWebuiReady();
  }

  UpdateState(ErrorScreenActor::ERROR_REASON_UPDATE);
}

void SigninScreenHandler::LoadAuthExtension(
    bool force, bool silent_load, bool offline) {
  GaiaContext context;
  context.force_reload = force;
  context.is_local = offline;
  context.password_changed =
      !email_.empty() && password_changed_for_.count(email_);
  if (delegate_)
    context.show_users = delegate_->IsShowUsers();
  context.use_offline = offline;
  if (delegate_)
    context.has_users = !delegate_->GetUsers().empty();
  context.email = email_;

  email_.clear();

  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->LoadGaia(context);
}

void SigninScreenHandler::UserSettingsChanged() {
  DCHECK(gaia_screen_handler_);
  GaiaContext context;
  if (delegate_)
    context.has_users = !delegate_->GetUsers().empty();
  gaia_screen_handler_->UpdateGaia(context);
  UpdateAddButtonStatus();
}

void SigninScreenHandler::UpdateAddButtonStatus() {
  CallJS("cr.ui.login.DisplayManager.updateAddUserButtonStatus",
         AllWhitelistedUsersPresent());
}

void SigninScreenHandler::HandleUsingSAMLAPI() {
  SetSAMLPrincipalsAPIUsed(true);
}

void SigninScreenHandler::HandleScrapedPasswordCount(int password_count) {
  SetSAMLPrincipalsAPIUsed(false);
  // Use a histogram that has 11 buckets, one for each of the values in [0, 9]
  // and an overflow bucket at the end.
  UMA_HISTOGRAM_ENUMERATION(
      "ChromeOS.SAML.Scraping.PasswordCount", std::min(password_count, 10), 11);
  if (password_count == 0)
    HandleScrapedPasswordVerificationFailed();
}

void SigninScreenHandler::HandleScrapedPasswordVerificationFailed() {
  RecordSAMLScrapingVerificationResultInHistogram(false);
}

void SigninScreenHandler::HandleCompleteLogin(const std::string& typed_email,
                                              const std::string& password,
                                              bool using_saml) {
  if (!delegate_)
    return;

  if (using_saml && !using_saml_api_)
    RecordSAMLScrapingVerificationResultInHistogram(true);

  const std::string sanitized_email = gaia::SanitizeEmail(typed_email);
  delegate_->SetDisplayEmail(sanitized_email);
  delegate_->CompleteLogin(UserContext(
      sanitized_email,
      password,
      std::string(),  // auth_code
      std::string(),  // username_hash
      true,           // using_oauth
      using_saml ? UserContext::AUTH_FLOW_GAIA_WITH_SAML
                 : UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML));

  if (test_expects_complete_login_) {
    VLOG(2) << "Complete test login for " << typed_email
            << ", requested=" << test_user_;

    test_expects_complete_login_ = false;
    test_user_.clear();
    test_pass_.clear();
  }
}

void SigninScreenHandler::HandleCompleteAuthentication(
    const std::string& email,
    const std::string& password,
    const std::string& auth_code) {
  if (!delegate_)
    return;
  const std::string sanitized_email = gaia::SanitizeEmail(email);
  delegate_->SetDisplayEmail(sanitized_email);
  delegate_->CompleteLogin(UserContext(sanitized_email, password, auth_code));
}

void SigninScreenHandler::HandleAuthenticateUser(const std::string& username,
                                                 const std::string& password) {
  if (!delegate_)
    return;
  delegate_->Login(UserContext(gaia::SanitizeEmail(username),
                               password,
                               std::string()));  // auth_code
}

void SigninScreenHandler::HandleLaunchDemoUser() {
  if (delegate_)
    delegate_->LoginAsRetailModeUser();
}

void SigninScreenHandler::HandleLaunchIncognito() {
  if (delegate_)
    delegate_->LoginAsGuest();
}

void SigninScreenHandler::HandleShowLocallyManagedUserCreationScreen() {
  if (!UserManager::Get()->AreLocallyManagedUsersAllowed()) {
    LOG(ERROR) << "Managed users not allowed.";
    return;
  }
  scoped_ptr<base::DictionaryValue> params(new base::DictionaryValue());
  LoginDisplayHostImpl::default_host()->
      StartWizard(WizardController::kLocallyManagedUserCreationScreenName,
      params.Pass());
}

void SigninScreenHandler::HandleLaunchPublicAccount(
    const std::string& username) {
  if (delegate_)
    delegate_->LoginAsPublicAccount(username);
}

void SigninScreenHandler::HandleOfflineLogin(const base::ListValue* args) {
  if (!delegate_ || delegate_->IsShowUsers()) {
    NOTREACHED();
    return;
  }
  if (!args->GetString(0, &email_))
    email_.clear();
  // Load auth extension. Parameters are: force reload, do not load extension in
  // background, use offline version.
  LoadAuthExtension(true, false, true);
  UpdateUIState(UI_STATE_GAIA_SIGNIN, NULL);
}

void SigninScreenHandler::HandleShutdownSystem() {
  ash::Shell::GetInstance()->lock_state_controller()->RequestShutdown();
}

void SigninScreenHandler::HandleLoadWallpaper(const std::string& email) {
  if (delegate_)
    delegate_->LoadWallpaper(email);
}

void SigninScreenHandler::HandleRebootSystem() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void SigninScreenHandler::HandleRemoveUser(const std::string& email) {
  if (!delegate_)
    return;
  delegate_->RemoveUser(email);
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
  OnShowAddUser(email);
}

void SigninScreenHandler::HandleToggleEnrollmentScreen() {
  if (delegate_)
    delegate_->ShowEnterpriseEnrollmentScreen();
}

void SigninScreenHandler::HandleToggleKioskEnableScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (delegate_ &&
      !auto_enrollment_progress_subscription_ &&
      !connector->IsEnterpriseManaged() &&
      LoginDisplayHostImpl::default_host()) {
    AutoEnrollmentController* auto_enrollment_controller =
        LoginDisplayHostImpl::default_host()->GetAutoEnrollmentController();
    auto_enrollment_progress_subscription_ =
        auto_enrollment_controller->RegisterProgressCallback(
            base::Bind(&SigninScreenHandler::ContinueKioskEnableFlow,
                       weak_factory_.GetWeakPtr()));
    ContinueKioskEnableFlow(auto_enrollment_controller->state());
  }
}

void SigninScreenHandler::HandleToggleResetScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (delegate_ && !connector->IsEnterpriseManaged())
    delegate_->ShowResetScreen();
}

void SigninScreenHandler::HandleToggleKioskAutolaunchScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (delegate_ && !connector->IsEnterpriseManaged())
    delegate_->ShowKioskAutolaunchScreen();
}

void SigninScreenHandler::FillUserDictionary(User* user,
                                             bool is_owner,
                                             bool is_signin_to_add,
                                             LoginDisplay::AuthType auth_type,
                                             base::DictionaryValue* user_dict) {
  const std::string& email = user->email();
  const bool is_public_account =
      user->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT;
  const bool is_locally_managed_user =
      user->GetType() == User::USER_TYPE_LOCALLY_MANAGED;

  user_dict->SetString(kKeyUsername, email);
  user_dict->SetString(kKeyEmailAddress, user->display_email());
  user_dict->SetString(kKeyDisplayName, user->GetDisplayName());
  user_dict->SetBoolean(kKeyPublicAccount, is_public_account);
  user_dict->SetBoolean(kKeyLocallyManagedUser, is_locally_managed_user);
  user_dict->SetInteger(kKeyInitialAuthType, auth_type);
  user_dict->SetBoolean(kKeySignedIn, user->is_logged_in());
  user_dict->SetBoolean(kKeyIsOwner, is_owner);

  // Fill in multi-profiles related fields.
  if (is_signin_to_add) {
    MultiProfileUserController* multi_profile_user_controller =
        UserManager::Get()->GetMultiProfileUserController();
    std::string behavior =  multi_profile_user_controller->
        GetCachedValue(user->email());
    user_dict->SetBoolean(kKeyMultiProfilesAllowed,
        multi_profile_user_controller->IsUserAllowedInSession(email) ==
            MultiProfileUserController::ALLOWED);
    user_dict->SetString(kKeyMultiProfilesPolicy, behavior);
  } else {
    user_dict->SetBoolean(kKeyMultiProfilesAllowed, true);
  }

  if (is_public_account) {
    policy::BrowserPolicyConnectorChromeOS* policy_connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();

    if (policy_connector->IsEnterpriseManaged()) {
      user_dict->SetString(kKeyEnterpriseDomain,
                           policy_connector->GetEnterpriseDomain());
    }
  }
}

void SigninScreenHandler::SendUserList(bool animated) {
  if (!delegate_)
    return;
  TRACE_EVENT_ASYNC_STEP_INTO0("ui",
                               "ShowLoginWebUI",
                               LoginDisplayHostImpl::kShowLoginWebUIid,
                               "SendUserList");
  BootTimesLoader::Get()->RecordCurrentStats("login-send-user-list");

  base::ListValue users_list;
  const UserList& users = delegate_->GetUsers();

  // TODO(nkostylev): Move to a separate method in UserManager.
  // http://crbug.com/230852
  bool is_signin_to_add = LoginDisplayHostImpl::default_host() &&
      UserManager::Get()->IsUserLoggedIn();

  user_pod_button_callback_map_.clear();
  user_auth_type_map_.clear();

  bool single_user = users.size() == 1;
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
  bool has_owner = owner.size() > 0;
  size_t max_non_owner_users = has_owner ? kMaxUsers - 1 : kMaxUsers;
  size_t non_owner_count = 0;
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->
          browser_policy_connector_chromeos();
  bool is_enterprise_managed = connector->IsEnterpriseManaged();


  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    const std::string& email = (*it)->email();
    bool is_owner = (email == owner);
    bool is_public_account =
        ((*it)->GetType() == User::USER_TYPE_PUBLIC_ACCOUNT);

    if ((is_public_account && !is_signin_to_add) ||
        is_owner ||
        (!is_public_account && non_owner_count < max_non_owner_users)) {
      LoginDisplay::AuthType initial_auth_type =
          ShouldForceOnlineSignIn(*it) ? LoginDisplay::ONLINE_SIGN_IN
                                       : LoginDisplay::OFFLINE_PASSWORD;
      user_auth_type_map_[email] = initial_auth_type;

      base::DictionaryValue* user_dict = new base::DictionaryValue();
      FillUserDictionary(
          *it, is_owner, is_signin_to_add, initial_auth_type, user_dict);
      bool signed_in = (*it)->is_logged_in();
      // Single user check here is necessary because owner info might not be
      // available when running into login screen on first boot.
      // See http://crosbug.com/12723
      bool can_remove_user = ((!single_user || is_enterprise_managed) &&
          !email.empty() && !is_owner && !is_public_account &&
          !signed_in && !is_signin_to_add);
      user_dict->SetBoolean(kKeyCanRemove, can_remove_user);

      if (is_owner) {
        users_list.Insert(0, user_dict);
      } else {
        ++non_owner_count;
        users_list.Append(user_dict);
      }
    }
  }
  while (users_list.GetSize() > kMaxUsers)
    users_list.Remove(kMaxUsers, NULL);

  CallJS("login.AccountPickerScreen.loadUsers", users_list, animated,
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
    HandleToggleResetScreen();
    return;
  }

  is_account_picker_showing_first_time_ = true;
  MaybePreloadAuthExtension();

  if (ScreenLocker::default_screen_locker())
    ScreenLocker::default_screen_locker()->delegate()->OnLockWebUIReady();

  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void SigninScreenHandler::HandleWallpaperReady() {
  if (ScreenLocker::default_screen_locker()) {
    ScreenLocker::default_screen_locker()->delegate()->
        OnLockBackgroundDisplayed();
  }
}

void SigninScreenHandler::HandleLoginWebuiReady() {
  if (focus_stolen_) {
    // Set focus to the Gaia page.
    // TODO(altimofeev): temporary solution, until focus parameters are
    // implemented on the Gaia side.
    // Do this only once. Any subsequent call would relod GAIA frame.
    focus_stolen_ = false;
    const char code[] =
        "if (typeof gWindowOnLoad != 'undefined) gWindowOnLoad();";
    content::RenderFrameHost* frame =
        LoginDisplayHostImpl::GetGaiaAuthIframe(web_ui()->GetWebContents());
    frame->ExecuteJavaScript(base::ASCIIToUTF16(code));
  }
  if (gaia_silent_load_) {
    focus_stolen_ = true;
    // Prevent focus stealing by the Gaia page.
    // TODO(altimofeev): temporary solution, until focus parameters are
    // implemented on the Gaia side.
    const char code[] = "var gWindowOnLoad = window.onload; "
                        "window.onload=function() {};";
    content::RenderFrameHost* frame =
        LoginDisplayHostImpl::GetGaiaAuthIframe(web_ui()->GetWebContents());
    frame->ExecuteJavaScript(base::ASCIIToUTF16(code));
    // As we could miss and window.onload could already be called, restore
    // focus to current pod (see crbug/175243).
    RefocusCurrentPod();
  }
  DCHECK(gaia_screen_handler_);
  gaia_screen_handler_->HandleFrameLoadingCompleted(0);

  if (test_expects_complete_login_)
    SubmitLoginFormForTest();
}

void SigninScreenHandler::HandleSignOutUser() {
  if (delegate_)
    delegate_->Signout();
}

void SigninScreenHandler::HandleCreateAccount() {
  if (delegate_)
    delegate_->CreateAccount();
}

void SigninScreenHandler::HandleOpenProxySettings() {
  LoginDisplayHostImpl::default_host()->OpenProxySettings();
}

void SigninScreenHandler::HandleLoginVisible(const std::string& source) {
  LOG(WARNING) << "Login WebUI >> loginVisible, src: " << source << ", "
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
}

void SigninScreenHandler::HandleCancelPasswordChangedFlow() {
  StartClearingCookies(base::Bind(
      &SigninScreenHandler::CancelPasswordChangedFlowInternal,
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
                                                    bool new_value) {
  VLOG(0) << "Login WebUI >> active: " << new_value << ", "
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
  DCHECK(UserManager::Get()->IsUserLoggedIn());
  if (ScreenLocker::default_screen_locker())
    ScreenLocker::default_screen_locker()->UnlockOnLoginSuccess();
}

void SigninScreenHandler::HandleShowLoadingTimeoutError() {
  UpdateState(ErrorScreenActor::ERROR_REASON_LOADING_TIMEOUT);
}

void SigninScreenHandler::HandleUpdateOfflineLogin(bool offline_login_active) {
  offline_login_active_ = offline_login_active;
}

void SigninScreenHandler::HandleFocusPod(const std::string& user_id) {
  SetUserInputMethod(user_id);
  WallpaperManager::Get()->SetUserWallpaperDelayed(user_id);
}

void SigninScreenHandler::HandleCustomButtonClicked(
    const std::string& username) {
  if (user_pod_button_callback_map_.find(username)
      == user_pod_button_callback_map_.end()) {
    LOG(WARNING) << "User pod custom button clicked but no callback found";
    return;
  }
  user_pod_button_callback_map_[username].Run();
}

void SigninScreenHandler::HandleRetrieveAuthenticatedUserEmail(
    double attempt_token) {
  email_retriever_.reset(new AuthenticatedUserEmailRetriever(
      base::Bind(&SigninScreenHandler::CallJS<double, std::string>,
                 base::Unretained(this),
                 "login.GaiaSigninScreen.setAuthenticatedUserEmail",
                 attempt_token),
      Profile::FromWebUI(web_ui())->GetRequestContext()));
}

void SigninScreenHandler::HandleLaunchKioskApp(const std::string& app_id,
                                               bool diagnostic_mode) {
  delegate_->LoginAsKioskApp(app_id, diagnostic_mode);
}

void SigninScreenHandler::StartClearingDnsCache() {
  if (dns_clear_task_running_ || !g_browser_process->io_thread())
    return;

  dns_cleared_ = false;
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ClearDnsCache, g_browser_process->io_thread()),
      base::Bind(&SigninScreenHandler::OnDnsCleared,
                 weak_factory_.GetWeakPtr()));
  dns_clear_task_running_ = true;
}

void SigninScreenHandler::StartClearingCookies(
    const base::Closure& on_clear_callback) {
  cookies_cleared_ = false;
  ProfileHelper* profile_helper =
      g_browser_process->platform_part()->profile_helper();
  LOG_ASSERT(
      Profile::FromWebUI(web_ui()) == profile_helper->GetSigninProfile());
  profile_helper->ClearSigninProfile(base::Bind(
      &SigninScreenHandler::OnCookiesCleared,
      weak_factory_.GetWeakPtr(), on_clear_callback));
}

void SigninScreenHandler::MaybePreloadAuthExtension() {
  LOG(WARNING) << "MaybePreloadAuthExtension() call.";

  // Fetching of the extension is not started before account picker page is
  // loaded because it can affect the loading speed. Also if cookies clearing
  // was initiated or |dns_clear_task_running_| then auth extension showing has
  // already been initiated and preloading is senseless.
  // Do not load the extension for the screen locker, see crosbug.com/25018.
  if (is_account_picker_showing_first_time_ &&
      !gaia_silent_load_ &&
      !ScreenLocker::default_screen_locker() &&
      !cookies_cleared_ &&
      !dns_clear_task_running_ &&
      network_state_informer_->state() == NetworkStateInformer::ONLINE) {
    gaia_silent_load_ = true;
    gaia_silent_load_network_ = network_state_informer_->network_path();
    LoadAuthExtension(true, true, false);
  }
}

bool SigninScreenHandler::AllWhitelistedUsersPresent() {
  CrosSettings* cros_settings = CrosSettings::Get();
  bool allow_new_user = false;
  cros_settings->GetBoolean(kAccountsPrefAllowNewUser, &allow_new_user);
  if (allow_new_user)
    return false;
  UserManager* user_manager = UserManager::Get();
  const UserList& users = user_manager->GetUsers();
  if (!delegate_ || users.size() > kMaxUsers) {
    return false;
  }
  const base::ListValue* whitelist = NULL;
  if (!cros_settings->GetList(kAccountsPrefUsers, &whitelist) || !whitelist)
    return false;
  for (size_t i = 0; i < whitelist->GetSize(); ++i) {
    std::string whitelisted_user;
    // NB: Wildcards in the whitelist are also detected as not present here.
    if (!whitelist->GetString(i, &whitelisted_user) ||
        !user_manager->IsKnownUser(whitelisted_user)) {
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

OobeUI::Screen SigninScreenHandler::GetCurrentScreen() const {
  OobeUI::Screen screen = OobeUI::SCREEN_UNKNOWN;
  OobeUI* oobe_ui = static_cast<OobeUI*>(web_ui()->GetController());
  if (oobe_ui)
    screen = oobe_ui->current_screen();
  return screen;
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
  return (GetCurrentScreen() == OobeUI::SCREEN_ERROR_MESSAGE) &&
      (IsSigninScreen(error_screen_actor_->parent_screen()));
}

bool SigninScreenHandler::IsGuestSigninAllowed() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings)
    return false;
  bool allow_guest;
  cros_settings->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  return allow_guest;
}

bool SigninScreenHandler::IsOfflineLoginAllowed() const {
  CrosSettings* cros_settings = CrosSettings::Get();
  if (!cros_settings)
    return false;

  // Offline login is allowed only when user pods are hidden.
  bool show_pods;
  cros_settings->GetBoolean(kAccountsPrefShowUserNamesOnSignIn, &show_pods);
  return !show_pods;
}

void SigninScreenHandler::SubmitLoginFormForTest() {
  VLOG(2) << "Submit login form for test, user=" << test_user_;

  std::string code;
  code += "document.getElementById('Email').value = '" + test_user_ + "';";
  code += "document.getElementById('Passwd').value = '" + test_pass_ + "';";
  code += "document.getElementById('signIn').click();";

  content::RenderFrameHost* frame =
      LoginDisplayHostImpl::GetGaiaAuthIframe(web_ui()->GetWebContents());
  frame->ExecuteJavaScript(base::ASCIIToUTF16(code));

  // Test properties are cleared in HandleCompleteLogin because the form
  // submission might fail and login will not be attempted after reloading
  // if they are cleared here.
}

void SigninScreenHandler::ContinueKioskEnableFlow(
    policy::AutoEnrollmentState state) {
  // Do not proceed with kiosk enable when auto enroll will be enforced.
  // TODO(xiyuan): Add an error UI feedkback so user knows what happens.
  switch (state) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
      // Wait for the next callback.
      return;
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
      // Auto-enrollment is on.
      LOG(WARNING) << "Kiosk enable flow aborted because auto enrollment is "
                      "going to be enforced.";
      if (!kiosk_enable_flow_aborted_callback_for_test_.is_null())
        kiosk_enable_flow_aborted_callback_for_test_.Run();
      break;
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
      // Auto-enrollment not applicable.
      if (delegate_)
        delegate_->ShowKioskEnableScreen();
      break;
  }
  auto_enrollment_progress_subscription_.reset();
}

void SigninScreenHandler::OnShowAddUser(const std::string& email) {
  email_ = email;
  is_account_picker_showing_first_time_ = false;

  if (gaia_silent_load_ && email_.empty()) {
    dns_cleared_ = true;
    cookies_cleared_ = true;
    ShowSigninScreenIfReady();
  } else {
    StartClearingDnsCache();
    StartClearingCookies(base::Bind(
        &SigninScreenHandler::ShowSigninScreenIfReady,
        weak_factory_.GetWeakPtr()));
  }
}

void SigninScreenHandler::SetSAMLPrincipalsAPIUsed(bool api_used) {
  using_saml_api_ = api_used;
  UMA_HISTOGRAM_BOOLEAN("ChromeOS.SAML.APIUsed", api_used);
}

GaiaScreenHandler::FrameState SigninScreenHandler::FrameState() const {
  DCHECK(gaia_screen_handler_);
  return gaia_screen_handler_->frame_state();
}

net::Error SigninScreenHandler::FrameError() const {
  DCHECK(gaia_screen_handler_);
  return gaia_screen_handler_->frame_error();
}

}  // namespace chromeos
