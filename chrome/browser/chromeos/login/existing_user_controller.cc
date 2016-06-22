// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/boot_times_recorder.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"
#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_user_context_initializer.h"
#include "chrome/browser/chromeos/login/easy_unlock/bootstrap_user_flow.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_token_initializer.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/device_disabling_manager.h"
#include "chrome/browser/signin/easy_unlock_service.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/user_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "policy/policy_constants.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// Enum types for Login.PasswordChangeFlow.
// Don't change the existing values and update LoginPasswordChangeFlow in
// histogram.xml when making changes here.
enum LoginPasswordChangeFlow {
  // User is sent to the password changed flow. This is the normal case.
  LOGIN_PASSWORD_CHANGE_FLOW_PASSWORD_CHANGED = 0,
  // User is sent to the unrecoverable cryptohome failure flow. This is the
  // case when http://crbug.com/547857 happens.
  LOGIN_PASSWORD_CHANGE_FLOW_CRYPTOHOME_FAILURE = 1,

  LOGIN_PASSWORD_CHANGE_FLOW_COUNT,  // Must be the last entry.
};

// Delay for transferring the auth cache to the system profile.
const long int kAuthCacheTransferDelayMs = 2000;

// Delay for restarting the ui if safe-mode login has failed.
const long int kSafeModeRestartUiDelayMs = 30000;

// Makes a call to the policy subsystem to reload the policy when we detect
// authentication change.
void RefreshPoliciesOnUIThread() {
  if (g_browser_process->policy_service())
    g_browser_process->policy_service()->RefreshPolicies(base::Closure());
}

// Copies any authentication details that were entered in the login profile in
// the mail profile to make sure all subsystems of Chrome can access the network
// with the provided authentication which are possibly for a proxy server.
void TransferContextAuthenticationsOnIOThread(
    net::URLRequestContextGetter* default_profile_context_getter,
    net::URLRequestContextGetter* browser_process_context_getter) {
  net::HttpAuthCache* new_cache =
      browser_process_context_getter->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache();
  net::HttpAuthCache* old_cache =
      default_profile_context_getter->GetURLRequestContext()->
      http_transaction_factory()->GetSession()->http_auth_cache();
  new_cache->UpdateAllFrom(*old_cache);
  VLOG(1) << "Main request context populated with authentication data.";
  // Last but not least tell the policy subsystem to refresh now as it might
  // have been stuck until now too.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(&RefreshPoliciesOnUIThread));
}

// Record UMA for password login of regular user when Easy sign-in is enabled.
void RecordPasswordLoginEvent(const UserContext& user_context) {
  EasyUnlockService* easy_unlock_service =
      EasyUnlockService::Get(ProfileHelper::GetSigninProfile());
  if (user_context.GetUserType() == user_manager::USER_TYPE_REGULAR &&
      user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE &&
      easy_unlock_service) {
    easy_unlock_service->RecordPasswordLoginEvent(user_context.GetAccountId());
  }
}

bool CanShowDebuggingFeatures() {
  // We need to be on the login screen and in dev mode to show this menu item.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kSystemDevMode) &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             chromeos::switches::kLoginManager) &&
         !user_manager::UserManager::Get()->IsSessionStarted();
}

void RecordPasswordChangeFlow(LoginPasswordChangeFlow flow) {
  UMA_HISTOGRAM_ENUMERATION("Login.PasswordChangeFlow", flow,
                            LOGIN_PASSWORD_CHANGE_FLOW_COUNT);
}

}  // namespace

// static
ExistingUserController* ExistingUserController::current_controller_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, public:

ExistingUserController::ExistingUserController(LoginDisplayHost* host)
    : host_(host),
      login_display_(host_->CreateLoginDisplay(this)),
      cros_settings_(CrosSettings::Get()),
      network_state_helper_(new login::NetworkStateHelper),
      weak_factory_(this) {
  DCHECK(current_controller_ == nullptr);
  current_controller_ = this;

  registrar_.Add(this,
                 chrome::NOTIFICATION_USER_LIST_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
  show_user_names_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefShowUserNamesOnSignIn,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  allow_new_user_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowNewUser,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  allow_guest_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefAllowGuest,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  allow_supervised_user_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefSupervisedUsersEnabled,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  users_subscription_ = cros_settings_->AddSettingsObserver(
      kAccountsPrefUsers,
      base::Bind(&ExistingUserController::DeviceSettingsChanged,
                 base::Unretained(this)));
  local_account_auto_login_id_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          base::Bind(&ExistingUserController::ConfigurePublicSessionAutoLogin,
                     base::Unretained(this)));
  local_account_auto_login_delay_subscription_ =
      cros_settings_->AddSettingsObserver(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          base::Bind(&ExistingUserController::ConfigurePublicSessionAutoLogin,
                     base::Unretained(this)));
}

void ExistingUserController::Init(const user_manager::UserList& users) {
  time_init_ = base::Time::Now();
  UpdateLoginDisplay(users);
  ConfigurePublicSessionAutoLogin();
}

void ExistingUserController::UpdateLoginDisplay(
    const user_manager::UserList& users) {
  bool show_users_on_signin;
  user_manager::UserList filtered_users;

  cros_settings_->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                             &show_users_on_signin);
  for (const auto& user : users) {
    // Skip kiosk apps for login screen user list. Kiosk apps as pods (aka new
    // kiosk UI) is currently disabled and it gets the apps directly from
    // KioskAppManager.
    if (user->GetType() == user_manager::USER_TYPE_KIOSK_APP)
      continue;

    // TODO(xiyuan): Clean user profile whose email is not in whitelist.
    const bool meets_supervised_requirements =
        user->GetType() != user_manager::USER_TYPE_SUPERVISED ||
        user_manager::UserManager::Get()->AreSupervisedUsersAllowed();
    const bool meets_whitelist_requirements =
        CrosSettings::IsWhitelisted(user->email(), nullptr) ||
        !user->HasGaiaAccount();

    // Public session accounts are always shown on login screen.
    const bool meets_show_users_requirements =
        show_users_on_signin ||
        user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    if (meets_supervised_requirements &&
        meets_whitelist_requirements &&
        meets_show_users_requirements) {
      filtered_users.push_back(user);
    }
  }

  // If no user pods are visible, fallback to single new user pod which will
  // have guest session link.
  bool show_guest;
  cros_settings_->GetBoolean(kAccountsPrefAllowGuest, &show_guest);
  show_users_on_signin |= !filtered_users.empty();
  show_guest &= !filtered_users.empty();
  bool show_new_user = true;
  login_display_->set_parent_window(GetNativeWindow());
  login_display_->Init(
      filtered_users, show_guest, show_users_on_signin, show_new_user);
  host_->OnPreferencesChanged();
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, content::NotificationObserver implementation:
//

void ExistingUserController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_SESSION_STARTED) {
    // Stop listening to any notification once session has started.
    // Sign in screen objects are marked for deletion with DeleteSoon so
    // make sure no object would be used after session has started.
    // http://crbug.com/125276
    registrar_.RemoveAll();
    return;
  }
  if (type == chrome::NOTIFICATION_USER_LIST_CHANGED) {
    DeviceSettingsChanged();
    return;
  }
  if (type == chrome::NOTIFICATION_AUTH_SUPPLIED) {
    // Possibly the user has authenticated against a proxy server and we might
    // need the credentials for enrollment and other system requests from the
    // main |g_browser_process| request context (see bug
    // http://crosbug.com/24861). So we transfer any credentials to the global
    // request context here.
    // The issue we have here is that the NOTIFICATION_AUTH_SUPPLIED is sent
    // just after the UI is closed but before the new credentials were stored
    // in the profile. Therefore we have to give it some time to make sure it
    // has been updated before we copy it.
    VLOG(1) << "Authentication was entered manually, possibly for proxyauth.";
    scoped_refptr<net::URLRequestContextGetter> browser_process_context_getter =
        g_browser_process->system_request_context();
    Profile* signin_profile = ProfileHelper::GetSigninProfile();
    scoped_refptr<net::URLRequestContextGetter> signin_profile_context_getter =
        signin_profile->GetRequestContext();
    DCHECK(browser_process_context_getter.get());
    DCHECK(signin_profile_context_getter.get());
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&TransferContextAuthenticationsOnIOThread,
                   base::RetainedRef(signin_profile_context_getter),
                   base::RetainedRef(browser_process_context_getter)),
        base::TimeDelta::FromMilliseconds(kAuthCacheTransferDelayMs));
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  UserSessionManager::GetInstance()->DelegateDeleted(this);

  if (current_controller_ == this) {
    current_controller_ = nullptr;
  } else {
    NOTREACHED() << "More than one controller are alive.";
  }
  DCHECK(login_display_.get());
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginDisplay::Delegate implementation:
//

void ExistingUserController::CancelPasswordChangedFlow() {
  login_performer_.reset(nullptr);
  PerformLoginFinishedActions(true /* start public session timer */);
}

void ExistingUserController::CompleteLogin(const UserContext& user_context) {
  if (!host_) {
    // Complete login event was generated already from UI. Ignore notification.
    return;
  }

  if (is_login_in_progress_)
    return;

  is_login_in_progress_ = true;

  ContinueLoginIfDeviceNotDisabled(base::Bind(
      &ExistingUserController::DoCompleteLogin,
      weak_factory_.GetWeakPtr(),
      user_context));
}

base::string16 ExistingUserController::GetConnectedNetworkName() {
  return network_state_helper_->GetCurrentNetworkName();
}

bool ExistingUserController::IsSigninInProgress() const {
  return is_login_in_progress_;
}

void ExistingUserController::Login(const UserContext& user_context,
                                   const SigninSpecifics& specifics) {
  if (is_login_in_progress_) {
    // If there is another login in progress, bail out. Do not re-enable
    // clicking on other windows and the status area. Do not start the
    // auto-login timer.
    return;
  }

  is_login_in_progress_ = true;

  if (user_context.GetUserType() != user_manager::USER_TYPE_REGULAR &&
      user_manager::UserManager::Get()->IsUserLoggedIn()) {
    // Multi-login is only allowed for regular users. If we are attempting to
    // do multi-login as another type of user somehow, bail out. Do not
    // re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer.
    return;
  }

  ContinueLoginIfDeviceNotDisabled(base::Bind(
      &ExistingUserController::DoLogin,
      weak_factory_.GetWeakPtr(),
      user_context,
      specifics));
}

void ExistingUserController::PerformLogin(
    const UserContext& user_context,
    LoginPerformer::AuthorizationMode auth_mode) {
  VLOG(1) << "Setting flow from PerformLogin";
  ChromeUserManager::Get()
      ->GetUserFlow(user_context.GetAccountId())
      ->SetHost(host_);

  BootTimesRecorder::Get()->RecordLoginAttempted();

  // Use the same LoginPerformer for subsequent login as it has state
  // such as Authenticator instance.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(nullptr);
    login_performer_.reset(new ChromeLoginPerformer(this));
  }

  if (gaia::ExtractDomainName(user_context.GetAccountId().GetUserEmail()) ==
      chromeos::login::kSupervisedUserDomain) {
    login_performer_->LoginAsSupervisedUser(user_context);
  } else {
    login_performer_->PerformLogin(user_context, auth_mode);
    RecordPasswordLoginEvent(user_context);
  }
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNING_IN));
}

void ExistingUserController::MigrateUserData(const std::string& old_password) {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get()) {
    VLOG(1) << "Migrate the existing cryptohome to new password.";
    login_performer_->RecoverEncryptedData(old_password);
  }
}

void ExistingUserController::OnSigninScreenReady() {
  signin_screen_ready_ = true;
  StartPublicSessionAutoLoginTimer();
}

void ExistingUserController::OnStartEnterpriseEnrollment() {
  if (KioskAppManager::Get()->IsConsumerKioskDeviceWithAutoLaunch()) {
    LOG(WARNING) << "Enterprise enrollment is not available after kiosk auto "
                    "launch is set.";
    return;
  }

  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
                 weak_factory_.GetWeakPtr()));
}

void ExistingUserController::OnStartEnableDebuggingScreen() {
  if (CanShowDebuggingFeatures())
    ShowEnableDebuggingScreen();
}

void ExistingUserController::OnStartKioskEnableScreen() {
  KioskAppManager::Get()->GetConsumerKioskAutoLaunchStatus(
      base::Bind(
          &ExistingUserController::OnConsumerKioskAutoLaunchCheckCompleted,
          weak_factory_.GetWeakPtr()));
}

void ExistingUserController::OnStartKioskAutolaunchScreen() {
  ShowKioskAutolaunchScreen();
}

void ExistingUserController::ResyncUserData() {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get()) {
    VLOG(1) << "Create a new cryptohome and resync user data.";
    login_performer_->ResyncEncryptedData();
  }
}

void ExistingUserController::SetDisplayEmail(const std::string& email) {
  display_email_ = email;
}

void ExistingUserController::ShowWrongHWIDScreen() {
  host_->StartWizard(WizardController::kWrongHWIDScreenName);
}

void ExistingUserController::Signout() {
  NOTREACHED();
}

bool ExistingUserController::IsUserWhitelisted(const AccountId& account_id) {
  bool wildcard_match = false;
  if (login_performer_.get())
    return login_performer_->IsUserWhitelisted(account_id, &wildcard_match);

  return chromeos::CrosSettings::IsWhitelisted(account_id.GetUserEmail(),
                                               &wildcard_match);
}

void ExistingUserController::OnConsumerKioskAutoLaunchCheckCompleted(
    KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  if (status == KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE)
    ShowKioskEnableScreen();
}

void ExistingUserController::OnEnrollmentOwnershipCheckCompleted(
    DeviceSettingsService::OwnershipStatus status) {
  if (status == DeviceSettingsService::OWNERSHIP_NONE) {
    ShowEnrollmentScreen();
  } else if (status == DeviceSettingsService::OWNERSHIP_TAKEN) {
    // On a device that is already owned we might want to allow users to
    // re-enroll if the policy information is invalid.
    CrosSettingsProvider::TrustedStatus trusted_status =
        CrosSettings::Get()->PrepareTrustedValues(
            base::Bind(
                &ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
                weak_factory_.GetWeakPtr(), status));
    if (trusted_status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
      ShowEnrollmentScreen();
    }
  } else {
    // OwnershipService::GetStatusAsync is supposed to return either
    // OWNERSHIP_NONE or OWNERSHIP_TAKEN.
    NOTREACHED();
  }
}

void ExistingUserController::ShowEnrollmentScreen() {
  host_->StartWizard(WizardController::kEnrollmentScreenName);
}

void ExistingUserController::ShowResetScreen() {
  host_->StartWizard(WizardController::kResetScreenName);
}

void ExistingUserController::ShowEnableDebuggingScreen() {
  host_->StartWizard(WizardController::kEnableDebuggingScreenName);
}

void ExistingUserController::ShowKioskEnableScreen() {
  host_->StartWizard(WizardController::kKioskEnableScreenName);
}

void ExistingUserController::ShowKioskAutolaunchScreen() {
  host_->StartWizard(WizardController::kKioskAutolaunchScreenName);
}

void ExistingUserController::ShowTPMError() {
  login_display_->SetUIEnabled(false);
  login_display_->ShowErrorScreen(LoginDisplay::TPM_ERROR);
}

void ExistingUserController::ShowPasswordChangedDialog() {
  RecordPasswordChangeFlow(LOGIN_PASSWORD_CHANGE_FLOW_PASSWORD_CHANGED);

  VLOG(1) << "Show password changed dialog"
          << ", count=" << login_performer_->password_changed_callback_count();

  // True if user has already made an attempt to enter old password and failed.
  bool show_invalid_old_password_error =
      login_performer_->password_changed_callback_count() > 1;

  // Note: We allow owner using "full sync" mode which will recreate
  // cryptohome and deal with owner private key being lost. This also allows
  // us to recover from a lost owner password/homedir.
  // TODO(gspencer): We shouldn't have to erase stateful data when
  // doing this.  See http://crosbug.com/9115 http://crosbug.com/7792
  login_display_->ShowPasswordChangedDialog(show_invalid_old_password_error,
                                            display_email_);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnAuthFailure(const AuthFailure& failure) {
  guest_mode_url_ = GURL::EmptyGURL();
  std::string error = failure.GetErrorString();

  PerformLoginFinishedActions(false /* don't start public session timer */);

  if (ChromeUserManager::Get()
          ->GetUserFlow(last_login_attempt_account_id_)
          ->HandleLoginFailure(failure)) {
    return;
  }

  if (failure.reason() == AuthFailure::OWNER_REQUIRED) {
    ShowError(IDS_LOGIN_ERROR_OWNER_REQUIRED, error);
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&SessionManagerClient::StopSession,
                   base::Unretained(DBusThreadManager::Get()->
                                    GetSessionManagerClient())),
        base::TimeDelta::FromMilliseconds(kSafeModeRestartUiDelayMs));
  } else if (failure.reason() == AuthFailure::TPM_ERROR) {
    ShowTPMError();
  } else if (last_login_attempt_account_id_ == login::GuestAccountId()) {
    // Show no errors, just re-enable input.
    login_display_->ClearAndEnablePassword();
    StartPublicSessionAutoLoginTimer();
  } else {
    // Check networking after trying to login in case user is
    // cached locally or the local admin account.
    const bool is_known_user = user_manager::UserManager::Get()->IsKnownUser(
        last_login_attempt_account_id_);
    if (!network_state_helper_->IsConnected()) {
      if (is_known_user)
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      else
        ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
    } else {
      // TODO(nkostylev): Cleanup rest of ClientLogin related code.
      if (failure.reason() == AuthFailure::NETWORK_AUTH_FAILED &&
          failure.error().state() ==
              GoogleServiceAuthError::HOSTED_NOT_ALLOWED) {
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED, error);
      } else {
        if (!is_known_user)
          ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_NEW, error);
        else
          ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      }
    }
    if (auth_flow_offline_)
      UMA_HISTOGRAM_BOOLEAN("Login.OfflineFailure.IsKnownUser", is_known_user);

    login_display_->ClearAndEnablePassword();
    StartPublicSessionAutoLoginTimer();
  }

  // Reset user flow to default, so that special flow will not affect next
  // attempt.
  ChromeUserManager::Get()->ResetUserFlow(last_login_attempt_account_id_);

  if (auth_status_consumer_)
    auth_status_consumer_->OnAuthFailure(failure);

  // Clear the recorded displayed email so it won't affect any future attempts.
  display_email_.clear();

  // TODO(ginkage): Fix this case once crbug.com/469990 is ready.
  /*
    if (failure.reason() == AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME) {
      RecordReauthReason(last_login_attempt_account_id_,
                         ReauthReason::MISSING_CRYPTOHOME);
    }
  */
}

void ExistingUserController::OnAuthSuccess(const UserContext& user_context) {
  is_login_in_progress_ = false;
  login_display_->set_signin_completed(true);

  // Login performer will be gone so cache this value to use
  // once profile is loaded.
  password_changed_ = login_performer_->password_changed();
  auth_mode_ = login_performer_->auth_mode();

  ChromeUserManager::Get()
      ->GetUserFlow(user_context.GetAccountId())
      ->HandleLoginSuccess(user_context);

  StopPublicSessionAutoLoginTimer();

  // Truth table of |has_auth_cookies|:
  //                          Regular        SAML
  //  /ServiceLogin              T            T
  //  /ChromeOsEmbeddedSetup     F            T
  //  Bootstrap experiment       F            N/A
  const bool has_auth_cookies =
      login_performer_->auth_mode() == LoginPerformer::AUTH_MODE_EXTENSION &&
      (user_context.GetAccessToken().empty() ||
       user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITH_SAML) &&
      user_context.GetAuthFlow() != UserContext::AUTH_FLOW_EASY_BOOTSTRAP;

  // LoginPerformer instance will delete itself in case of successful auth.
  login_performer_->set_delegate(nullptr);
  ignore_result(login_performer_.release());

  if (user_context.GetAuthFlow() == UserContext::AUTH_FLOW_OFFLINE)
    UMA_HISTOGRAM_COUNTS_100("Login.OfflineSuccess.Attempts",
                             num_login_attempts_);

  UserSessionManager::StartSessionType start_session_type =
      UserAddingScreen::Get()->IsRunning()
          ? UserSessionManager::SECONDARY_USER_SESSION
          : UserSessionManager::PRIMARY_USER_SESSION;
  UserSessionManager::GetInstance()->StartSession(
      user_context, start_session_type, has_auth_cookies,
      false,  // Start session for user.
      this);

  // Update user's displayed email.
  if (!display_email_.empty()) {
    user_manager::UserManager::Get()->SaveUserDisplayEmail(
        user_context.GetAccountId(), display_email_);
    display_email_.clear();
  }
}

void ExistingUserController::OnProfilePrepared(Profile* profile,
                                               bool browser_launched) {
  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);

  if (browser_launched)
    host_ = nullptr;

  // Inform |auth_status_consumer_| about successful login.
  // TODO(nkostylev): Pass UserContext back crbug.com/424550
  if (auth_status_consumer_) {
    auth_status_consumer_->OnAuthSuccess(
        UserContext(last_login_attempt_account_id_));
  }
}

void ExistingUserController::OnOffTheRecordAuthSuccess() {
  is_login_in_progress_ = false;

  // Mark the device as registered., i.e. the second part of OOBE as completed.
  if (!StartupUtils::IsDeviceRegistered())
    StartupUtils::MarkDeviceRegistered(base::Closure());

  UserSessionManager::GetInstance()->CompleteGuestSessionLogin(guest_mode_url_);

  if (auth_status_consumer_)
    auth_status_consumer_->OnOffTheRecordAuthSuccess();
}

void ExistingUserController::OnPasswordChangeDetected() {
  is_login_in_progress_ = false;

  // Must not proceed without signature verification.
  if (CrosSettingsProvider::TRUSTED != cros_settings_->PrepareTrustedValues(
      base::Bind(&ExistingUserController::OnPasswordChangeDetected,
                 weak_factory_.GetWeakPtr()))) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  if (ChromeUserManager::Get()
          ->GetUserFlow(last_login_attempt_account_id_)
          ->HandlePasswordChangeDetected()) {
    return;
  }

  if (auth_status_consumer_)
    auth_status_consumer_->OnPasswordChangeDetected();

  // If the password change happens after an online auth, do a TokenHandle check
  // to find out whether the user password is really changed or not.
  // TODO(xiyuan): Remove channel restriction. See http://crbug.com/585530
  if (chrome::GetChannel() <= version_info::Channel::DEV &&
      auth_mode() == LoginPerformer::AUTH_MODE_EXTENSION) {
    token_handle_util_.reset(new TokenHandleUtil);
    if (token_handle_util_->HasToken(last_login_attempt_account_id_)) {
      token_handle_util_->CheckToken(
          last_login_attempt_account_id_,
          base::Bind(&ExistingUserController::OnTokenHandleChecked,
                     weak_factory_.GetWeakPtr()));
      return;
    }
  }

  ShowPasswordChangedDialog();
}

void ExistingUserController::WhiteListCheckFailed(const std::string& email) {
  PerformLoginFinishedActions(true /* start public session timer */);

  login_display_->ShowWhitelistCheckFailedError();

  if (auth_status_consumer_) {
    auth_status_consumer_->OnAuthFailure(
        AuthFailure(AuthFailure::WHITELIST_CHECK_FAILED));
  }

  display_email_.clear();
}

void ExistingUserController::PolicyLoadFailed() {
  ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, "");

  PerformLoginFinishedActions(false /* don't start public session timer */);
  display_email_.clear();
}

void ExistingUserController::SetAuthFlowOffline(bool offline) {
  auth_flow_offline_ = offline;
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

void ExistingUserController::DeviceSettingsChanged() {
  // If login was already completed, we should avoid any signin screen
  // transitions, see http://crbug.com/461604 for example.
  if (host_ != nullptr && !login_display_->is_signin_completed()) {
    // Signed settings or user list changed. Notify views and update them.
    UpdateLoginDisplay(user_manager::UserManager::Get()->GetUsers());
    ConfigurePublicSessionAutoLogin();
  }
}

LoginPerformer::AuthorizationMode ExistingUserController::auth_mode() const {
  if (login_performer_)
    return login_performer_->auth_mode();

  return auth_mode_;
}

bool ExistingUserController::password_changed() const {
  if (login_performer_)
    return login_performer_->password_changed();

  return password_changed_;
}

void ExistingUserController::LoginAsGuest() {
  PerformPreLoginActions(
      UserContext(user_manager::USER_TYPE_GUEST, login::GuestAccountId()));

  bool allow_guest;
  cros_settings_->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  if (!allow_guest) {
    // Disallowed. The UI should normally not show the guest session button.
    LOG(ERROR) << "Guest login attempt when guest mode is disallowed.";
    PerformLoginFinishedActions(true /* start public session timer */);
    display_email_.clear();
    return;
  }

  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(nullptr);
  login_performer_.reset(new ChromeLoginPerformer(this));
  login_performer_->LoginOffTheRecord();
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_OFFRECORD));
}

void ExistingUserController::LoginAsPublicSession(
    const UserContext& user_context) {
  PerformPreLoginActions(user_context);

  // If there is no public account with the given user ID, logging in is not
  // possible.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    PerformLoginFinishedActions(true /* start public session timer */);
    return;
  }

  UserContext new_user_context = user_context;
  std::string locale = user_context.GetPublicSessionLocale();
  if (locale.empty()) {
    // When performing auto-login, no locale is chosen by the user. Check
    // whether a list of recommended locales was set by policy. If so, use its
    // first entry. Otherwise, |locale| will remain blank, indicating that the
    // public session should use the current UI locale.
    const policy::PolicyMap::Entry* entry =
        g_browser_process->platform_part()
            ->browser_policy_connector_chromeos()
            ->GetDeviceLocalAccountPolicyService()
            ->GetBrokerForUser(user_context.GetAccountId().GetUserEmail())
            ->core()
            ->store()
            ->policy_map()
            .Get(policy::key::kSessionLocales);
    base::ListValue const* list = nullptr;
    if (entry &&
        entry->level == policy::POLICY_LEVEL_RECOMMENDED &&
        entry->value &&
        entry->value->GetAsList(&list)) {
      if (list->GetString(0, &locale))
        new_user_context.SetPublicSessionLocale(locale);
    }
  }

  if (!locale.empty() &&
      new_user_context.GetPublicSessionInputMethod().empty()) {
    // When |locale| is set, a suitable keyboard layout should be chosen. In
    // most cases, this will already be the case because the UI shows a list of
    // keyboard layouts suitable for the |locale| and ensures that one of them
    // us selected. However, it is still possible that |locale| is set but no
    // keyboard layout was chosen:
    // * The list of keyboard layouts is updated asynchronously. If the user
    //   enters the public session before the list of keyboard layouts for the
    //   |locale| has been retrieved, the UI will indicate that no keyboard
    //   layout was chosen.
    // * During auto-login, the |locale| is set in this method and a suitable
    //   keyboard layout must be chosen next.
    //
    // The list of suitable keyboard layouts is constructed asynchronously. Once
    // it has been retrieved, |SetPublicSessionKeyboardLayoutAndLogin| will
    // select the first layout from the list and continue login.
    GetKeyboardLayoutsForLocale(
        base::Bind(
            &ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin,
            weak_factory_.GetWeakPtr(),
            new_user_context),
        locale);
    return;
  }

  // The user chose a locale and a suitable keyboard layout or left both unset.
  // Login can continue immediately.
  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsKioskApp(const std::string& app_id,
                                             bool diagnostic_mode) {
  const bool auto_start = false;
  host_->StartAppLaunch(app_id, diagnostic_mode, auto_start);
}

void ExistingUserController::ConfigurePublicSessionAutoLogin() {
  std::string auto_login_account_id;
  cros_settings_->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                            &auto_login_account_id);
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);

  public_session_auto_login_account_id_ = EmptyAccountId();
  for (std::vector<policy::DeviceLocalAccount>::const_iterator
           it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->account_id == auto_login_account_id) {
      public_session_auto_login_account_id_ =
          AccountId::FromUserEmail(it->user_id);
      break;
    }
  }

  const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
      public_session_auto_login_account_id_);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    public_session_auto_login_account_id_ = EmptyAccountId();

  if (!cros_settings_->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          &public_session_auto_login_delay_)) {
    public_session_auto_login_delay_ = 0;
  }

  if (public_session_auto_login_account_id_.is_valid())
    StartPublicSessionAutoLoginTimer();
  else
    StopPublicSessionAutoLoginTimer();
}

void ExistingUserController::ResetPublicSessionAutoLoginTimer() {
  // Only restart the auto-login timer if it's already running.
  if (auto_login_timer_ && auto_login_timer_->IsRunning()) {
    StopPublicSessionAutoLoginTimer();
    StartPublicSessionAutoLoginTimer();
  }
}

void ExistingUserController::OnPublicSessionAutoLoginTimerFire() {
  CHECK(signin_screen_ready_ &&
        public_session_auto_login_account_id_.is_valid());
  Login(UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                    public_session_auto_login_account_id_),
        SigninSpecifics());
}

void ExistingUserController::StopPublicSessionAutoLoginTimer() {
  if (auto_login_timer_)
    auto_login_timer_->Stop();
}

void ExistingUserController::StartPublicSessionAutoLoginTimer() {
  if (!signin_screen_ready_ || is_login_in_progress_ ||
      !public_session_auto_login_account_id_.is_valid()) {
    return;
  }

  // Start the auto-login timer.
  if (!auto_login_timer_)
    auto_login_timer_.reset(new base::OneShotTimer);

  auto_login_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          public_session_auto_login_delay_),
      base::Bind(
          &ExistingUserController::OnPublicSessionAutoLoginTimerFire,
          weak_factory_.GetWeakPtr()));
}

gfx::NativeWindow ExistingUserController::GetNativeWindow() const {
  return host_->GetNativeWindow();
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  VLOG(1) << details;
  HelpAppLauncher::HelpTopic help_topic_id;
  if (login_performer_) {
    switch (login_performer_->error().state()) {
      case GoogleServiceAuthError::ACCOUNT_DISABLED:
        help_topic_id = HelpAppLauncher::HELP_ACCOUNT_DISABLED;
        break;
      case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
        help_topic_id = HelpAppLauncher::HELP_HOSTED_ACCOUNT;
        break;
      default:
        help_topic_id = HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT;
        break;
    }
  } else {
    // login_performer_ will be null if an error occurred during OAuth2 token
    // fetch. In this case, show a generic error.
    help_topic_id = HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT;
  }

  if (error_id == IDS_LOGIN_ERROR_AUTHENTICATING) {
    if (num_login_attempts_ > 1) {
      const user_manager::User* user =
          user_manager::UserManager::Get()->FindUser(
              last_login_attempt_account_id_);
      if (user && (user->GetType() == user_manager::USER_TYPE_SUPERVISED))
        error_id = IDS_LOGIN_ERROR_AUTHENTICATING_2ND_TIME_SUPERVISED;
    }
  }

  login_display_->ShowError(error_id, num_login_attempts_, help_topic_id);
}

void ExistingUserController::SendAccessibilityAlert(
    const std::string& alert_text) {
  AutomationManagerAura::GetInstance()->HandleAlert(
      ProfileHelper::GetSigninProfile(), alert_text);
}

void ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin(
    const UserContext& user_context,
    std::unique_ptr<base::ListValue> keyboard_layouts) {
  UserContext new_user_context = user_context;
  std::string keyboard_layout;
  for (size_t i = 0; i < keyboard_layouts->GetSize(); ++i) {
    base::DictionaryValue* entry = nullptr;
    keyboard_layouts->GetDictionary(i, &entry);
    bool selected = false;
    entry->GetBoolean("selected", &selected);
    if (selected) {
      entry->GetString("value", &keyboard_layout);
      break;
    }
  }
  DCHECK(!keyboard_layout.empty());
  new_user_context.SetPublicSessionInputMethod(keyboard_layout);

  LoginAsPublicSessionInternal(new_user_context);
}

void ExistingUserController::LoginAsPublicSessionInternal(
    const UserContext& user_context) {
  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(nullptr);
  login_performer_.reset(new ChromeLoginPerformer(this));
  login_performer_->LoginAsPublicSession(user_context);
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_PUBLIC_ACCOUNT));
}

void ExistingUserController::PerformPreLoginActions(
    const UserContext& user_context) {
  // Disable clicking on other windows and status tray.
  login_display_->SetUIEnabled(false);

  if (last_login_attempt_account_id_ != user_context.GetAccountId()) {
    last_login_attempt_account_id_ = user_context.GetAccountId();
    num_login_attempts_ = 0;
  }
  num_login_attempts_++;

  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();
}

void ExistingUserController::PerformLoginFinishedActions(
    bool start_public_session_timer) {
  is_login_in_progress_ = false;

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);

  if (start_public_session_timer)
    StartPublicSessionAutoLoginTimer();
}

void ExistingUserController::ContinueLoginIfDeviceNotDisabled(
    const base::Closure& continuation) {
  // Disable clicking on other windows and status tray.
  login_display_->SetUIEnabled(false);

  // Stop the auto-login timer.
  StopPublicSessionAutoLoginTimer();

  // Wait for the |cros_settings_| to become either trusted or permanently
  // untrusted.
  const CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::Bind(
          &ExistingUserController::ContinueLoginIfDeviceNotDisabled,
          weak_factory_.GetWeakPtr(),
          continuation));
  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED)
    return;

  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // If the |cros_settings_| are permanently untrusted, show an error message
    // and refuse to log in.
    login_display_->ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST,
                              1,
                              HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. Without trusted |cros_settings_|, no auto-login
    // can succeed.
    login_display_->SetUIEnabled(true);
    return;
  }

  bool device_disabled = false;
  cros_settings_->GetBoolean(kDeviceDisabled, &device_disabled);
  if (device_disabled && system::DeviceDisablingManager::
                             HonorDeviceDisablingDuringNormalOperation()) {
    // If the device is disabled, bail out. A device disabled screen will be
    // shown by the DeviceDisablingManager.

    // Re-enable clicking on other windows and the status area. Do not start the
    // auto-login timer though. On a disabled device, no auto-login can succeed.
    login_display_->SetUIEnabled(true);
    return;
  }

  continuation.Run();
}

void ExistingUserController::DoCompleteLogin(
    const UserContext& user_context_wo_device_id) {
  UserContext user_context = user_context_wo_device_id;
  std::string device_id =
      user_manager::known_user::GetDeviceId(user_context.GetAccountId());
  if (device_id.empty()) {
    bool is_ephemeral = ChromeUserManager::Get()->AreEphemeralUsersEnabled() &&
                        user_context.GetAccountId() !=
                            ChromeUserManager::Get()->GetOwnerAccountId();
    device_id = SigninClient::GenerateSigninScopedDeviceID(is_ephemeral);
  }
  user_context.SetDeviceId(device_id);

  const std::string& gaps_cookie = user_context.GetGAPSCookie();
  if (!gaps_cookie.empty()) {
    user_manager::known_user::SetGAPSCookie(user_context.GetAccountId(),
                                            gaps_cookie);
  }

  PerformPreLoginActions(user_context);

  if (!time_init_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_init_;
    UMA_HISTOGRAM_MEDIUM_TIMES("Login.PromptToCompleteLoginTime", delta);
    time_init_ = base::Time();  // Reset to null.
  }

  host_->OnCompleteLogin();

  if (user_context.GetAuthFlow() == UserContext::AUTH_FLOW_EASY_BOOTSTRAP) {
    bootstrap_user_context_initializer_.reset(
        new BootstrapUserContextInitializer());
    bootstrap_user_context_initializer_->Start(
        user_context.GetAuthCode(),
        base::Bind(&ExistingUserController::OnBootstrapUserContextInitialized,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  // Fetch OAuth2 tokens if we have an auth code and are not using SAML.
  // SAML uses cookies to get tokens.
  if (user_context.GetAuthFlow() == UserContext::AUTH_FLOW_GAIA_WITHOUT_SAML &&
      !user_context.GetAuthCode().empty()) {
    oauth2_token_initializer_.reset(new OAuth2TokenInitializer);
    oauth2_token_initializer_->Start(
        user_context, base::Bind(&ExistingUserController::OnOAuth2TokensFetched,
                                 weak_factory_.GetWeakPtr()));
    return;
  }

  PerformLogin(user_context, LoginPerformer::AUTH_MODE_EXTENSION);
}

void ExistingUserController::DoLogin(const UserContext& user_context,
                                     const SigninSpecifics& specifics) {
  if (user_context.GetUserType() == user_manager::USER_TYPE_GUEST) {
    if (!specifics.guest_mode_url.empty()) {
      guest_mode_url_ = GURL(specifics.guest_mode_url);
      if (specifics.guest_mode_url_append_locale)
        guest_mode_url_ = google_util::AppendGoogleLocaleParam(
            guest_mode_url_, g_browser_process->GetApplicationLocale());
    }
    LoginAsGuest();
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    LoginAsPublicSession(user_context);
    return;
  }

  if (user_context.GetUserType() == user_manager::USER_TYPE_KIOSK_APP) {
    LoginAsKioskApp(user_context.GetAccountId().GetUserEmail(),
                    specifics.kiosk_diagnostic_mode);
    return;
  }

  // Regular user or supervised user login.

  if (!user_context.HasCredentials()) {
    // If credentials are missing, refuse to log in.

    // Reenable clicking on other windows and status area.
    login_display_->SetUIEnabled(true);
    // Restart the auto-login timer.
    StartPublicSessionAutoLoginTimer();
  }

  PerformPreLoginActions(user_context);
  PerformLogin(user_context, LoginPerformer::AUTH_MODE_INTERNAL);
}

void ExistingUserController::OnBootstrapUserContextInitialized(
    bool success,
    const UserContext& user_context) {
  if (!success) {
    LOG(ERROR) << "Easy bootstrap failed.";
    OnAuthFailure(AuthFailure(AuthFailure::NETWORK_AUTH_FAILED));
    return;
  }

  // Setting a customized login user flow to perform additional initializations
  // for bootstrap after the user session is started.
  ChromeUserManager::Get()->SetUserFlow(
      user_context.GetAccountId(),
      new BootstrapUserFlow(
          user_context,
          bootstrap_user_context_initializer_->random_key_used()));

  PerformLogin(user_context, LoginPerformer::AUTH_MODE_EXTENSION);
}

void ExistingUserController::OnOAuth2TokensFetched(
    bool success,
    const UserContext& user_context) {
  if (!success) {
    LOG(ERROR) << "OAuth2 token fetch failed.";
    OnAuthFailure(AuthFailure(AuthFailure::FAILED_TO_INITIALIZE_TOKEN));
    return;
  }
  UserSessionManager::GetInstance()->OnOAuth2TokensFetched(user_context);
  PerformLogin(user_context, LoginPerformer::AUTH_MODE_EXTENSION);
}

void ExistingUserController::OnTokenHandleChecked(
    const AccountId&,
    TokenHandleUtil::TokenHandleStatus token_handle_status) {
  // If TokenHandle is invalid or unknown, continue with regular password
  // changed flow.
  if (token_handle_status != TokenHandleUtil::VALID) {
    VLOG(1) << "Checked TokenHandle status=" << token_handle_status;
    ShowPasswordChangedDialog();
    return;
  }

  // Otherwise, show the unrecoverable cryptohome error UI and ask user's
  // permission to collect a feedback.
  RecordPasswordChangeFlow(LOGIN_PASSWORD_CHANGE_FLOW_CRYPTOHOME_FAILURE);
  VLOG(1) << "Show unrecoverable cryptohome error dialog.";
  login_display_->ShowUnrecoverableCrypthomeErrorDialog();
}

}  // namespace chromeos
