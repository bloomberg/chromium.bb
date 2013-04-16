// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/connectivity_state_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/generated_resources.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// Major version where we still show GSG as "Release Notes" after the update.
const long int kReleaseNotesTargetRelease = 19;

// Getting started guide url, will be opened as in app window for each new
// user who logs on the device.
// It actually goes to HelpApp and get redirected to our actual server there.
const char kGetStartedWebUrl[] =
#if defined(OFFICIAL_BUILD)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/gsg.html?oobe=1";
#else
    "https://gweb-gettingstartedguide.appspot.com/";
#endif  // defined(OFFICIAL_BUILD)

// Getting started guide application window size.
const char kGSGAppWindowSize[] = "820,600";

// URL for account creation.
const char kCreateAccountURL[] =
    "https://accounts.google.com/NewAccount?service=mail";

// ChromeVox tutorial URL (used in place of "getting started" url when
// accessibility is enabled).
const char kChromeVoxTutorialURLPattern[] =
    "http://www.chromevox.com/tutorial/index.html?lang=%s";

// Delay for transferring the auth cache to the system profile.
const long int kAuthCacheTransferDelayMs = 2000;

// Delay for restarting the ui if safe-mode login has failed.
const long int kSafeModeRestartUiDelayMs = 30000;

// Delay for rebooting machine if TPM critical error was encountered.
const long int kCriticalErrorRebootDelayMs = 3500;

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

}  // namespace

// static
ExistingUserController* ExistingUserController::current_controller_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, public:

ExistingUserController::ExistingUserController(LoginDisplayHost* host)
    : login_status_consumer_(NULL),
      host_(host),
      login_display_(host_->CreateLoginDisplay(this)),
      num_login_attempts_(0),
      cros_settings_(CrosSettings::Get()),
      weak_factory_(this),
      offline_failed_(false),
      is_login_in_progress_(false),
      password_changed_(false),
      do_auto_enrollment_(false),
      signin_screen_ready_(false) {
  DCHECK(current_controller_ == NULL);
  current_controller_ = this;

  registrar_.Add(this,
                 chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_USER_LIST_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTH_SUPPLIED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_SESSION_STARTED,
                 content::NotificationService::AllSources());
  cros_settings_->AddSettingsObserver(kAccountsPrefShowUserNamesOnSignIn, this);
  cros_settings_->AddSettingsObserver(kAccountsPrefAllowNewUser, this);
  cros_settings_->AddSettingsObserver(kAccountsPrefAllowGuest, this);
  cros_settings_->AddSettingsObserver(kAccountsPrefUsers, this);
  cros_settings_->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccountAutoLoginId,
      this);
  cros_settings_->AddSettingsObserver(
      kAccountsPrefDeviceLocalAccountAutoLoginDelay,
      this);
}

void ExistingUserController::Init(const UserList& users) {
  time_init_ = base::Time::Now();
  UpdateLoginDisplay(users);
  ConfigurePublicSessionAutoLogin();

  LoginUtils::Get()->PrewarmAuthentication();
  DBusThreadManager::Get()->GetSessionManagerClient()->EmitLoginPromptReady();
}

void ExistingUserController::UpdateLoginDisplay(const UserList& users) {
  bool show_users_on_signin;
  UserList filtered_users;

  cros_settings_->GetBoolean(kAccountsPrefShowUserNamesOnSignIn,
                             &show_users_on_signin);
  if (show_users_on_signin) {
    for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
      // TODO(xiyuan): Clean user profile whose email is not in whitelist.
      if (LoginUtils::IsWhitelisted((*it)->email()) ||
          (*it)->GetType() != User::USER_TYPE_REGULAR) {
        filtered_users.push_back(*it);
      }
    }
  }

  // If no user pods are visible, fallback to single new user pod which will
  // have guest session link.
  bool show_guest;
  cros_settings_->GetBoolean(kAccountsPrefAllowGuest, &show_guest);
  bool show_users;
  cros_settings_->GetBoolean(kAccountsPrefShowUserNamesOnSignIn, &show_users);
  show_guest &= !filtered_users.empty();
  bool show_new_user = true;
  login_display_->set_parent_window(GetNativeWindow());
  login_display_->Init(filtered_users, show_guest, show_users, show_new_user);
  host_->OnPreferencesChanged();
}

void ExistingUserController::DoAutoEnrollment() {
  do_auto_enrollment_ = true;
}

void ExistingUserController::ResumeLogin() {
  // This means the user signed-in, then auto-enrollment used his credentials
  // to enroll and succeeded.
  resume_login_callback_.Run();
  resume_login_callback_.Reset();
}

void ExistingUserController::PrepareKioskAppLaunch() {
  // Disable login UI while waiting for the kiosk app launch. There is no
  // balanced UI enable call because this very login screen will not be
  // accessed again. If app is launched, it will be destroyed. If app fails to
  // launch, chrome is restarted to go back to a new login screen.
  login_display_->SetUIEnabled(false);
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
  if (type == chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED) {
    const std::string setting = *content::Details<const std::string>(
        details).ptr();
    if (setting == kAccountsPrefDeviceLocalAccountAutoLoginId ||
        setting == kAccountsPrefDeviceLocalAccountAutoLoginDelay) {
      ConfigurePublicSessionAutoLogin();
    }
  }
  if (type == chrome::NOTIFICATION_SYSTEM_SETTING_CHANGED ||
      type == chrome::NOTIFICATION_USER_LIST_CHANGED) {
    if (host_ != NULL) {
      // Signed settings or user list changed. Notify views and update them.
      UpdateLoginDisplay(chromeos::UserManager::Get()->GetUsers());
      return;
    }
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
    LOG(INFO) << "Authentication was entered manually, possibly for proxyauth.";
    scoped_refptr<net::URLRequestContextGetter> browser_process_context_getter =
        g_browser_process->system_request_context();
    Profile* default_profile = ProfileManager::GetDefaultProfile();
    scoped_refptr<net::URLRequestContextGetter> default_profile_context_getter =
        default_profile->GetRequestContext();
    DCHECK(browser_process_context_getter.get());
    DCHECK(default_profile_context_getter.get());
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&TransferContextAuthenticationsOnIOThread,
                   default_profile_context_getter,
                   browser_process_context_getter),
        base::TimeDelta::FromMilliseconds(kAuthCacheTransferDelayMs));
  }
  if (type != chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED)
    return;
  login_display_->OnUserImageChanged(*content::Details<User>(details).ptr());
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  LoginUtils::Get()->DelegateDeleted(this);

  cros_settings_->RemoveSettingsObserver(kAccountsPrefShowUserNamesOnSignIn,
                                         this);
  cros_settings_->RemoveSettingsObserver(kAccountsPrefAllowNewUser, this);
  cros_settings_->RemoveSettingsObserver(kAccountsPrefAllowGuest, this);
  cros_settings_->RemoveSettingsObserver(kAccountsPrefUsers, this);
  cros_settings_->RemoveSettingsObserver(
      kAccountsPrefDeviceLocalAccountAutoLoginId,
      this);
  cros_settings_->RemoveSettingsObserver(
      kAccountsPrefDeviceLocalAccountAutoLoginDelay,
      this);

  if (current_controller_ == this) {
    current_controller_ = NULL;
  } else {
    NOTREACHED() << "More than one controller are alive.";
  }
  DCHECK(login_display_.get());
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginDisplay::Delegate implementation:
//

void ExistingUserController::CancelPasswordChangedFlow() {
  login_performer_.reset(NULL);
  login_display_->SetUIEnabled(true);
  StartPublicSessionAutoLoginTimer();
}

void ExistingUserController::CreateAccount() {
  content::RecordAction(content::UserMetricsAction("Login.CreateAccount"));
  guest_mode_url_ =
      google_util::AppendGoogleLocaleParam(GURL(kCreateAccountURL));
  LoginAsGuest();
}

void ExistingUserController::CompleteLogin(const UserContext& user_context) {
  if (!host_) {
    // Complete login event was generated already from UI. Ignore notification.
    return;
  }

  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();

  // Disable UI while loading user profile.
  login_display_->SetUIEnabled(false);

  if (!time_init_.is_null()) {
    base::TimeDelta delta = base::Time::Now() - time_init_;
    UMA_HISTOGRAM_MEDIUM_TIMES("Login.PromptToCompleteLoginTime", delta);
    time_init_ = base::Time();  // Reset to null.
  }

  host_->OnCompleteLogin();

  // Do an ownership check now to avoid auto-enrolling if the device has
  // already been owned.
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&ExistingUserController::CompleteLoginInternal,
                 weak_factory_.GetWeakPtr(),
                 user_context));
}

void ExistingUserController::CompleteLoginInternal(
    const UserContext& user_context,
    DeviceSettingsService::OwnershipStatus ownership_status,
    bool is_owner) {
  // Auto-enrollment must have made a decision by now. It's too late to enroll
  // if the protocol isn't done at this point.
  if (do_auto_enrollment_ &&
      ownership_status == DeviceSettingsService::OWNERSHIP_NONE) {
    VLOG(1) << "Forcing auto-enrollment before completing login";
    // The only way to get out of the enrollment screen from now on is to either
    // complete enrollment, or opt-out of it. So this controller shouldn't force
    // enrollment again if it is reused for another sign-in.
    do_auto_enrollment_ = false;
    auto_enrollment_username_ = user_context.username;
    resume_login_callback_ = base::Bind(
        &ExistingUserController::PerformLogin,
        weak_factory_.GetWeakPtr(),
        user_context, LoginPerformer::AUTH_MODE_EXTENSION);
    ShowEnrollmentScreen(true, user_context.username);
    // Enable UI for the enrollment screen. SetUIEnabled(true) will post a
    // request to show the sign-in screen again when invoked at the sign-in
    // screen; invoke SetUIEnabled() after navigating to the enrollment screen.
    login_display_->SetUIEnabled(true);
  } else {
    PerformLogin(user_context, LoginPerformer::AUTH_MODE_EXTENSION);
  }
}

string16 ExistingUserController::GetConnectedNetworkName() {
  return GetCurrentNetworkName();
}

void ExistingUserController::Login(const UserContext& user_context) {
  if ((user_context.username.empty() || user_context.password.empty()) &&
      user_context.auth_code.empty())
    return;

  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();

  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  BootTimesLoader::Get()->RecordLoginAttempted();

  if (last_login_attempt_username_ != user_context.username) {
    last_login_attempt_username_ = user_context.username;
    num_login_attempts_ = 0;
    // Also reset state variables, which are used to determine password change.
    offline_failed_ = false;
    online_succeeded_for_.clear();
  }
  num_login_attempts_++;
  PerformLogin(user_context, LoginPerformer::AUTH_MODE_INTERNAL);
}

void ExistingUserController::PerformLogin(
    const UserContext& user_context,
    LoginPerformer::AuthorizationMode auth_mode) {
  UserManager::Get()->GetUserFlow(last_login_attempt_username_)->
      set_host(host_);

  // Disable UI while loading user profile.
  login_display_->SetUIEnabled(false);

  // Use the same LoginPerformer for subsequent login as it has state
  // such as Authenticator instance.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    LoginPerformer::Delegate* delegate = this;
    if (login_performer_delegate_.get())
      delegate = login_performer_delegate_.get();
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(NULL);
    login_performer_.reset(new LoginPerformer(delegate));
  }

  is_login_in_progress_ = true;
  if (gaia::ExtractDomainName(user_context.username) ==
          UserManager::kLocallyManagedUserDomain) {
    login_performer_->LoginAsLocallyManagedUser(
        UserContext(user_context.username,
                    user_context.password,
                    std::string()));  // auth_code
  } else {
    login_performer_->PerformLogin(user_context, auth_mode);
  }
  accessibility::MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNING_IN));
}

void ExistingUserController::LoginAsRetailModeUser() {
  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();

  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);
  // TODO(rkc): Add a CHECK to make sure retail mode logins are allowed once
  // the enterprise policy wiring is done for retail mode.

  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(NULL);
  login_performer_.reset(new LoginPerformer(this));
  is_login_in_progress_ = true;
  login_performer_->LoginRetailMode();
  accessibility::MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_DEMOUSER));
}

void ExistingUserController::LoginAsGuest() {
  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();

  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ExistingUserController::LoginAsGuest,
                     weak_factory_.GetWeakPtr()));
  // Must not proceed without signature verification.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    login_display_->ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, 1,
                              HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    // Reenable clicking on other windows and status area.
    login_display_->SetUIEnabled(true);
    StartPublicSessionAutoLoginTimer();
    display_email_.clear();
    return;
  } else if (status != CrosSettingsProvider::TRUSTED) {
    // Value of AllowNewUser setting is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  bool allow_guest;
  cros_settings_->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
  if (!allow_guest) {
    // Disallowed. The UI should normally not show the guest pod but if for some
    // reason this has been made available to the user here is the time to tell
    // this nicely.
    login_display_->ShowError(IDS_LOGIN_ERROR_WHITELIST, 1,
                              HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    // Reenable clicking on other windows and status area.
    login_display_->SetUIEnabled(true);
    StartPublicSessionAutoLoginTimer();
    display_email_.clear();
    return;
  }

  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(NULL);
  login_performer_.reset(new LoginPerformer(this));
  is_login_in_progress_ = true;
  login_performer_->LoginOffTheRecord();
  accessibility::MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_OFFRECORD));
}

void ExistingUserController::MigrateUserData(const std::string& old_password) {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get())
    login_performer_->RecoverEncryptedData(old_password);
}

void ExistingUserController::LoginAsPublicAccount(
    const std::string& username) {
  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();

  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ExistingUserController::LoginAsPublicAccount,
                     weak_factory_.GetWeakPtr(),
                     username));
  // If device policy is permanently unavailable, logging into public accounts
  // is not possible.
  if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    login_display_->ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, 1,
                              HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    // Re-enable clicking on other windows.
    login_display_->SetUIEnabled(true);
    return;
  }

  // If device policy is not verified yet, this function will be called again
  // when verification finishes.
  if (status != CrosSettingsProvider::TRUSTED)
    return;

  // If there is no public account with the given |username|, logging in is not
  // possible.
  const User* user = UserManager::Get()->FindUser(username);
  if (!user || user->GetType() != User::USER_TYPE_PUBLIC_ACCOUNT) {
    // Re-enable clicking on other windows.
    login_display_->SetUIEnabled(true);
    StartPublicSessionAutoLoginTimer();
    return;
  }

  // Only one instance of LoginPerformer should exist at a time.
  login_performer_.reset(NULL);
  login_performer_.reset(new LoginPerformer(this));
  is_login_in_progress_ = true;
  login_performer_->LoginAsPublicAccount(username);
  accessibility::MaybeSpeak(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_PUBLIC_ACCOUNT));
}

void ExistingUserController::OnSigninScreenReady() {
  signin_screen_ready_ = true;
  StartPublicSessionAutoLoginTimer();
}

void ExistingUserController::OnUserSelected(const std::string& username) {
  login_performer_.reset(NULL);
  num_login_attempts_ = 0;
}

void ExistingUserController::OnStartEnterpriseEnrollment() {
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
                 weak_factory_.GetWeakPtr()));
}

void ExistingUserController::OnStartDeviceReset() {
  ShowResetScreen();
}

void ExistingUserController::ResyncUserData() {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get())
    login_performer_->ResyncEncryptedData();
}

void ExistingUserController::SetDisplayEmail(const std::string& email) {
  display_email_ = email;
}

void ExistingUserController::ShowWrongHWIDScreen() {
  host_->StartWizard(WizardController::kWrongHWIDScreenName, NULL);
  login_display_->OnFadeOut();
}

void ExistingUserController::Signout() {
  NOTREACHED();
}

void ExistingUserController::OnEnrollmentOwnershipCheckCompleted(
    DeviceSettingsService::OwnershipStatus status,
    bool current_user_is_owner) {
  if (status == DeviceSettingsService::OWNERSHIP_NONE) {
    ShowEnrollmentScreen(false, std::string());
  } else if (status == DeviceSettingsService::OWNERSHIP_TAKEN) {
    // On a device that is already owned we might want to allow users to
    // re-enroll if the policy information is invalid.
    CrosSettingsProvider::TrustedStatus trusted_status =
        CrosSettings::Get()->PrepareTrustedValues(
            base::Bind(
                &ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
                weak_factory_.GetWeakPtr(), status, current_user_is_owner));
    if (trusted_status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED)
      ShowEnrollmentScreen(false, std::string());
  } else {
    // OwnershipService::GetStatusAsync is supposed to return either
    // OWNERSHIP_NONE or OWNERSHIP_TAKEN.
    NOTREACHED();
  }
}

void ExistingUserController::ShowEnrollmentScreen(bool is_auto_enrollment,
                                                  const std::string& user) {
  DictionaryValue* params = NULL;
  if (is_auto_enrollment) {
    params = new DictionaryValue;
    params->SetBoolean("is_auto_enrollment", true);
    params->SetString("user", user);
  }
  host_->StartWizard(WizardController::kEnterpriseEnrollmentScreenName, params);
  login_display_->OnFadeOut();
}

void ExistingUserController::ShowResetScreen() {
  host_->StartWizard(WizardController::kResetScreenName, NULL);
  login_display_->OnFadeOut();
}

void ExistingUserController::ShowTPMError() {
  login_display_->SetUIEnabled(false);
  login_display_->ShowErrorScreen(LoginDisplay::TPM_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnLoginFailure(const LoginFailure& failure) {
  is_login_in_progress_ = false;
  offline_failed_ = true;

  guest_mode_url_ = GURL::EmptyGURL();
  std::string error = failure.GetErrorString();

  if (UserManager::Get()->GetUserFlow(last_login_attempt_username_)->
          HandleLoginFailure(failure)) {
    return;
  }

  if (failure.reason() == LoginFailure::OWNER_REQUIRED) {
    ShowError(IDS_LOGIN_ERROR_OWNER_REQUIRED, error);
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&SessionManagerClient::StopSession,
                   base::Unretained(DBusThreadManager::Get()->
                                    GetSessionManagerClient())),
        base::TimeDelta::FromMilliseconds(kSafeModeRestartUiDelayMs));
  } else if (failure.reason() == LoginFailure::TPM_ERROR) {
    ShowTPMError();
  } else if (!online_succeeded_for_.empty()) {
    ShowGaiaPasswordChanged(online_succeeded_for_);
  } else {
    // Check networking after trying to login in case user is
    // cached locally or the local admin account.
    bool is_known_user =
        UserManager::Get()->IsKnownUser(last_login_attempt_username_);
    if (!ConnectivityStateHelper::Get()->IsConnected()) {
      if (is_known_user)
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      else
        ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
    } else {
      // TODO(nkostylev): Cleanup rest of ClientLogin related code.
      if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
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
    // Reenable clicking on other windows and status area.
    login_display_->SetUIEnabled(true);
    StartPublicSessionAutoLoginTimer();
  }

  // Reset user flow to default, so that special flow will not affect next
  // attempt.
  UserManager::Get()->ResetUserFlow(last_login_attempt_username_);

  if (login_status_consumer_)
    login_status_consumer_->OnLoginFailure(failure);

  // Clear the recorded displayed email so it won't affect any future attempts.
  display_email_.clear();
}

void ExistingUserController::OnLoginSuccess(
    const UserContext& user_context,
    bool pending_requests,
    bool using_oauth) {
  is_login_in_progress_ = false;
  offline_failed_ = false;

  StopPublicSessionAutoLoginTimer();

  bool has_cookies =
      login_performer_->auth_mode() == LoginPerformer::AUTH_MODE_EXTENSION &&
      user_context.auth_code.empty();

  // Login performer will be gone so cache this value to use
  // once profile is loaded.
  password_changed_ = login_performer_->password_changed();

  // LoginPerformer instance will delete itself once online auth result is OK.
  // In case of failure it'll bring up ScreenLock and ask for
  // correct password/display error message.
  // Even in case when following online,offline protocol and returning
  // requests_pending = false, let LoginPerformer delete itself.
  login_performer_->set_delegate(NULL);
  ignore_result(login_performer_.release());

  // Will call OnProfilePrepared() in the end.
  LoginUtils::Get()->PrepareProfile(user_context,
                                    display_email_,
                                    using_oauth,
                                    has_cookies,
                                    this);

  display_email_.clear();

  // Notify LoginDisplay to allow it provide visual feedback to user.
  login_display_->OnLoginSuccess(user_context.username);
}

void ExistingUserController::OnProfilePrepared(Profile* profile) {
  OptionallyShowReleaseNotes(profile);

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);

  if (UserManager::Get()->IsCurrentUserNew() &&
      !UserManager::Get()->GetCurrentUserFlow()->ShouldSkipPostLoginScreens() &&
      !WizardController::default_controller()->skip_post_login_screens()) {
    // Don't specify start URLs if the administrator has configured the start
    // URLs via policy.
    if (!SessionStartupPref::TypeIsManaged(profile->GetPrefs()))
      InitializeStartUrls();
#ifndef NDEBUG
    if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOobeSkipPostLogin)) {
      LoginUtils::Get()->DoBrowserLaunch(profile, host_);
      host_ = NULL;
    } else {
#endif
      ActivateWizard(WizardController::IsDeviceRegistered() ?
          WizardController::kTermsOfServiceScreenName :
          WizardController::kRegistrationScreenName);
#ifndef NDEBUG
    }
#endif
  } else {
    LoginUtils::Get()->DoBrowserLaunch(profile, host_);
    host_ = NULL;
  }
  // Inform |login_status_consumer_| about successful login. Set most
  // parameters to empty since they're not needed.
  if (login_status_consumer_) {
    login_status_consumer_->OnLoginSuccess(UserContext(),
                                           false,    // pending_requests
                                           false);   // using_oauth
  }
  login_display_->OnFadeOut();
}

void ExistingUserController::OnOffTheRecordLoginSuccess() {
  is_login_in_progress_ = false;
  offline_failed_ = false;
  if (WizardController::IsDeviceRegistered()) {
    LoginUtils::Get()->CompleteOffTheRecordLogin(guest_mode_url_);
  } else {
    // Postpone CompleteOffTheRecordLogin until registration completion.
    // TODO(nkostylev): Kind of hack. We have to instruct UserManager here
    // that we're actually logged in as Guest user as we'll ask UserManager
    // later in the code path whether we've signed in as Guest and depending
    // on that would either show image screen or call CompleteOffTheRecordLogin.
    UserManager::Get()->GuestUserLoggedIn();
    ActivateWizard(WizardController::kRegistrationScreenName);
  }

  if (login_status_consumer_)
    login_status_consumer_->OnOffTheRecordLoginSuccess();
}

void ExistingUserController::OnPasswordChangeDetected() {
  // Must not proceed without signature verification.
  if (CrosSettingsProvider::TRUSTED != cros_settings_->PrepareTrustedValues(
      base::Bind(&ExistingUserController::OnPasswordChangeDetected,
                 weak_factory_.GetWeakPtr()))) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  if (UserManager::Get()->GetUserFlow(last_login_attempt_username_)->
          HandlePasswordChangeDetected()) {
    return;
  }

  // True if user has already made an attempt to enter old password and failed.
  bool show_invalid_old_password_error =
      login_performer_->password_changed_callback_count() > 1;

  // Note: We allow owner using "full sync" mode which will recreate
  // cryptohome and deal with owner private key being lost. This also allows
  // us to recover from a lost owner password/homedir.
  // TODO(gspencer): We shouldn't have to erase stateful data when
  // doing this.  See http://crosbug.com/9115 http://crosbug.com/7792
  login_display_->ShowPasswordChangedDialog(show_invalid_old_password_error);

  if (login_status_consumer_)
    login_status_consumer_->OnPasswordChangeDetected();

  display_email_.clear();
}

void ExistingUserController::WhiteListCheckFailed(const std::string& email) {
  ShowError(IDS_LOGIN_ERROR_WHITELIST, email);

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);
  login_display_->ShowSigninUI(email);

  if (login_status_consumer_) {
    login_status_consumer_->OnLoginFailure(LoginFailure(
          LoginFailure::WHITELIST_CHECK_FAILED));
  }

  display_email_.clear();

  StartPublicSessionAutoLoginTimer();
}

void ExistingUserController::PolicyLoadFailed() {
  ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, "");

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);

  display_email_.clear();

  // Policy load failure stops login attempts -- restart the timer.
  StartPublicSessionAutoLoginTimer();
}

void ExistingUserController::OnOnlineChecked(const std::string& username,
                                             bool success) {
  if (success && last_login_attempt_username_ == username) {
    online_succeeded_for_ = username;
    // Wait for login attempt to end, if it hasn't yet.
    if (offline_failed_ && !is_login_in_progress_)
      ShowGaiaPasswordChanged(username);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

void ExistingUserController::ActivateWizard(const std::string& screen_name) {
  DictionaryValue* params = NULL;
  if (chromeos::UserManager::Get()->IsLoggedInAsGuest()) {
    params = new DictionaryValue;
    params->SetString("start_url", guest_mode_url_.spec());
  }
  host_->StartWizard(screen_name, params);
}

void ExistingUserController::ConfigurePublicSessionAutoLogin() {
  if (!cros_settings_->GetString(
          kAccountsPrefDeviceLocalAccountAutoLoginId,
          &public_session_auto_login_username_)) {
    public_session_auto_login_username_.clear();
  }
  if (!cros_settings_->GetInteger(
          kAccountsPrefDeviceLocalAccountAutoLoginDelay,
          &public_session_auto_login_delay_)) {
    public_session_auto_login_delay_ = 0;
  }

  if (!public_session_auto_login_username_.empty())
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
        !is_login_in_progress_ &&
        !public_session_auto_login_username_.empty());
  LoginAsPublicAccount(public_session_auto_login_username_);
}

void ExistingUserController::StopPublicSessionAutoLoginTimer() {
  if (auto_login_timer_)
    auto_login_timer_->Stop();
}

void ExistingUserController::StartPublicSessionAutoLoginTimer() {
  if (!signin_screen_ready_ ||
      is_login_in_progress_ ||
      public_session_auto_login_username_.empty()) {
    return;
  }

  // Start the auto-login timer.
  if (!auto_login_timer_)
    auto_login_timer_.reset(new base::OneShotTimer<ExistingUserController>);

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

void ExistingUserController::InitializeStartUrls() const {
  std::vector<std::string> start_urls;

  PrefService* prefs = g_browser_process->local_state();
  const base::ListValue *urls;
  bool show_getstarted_guide = false;
  if (UserManager::Get()->IsLoggedInAsDemoUser()) {
    if (CrosSettings::Get()->GetList(kStartUpUrls, &urls)) {
      // The retail mode user will get start URLs from a special policy if it is
      // set.
      for (base::ListValue::const_iterator it = urls->begin();
           it != urls->end(); ++it) {
        std::string url;
        if ((*it)->GetAsString(&url))
          start_urls.push_back(url);
      }
    }
  // Skip the default first-run behavior for public accounts.
  } else if (!UserManager::Get()->IsLoggedInAsPublicAccount()) {
    if (prefs->GetBoolean(prefs::kSpokenFeedbackEnabled)) {
      const char* url = kChromeVoxTutorialURLPattern;
      const std::string current_locale =
          StringToLowerASCII(prefs->GetString(prefs::kApplicationLocale));
      std::string vox_url = base::StringPrintf(url, current_locale.c_str());
      start_urls.push_back(vox_url);
    } else {
      show_getstarted_guide = true;
    }
  }

  ServicesCustomizationDocument* customization =
      ServicesCustomizationDocument::GetInstance();
  if (!ServicesCustomizationDocument::WasApplied() &&
      customization->IsReady()) {
    // Since we don't use OEM start URL anymore, just mark as applied.
    customization->ApplyCustomization();
  }

  if (show_getstarted_guide) {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kApp, kGetStartedWebUrl);
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppWindowSize, kGSGAppWindowSize);
  } else {
    // We should not be adding any start URLs if guide
    // is defined as it launches as a standalone app window.
    for (size_t i = 0; i < start_urls.size(); ++i)
      CommandLine::ForCurrentProcess()->AppendArg(start_urls[i]);
  }
}

void ExistingUserController::OptionallyShowReleaseNotes(
    Profile* profile) const {
  // TODO(nkostylev): Fix WizardControllerFlowTest case.
  if (!profile || KioskModeSettings::Get()->IsKioskModeEnabled())
    return;
  if (UserManager::Get()->GetCurrentUserFlow()->ShouldSkipPostLoginScreens())
    return;
  PrefService* prefs = profile->GetPrefs();
  chrome::VersionInfo version_info;
  // New users would get this info with default getting started guide.
  // In password changed case 2 options are available:
  // 1. Cryptohome removed, pref is gone, not yet synced, recreate
  //    with latest version.
  // 2. Cryptohome migrated, pref is available. To simplify implementation
  //    update version here too. Unlikely that user signs in first time on
  //    the machine after update with password changed.
  if (UserManager::Get()->IsCurrentUserNew() || password_changed_) {
    prefs->SetString(prefs::kChromeOSReleaseNotesVersion,
                     version_info.Version());
    return;
  }

  std::string prev_version_pref =
      prefs->GetString(prefs::kChromeOSReleaseNotesVersion);
  Version prev_version(prev_version_pref);
  if (!prev_version.IsValid())
    prev_version = Version("0.0.0.0");
  Version current_version(version_info.Version());

  if (!current_version.components().size()) {
    NOTREACHED() << "Incorrect version " << current_version.GetString();
    return;
  }

  // No "Release Notes" content yet for upgrade from M19 to later release.
  if (prev_version.components()[0] >= kReleaseNotesTargetRelease)
    return;

  // Otherwise, trigger on major version change.
  if (current_version.components()[0] > prev_version.components()[0]) {
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kApp, kGetStartedWebUrl);
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppWindowSize, kGSGAppWindowSize);
      prefs->SetString(prefs::kChromeOSReleaseNotesVersion,
                       current_version.GetString());
  }
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  // TODO(dpolukhin): show detailed error info. |details| string contains
  // low level error info that is not localized and even is not user friendly.
  // For now just ignore it because error_text contains all required information
  // for end users, developers can see details string in Chrome logs.
  VLOG(1) << details;
  HelpAppLauncher::HelpTopic help_topic_id;
  bool is_offline = !ConnectivityStateHelper::Get()->IsConnected();
  switch (login_performer_->error().state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
      help_topic_id = HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE;
      break;
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      help_topic_id = HelpAppLauncher::HELP_ACCOUNT_DISABLED;
      break;
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
      help_topic_id = HelpAppLauncher::HELP_HOSTED_ACCOUNT;
      break;
    default:
      help_topic_id = is_offline ?
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE :
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT;
      break;
  }

  login_display_->ShowError(error_id, num_login_attempts_, help_topic_id);
}

void ExistingUserController::ShowGaiaPasswordChanged(
    const std::string& username) {
  // Invalidate OAuth token, since it can't be correct after password is
  // changed.
  UserManager::Get()->SaveUserOAuthStatus(
      username,
      User::OAUTH2_TOKEN_STATUS_INVALID);

  login_display_->SetUIEnabled(true);
  login_display_->ShowGaiaPasswordChanged(username);
}

}  // namespace chromeos
