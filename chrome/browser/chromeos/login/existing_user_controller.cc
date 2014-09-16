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
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/accessibility/accessibility_events.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/login/user_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/google/core/browser/google_util.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
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
    : auth_status_consumer_(NULL),
      host_(host),
      login_display_(host_->CreateLoginDisplay(this)),
      num_login_attempts_(0),
      cros_settings_(CrosSettings::Get()),
      offline_failed_(false),
      is_login_in_progress_(false),
      password_changed_(false),
      auth_mode_(LoginPerformer::AUTH_MODE_EXTENSION),
      do_auto_enrollment_(false),
      signin_screen_ready_(false),
      network_state_helper_(new login::NetworkStateHelper),
      weak_factory_(this) {
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
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end();
       ++it) {
    // TODO(xiyuan): Clean user profile whose email is not in whitelist.
    bool meets_supervised_requirements =
        (*it)->GetType() != user_manager::USER_TYPE_SUPERVISED ||
        user_manager::UserManager::Get()->AreSupervisedUsersAllowed();
    bool meets_whitelist_requirements =
        LoginUtils::IsWhitelisted((*it)->email(), NULL) ||
        (*it)->GetType() != user_manager::USER_TYPE_REGULAR;

    // Public session accounts are always shown on login screen.
    bool meets_show_users_requirements =
        show_users_on_signin ||
        (*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
    if (meets_supervised_requirements &&
        meets_whitelist_requirements &&
        meets_show_users_requirements) {
      filtered_users.push_back(*it);
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

void ExistingUserController::DoAutoEnrollment() {
  do_auto_enrollment_ = true;
}

void ExistingUserController::ResumeLogin() {
  // This means the user signed-in, then auto-enrollment used his credentials
  // to enroll and succeeded.
  resume_login_callback_.Run();
  resume_login_callback_.Reset();
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
                   signin_profile_context_getter,
                   browser_process_context_getter),
        base::TimeDelta::FromMilliseconds(kAuthCacheTransferDelayMs));
  }
  if (type != chrome::NOTIFICATION_LOGIN_USER_IMAGE_CHANGED)
    return;
  login_display_->OnUserImageChanged(
      *content::Details<user_manager::User>(details).ptr());
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, private:

ExistingUserController::~ExistingUserController() {
  LoginUtils::Get()->DelegateDeleted(this);

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
  content::RecordAction(base::UserMetricsAction("Login.CreateAccount"));
  guest_mode_url_ = google_util::AppendGoogleLocaleParam(
      GURL(kCreateAccountURL), g_browser_process->GetApplicationLocale());
  LoginAsGuest();
}

void ExistingUserController::CompleteLogin(const UserContext& user_context) {
  login_display_->set_signin_completed(true);
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
    DeviceSettingsService::OwnershipStatus ownership_status) {
  // Auto-enrollment must have made a decision by now. It's too late to enroll
  // if the protocol isn't done at this point.
  if (do_auto_enrollment_ &&
      ownership_status == DeviceSettingsService::OWNERSHIP_NONE) {
    VLOG(1) << "Forcing auto-enrollment before completing login";
    // The only way to get out of the enrollment screen from now on is to either
    // complete enrollment, or opt-out of it. So this controller shouldn't force
    // enrollment again if it is reused for another sign-in.
    do_auto_enrollment_ = false;
    auto_enrollment_username_ = user_context.GetUserID();
    resume_login_callback_ = base::Bind(
        &ExistingUserController::PerformLogin,
        weak_factory_.GetWeakPtr(),
        user_context, LoginPerformer::AUTH_MODE_EXTENSION);
    ShowEnrollmentScreen(true, user_context.GetUserID());
    // Enable UI for the enrollment screen. SetUIEnabled(true) will post a
    // request to show the sign-in screen again when invoked at the sign-in
    // screen; invoke SetUIEnabled() after navigating to the enrollment screen.
    login_display_->SetUIEnabled(true);
  } else {
    PerformLogin(user_context, LoginPerformer::AUTH_MODE_EXTENSION);
  }
}

base::string16 ExistingUserController::GetConnectedNetworkName() {
  return network_state_helper_->GetCurrentNetworkName();
}

bool ExistingUserController::IsSigninInProgress() const {
  return is_login_in_progress_;
}

void ExistingUserController::Login(const UserContext& user_context,
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
  } else if (user_context.GetUserType() ==
             user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    LoginAsPublicSession(user_context);
    return;
  } else if (user_context.GetUserType() ==
             user_manager::USER_TYPE_RETAIL_MODE) {
    LoginAsRetailModeUser();
    return;
  } else if (user_context.GetUserType() == user_manager::USER_TYPE_KIOSK_APP) {
    LoginAsKioskApp(user_context.GetUserID(), specifics.kiosk_diagnostic_mode);
    return;
  }

  if (!user_context.HasCredentials())
    return;

  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();

  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  if (last_login_attempt_username_ != user_context.GetUserID()) {
    last_login_attempt_username_ = user_context.GetUserID();
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
  ChromeUserManager::Get()->GetUserFlow(last_login_attempt_username_)->set_host(
      host_);

  BootTimesLoader::Get()->RecordLoginAttempted();

  // Disable UI while loading user profile.
  login_display_->SetUIEnabled(false);

  // Use the same LoginPerformer for subsequent login as it has state
  // such as Authenticator instance.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    // Only one instance of LoginPerformer should exist at a time.
    login_performer_.reset(NULL);
    login_performer_.reset(new LoginPerformer(this));
  }

  is_login_in_progress_ = true;
  if (gaia::ExtractDomainName(user_context.GetUserID()) ==
      chromeos::login::kSupervisedUserDomain) {
    login_performer_->LoginAsSupervisedUser(user_context);
  } else {
    login_performer_->PerformLogin(user_context, auth_mode);
  }
  SendAccessibilityAlert(
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
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_DEMOUSER));
}

void ExistingUserController::LoginAsGuest() {
  if (is_login_in_progress_ ||
      user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return;
  }

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
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_OFFRECORD));
}

void ExistingUserController::MigrateUserData(const std::string& old_password) {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get())
    login_performer_->RecoverEncryptedData(old_password);
}

void ExistingUserController::LoginAsPublicSession(
    const UserContext& user_context) {
  if (is_login_in_progress_ ||
      user_manager::UserManager::Get()->IsUserLoggedIn()) {
    return;
  }

  // Stop the auto-login timer when attempting login.
  StopPublicSessionAutoLoginTimer();

  // Disable clicking on other windows.
  login_display_->SetUIEnabled(false);

  CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&ExistingUserController::LoginAsPublicSession,
                     weak_factory_.GetWeakPtr(),
                     user_context));
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

  // If there is no public account with the given user ID, logging in is not
  // possible.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetUserID());
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    // Re-enable clicking on other windows.
    login_display_->SetUIEnabled(true);
    StartPublicSessionAutoLoginTimer();
    return;
  }

  UserContext new_user_context = user_context;
  std::string locale = user_context.GetPublicSessionLocale();
  if (locale.empty()) {
    // When performing auto-login, no locale is chosen by the user. Check
    // whether a list of recommended locales was set by policy. If so, use its
    // first entry. Otherwise, |locale| will remain blank, indicating that the
    // public session should use the current UI locale.
    const policy::PolicyMap::Entry* entry = g_browser_process->platform_part()->
        browser_policy_connector_chromeos()->
            GetDeviceLocalAccountPolicyService()->
                GetBrokerForUser(user_context.GetUserID())->core()->store()->
                    policy_map().Get(policy::key::kSessionLocales);
    base::ListValue const* list = NULL;
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
  host_->StartAppLaunch(app_id, diagnostic_mode);
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
  if (login_performer_.get())
    login_performer_->ResyncEncryptedData();
}

void ExistingUserController::SetDisplayEmail(const std::string& email) {
  display_email_ = email;
}

void ExistingUserController::ShowWrongHWIDScreen() {
  scoped_ptr<base::DictionaryValue> params;
  host_->StartWizard(WizardController::kWrongHWIDScreenName, params.Pass());
}

void ExistingUserController::Signout() {
  NOTREACHED();
}

void ExistingUserController::OnConsumerKioskAutoLaunchCheckCompleted(
    KioskAppManager::ConsumerKioskAutoLaunchStatus status) {
  if (status == KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE)
    ShowKioskEnableScreen();
}

void ExistingUserController::OnEnrollmentOwnershipCheckCompleted(
    DeviceSettingsService::OwnershipStatus status) {
  if (status == DeviceSettingsService::OWNERSHIP_NONE) {
    ShowEnrollmentScreen(false, std::string());
  } else if (status == DeviceSettingsService::OWNERSHIP_TAKEN) {
    // On a device that is already owned we might want to allow users to
    // re-enroll if the policy information is invalid.
    CrosSettingsProvider::TrustedStatus trusted_status =
        CrosSettings::Get()->PrepareTrustedValues(
            base::Bind(
                &ExistingUserController::OnEnrollmentOwnershipCheckCompleted,
                weak_factory_.GetWeakPtr(), status));
    if (trusted_status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
      ShowEnrollmentScreen(false, std::string());
    }
  } else {
    // OwnershipService::GetStatusAsync is supposed to return either
    // OWNERSHIP_NONE or OWNERSHIP_TAKEN.
    NOTREACHED();
  }
}

void ExistingUserController::ShowEnrollmentScreen(bool is_auto_enrollment,
                                                  const std::string& user) {
  scoped_ptr<base::DictionaryValue> params;
  if (is_auto_enrollment) {
    params.reset(new base::DictionaryValue());
    params->SetBoolean("is_auto_enrollment", true);
    params->SetString("user", user);
  }
  host_->StartWizard(WizardController::kEnrollmentScreenName,
                     params.Pass());
}

void ExistingUserController::ShowResetScreen() {
  scoped_ptr<base::DictionaryValue> params;
  host_->StartWizard(WizardController::kResetScreenName, params.Pass());
}

void ExistingUserController::ShowKioskEnableScreen() {
  scoped_ptr<base::DictionaryValue> params;
  host_->StartWizard(WizardController::kKioskEnableScreenName, params.Pass());
}

void ExistingUserController::ShowKioskAutolaunchScreen() {
  scoped_ptr<base::DictionaryValue> params;
  host_->StartWizard(WizardController::kKioskAutolaunchScreenName,
                     params.Pass());
}

void ExistingUserController::ShowTPMError() {
  login_display_->SetUIEnabled(false);
  login_display_->ShowErrorScreen(LoginDisplay::TPM_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
// ExistingUserController, LoginPerformer::Delegate implementation:
//

void ExistingUserController::OnAuthFailure(const AuthFailure& failure) {
  is_login_in_progress_ = false;
  offline_failed_ = true;

  guest_mode_url_ = GURL::EmptyGURL();
  std::string error = failure.GetErrorString();

  if (ChromeUserManager::Get()
          ->GetUserFlow(last_login_attempt_username_)
          ->HandleLoginFailure(failure)) {
    login_display_->SetUIEnabled(true);
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
  } else if (!online_succeeded_for_.empty()) {
    ShowGaiaPasswordChanged(online_succeeded_for_);
  } else {
    // Check networking after trying to login in case user is
    // cached locally or the local admin account.
    bool is_known_user = user_manager::UserManager::Get()->IsKnownUser(
        last_login_attempt_username_);
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
    // Reenable clicking on other windows and status area.
    login_display_->SetUIEnabled(true);
    login_display_->ClearAndEnablePassword();
    StartPublicSessionAutoLoginTimer();
  }

  // Reset user flow to default, so that special flow will not affect next
  // attempt.
  ChromeUserManager::Get()->ResetUserFlow(last_login_attempt_username_);

  if (auth_status_consumer_)
    auth_status_consumer_->OnAuthFailure(failure);

  // Clear the recorded displayed email so it won't affect any future attempts.
  display_email_.clear();
}

void ExistingUserController::OnAuthSuccess(const UserContext& user_context) {
  is_login_in_progress_ = false;
  offline_failed_ = false;
  login_display_->set_signin_completed(true);

  // Login performer will be gone so cache this value to use
  // once profile is loaded.
  password_changed_ = login_performer_->password_changed();
  auth_mode_ = login_performer_->auth_mode();

  ChromeUserManager::Get()
      ->GetUserFlow(user_context.GetUserID())
      ->HandleLoginSuccess(user_context);

  StopPublicSessionAutoLoginTimer();

  const bool has_auth_cookies =
      login_performer_->auth_mode() == LoginPerformer::AUTH_MODE_EXTENSION &&
      user_context.GetAuthCode().empty();

  // LoginPerformer instance will delete itself once online auth result is OK.
  // In case of failure it'll bring up ScreenLock and ask for
  // correct password/display error message.
  // Even in case when following online,offline protocol and returning
  // requests_pending = false, let LoginPerformer delete itself.
  login_performer_->set_delegate(NULL);
  ignore_result(login_performer_.release());

  // Will call OnProfilePrepared() in the end.
  LoginUtils::Get()->PrepareProfile(user_context,
                                    has_auth_cookies,
                                    false,          // Start session for user.
                                    this);

  // Update user's displayed email.
  if (!display_email_.empty()) {
    user_manager::UserManager::Get()->SaveUserDisplayEmail(
        user_context.GetUserID(), display_email_);
    display_email_.clear();
  }
}

void ExistingUserController::OnProfilePrepared(Profile* profile) {
  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager->IsCurrentUserNew() &&
      user_manager->IsLoggedInAsSupervisedUser()) {
    // Supervised users should launch into empty desktop on first run.
    CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kSilentLaunch);
  }

  if (user_manager->IsCurrentUserNew() &&
      !ChromeUserManager::Get()
           ->GetCurrentUserFlow()
           ->ShouldSkipPostLoginScreens() &&
      !WizardController::default_controller()->skip_post_login_screens()) {
    // Don't specify start URLs if the administrator has configured the start
    // URLs via policy.
    if (!SessionStartupPref::TypeIsManaged(profile->GetPrefs()))
      InitializeStartUrls();

    // Mark the device as registered., i.e. the second part of OOBE as
    // completed.
    if (!StartupUtils::IsDeviceRegistered())
      StartupUtils::MarkDeviceRegistered(base::Closure());

    if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeSkipPostLogin)) {
      LoginUtils::Get()->DoBrowserLaunch(profile, host_);
      host_ = NULL;
    } else {
      ActivateWizard(WizardController::kTermsOfServiceScreenName);
    }
  } else {
    LoginUtils::Get()->DoBrowserLaunch(profile, host_);
    host_ = NULL;
  }
  // Inform |auth_status_consumer_| about successful login.
  if (auth_status_consumer_)
    auth_status_consumer_->OnAuthSuccess(UserContext());
}

void ExistingUserController::OnOffTheRecordAuthSuccess() {
  is_login_in_progress_ = false;
  offline_failed_ = false;

  // Mark the device as registered., i.e. the second part of OOBE as completed.
  if (!StartupUtils::IsDeviceRegistered())
    StartupUtils::MarkDeviceRegistered(base::Closure());

  LoginUtils::Get()->CompleteOffTheRecordLogin(guest_mode_url_);

  if (auth_status_consumer_)
    auth_status_consumer_->OnOffTheRecordAuthSuccess();
}

void ExistingUserController::OnPasswordChangeDetected() {
  is_login_in_progress_ = false;
  offline_failed_ = false;

  // Must not proceed without signature verification.
  if (CrosSettingsProvider::TRUSTED != cros_settings_->PrepareTrustedValues(
      base::Bind(&ExistingUserController::OnPasswordChangeDetected,
                 weak_factory_.GetWeakPtr()))) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }

  if (ChromeUserManager::Get()
          ->GetUserFlow(last_login_attempt_username_)
          ->HandlePasswordChangeDetected()) {
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

  if (auth_status_consumer_)
    auth_status_consumer_->OnPasswordChangeDetected();

  display_email_.clear();
}

void ExistingUserController::WhiteListCheckFailed(const std::string& email) {
  is_login_in_progress_ = false;
  offline_failed_ = false;

  ShowError(IDS_LOGIN_ERROR_WHITELIST, email);

  // Reenable clicking on other windows and status area.
  login_display_->SetUIEnabled(true);
  login_display_->ShowSigninUI(email);

  if (auth_status_consumer_) {
    auth_status_consumer_->OnAuthFailure(
        AuthFailure(AuthFailure::WHITELIST_CHECK_FAILED));
  }

  display_email_.clear();

  StartPublicSessionAutoLoginTimer();
}

void ExistingUserController::PolicyLoadFailed() {
  ShowError(IDS_LOGIN_ERROR_OWNER_KEY_LOST, "");

  // Reenable clicking on other windows and status area.
  is_login_in_progress_ = false;
  offline_failed_ = false;
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

void ExistingUserController::DeviceSettingsChanged() {
  if (host_ != NULL) {
    // Signed settings or user list changed. Notify views and update them.
    UpdateLoginDisplay(user_manager::UserManager::Get()->GetUsers());
    ConfigurePublicSessionAutoLogin();
    return;
  }
}

void ExistingUserController::ActivateWizard(const std::string& screen_name) {
  scoped_ptr<base::DictionaryValue> params;
  host_->StartWizard(screen_name, params.Pass());
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

void ExistingUserController::ConfigurePublicSessionAutoLogin() {
  std::string auto_login_account_id;
  cros_settings_->GetString(kAccountsPrefDeviceLocalAccountAutoLoginId,
                            &auto_login_account_id);
  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(cros_settings_);

  public_session_auto_login_username_.clear();
  for (std::vector<policy::DeviceLocalAccount>::const_iterator
           it = device_local_accounts.begin();
       it != device_local_accounts.end(); ++it) {
    if (it->account_id == auto_login_account_id) {
      public_session_auto_login_username_ = it->user_id;
      break;
    }
  }

  const user_manager::User* user = user_manager::UserManager::Get()->FindUser(
      public_session_auto_login_username_);
  if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    public_session_auto_login_username_.clear();

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
  // TODO(bartfab): Set the UI language and initial locale.
  LoginAsPublicSession(UserContext(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                   public_session_auto_login_username_));
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

  const base::ListValue *urls;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  bool can_show_getstarted_guide =
      user_manager->GetActiveUser()->GetType() ==
          user_manager::USER_TYPE_REGULAR &&
      !user_manager->IsCurrentUserNonCryptohomeDataEphemeral();
  if (user_manager->IsLoggedInAsDemoUser()) {
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
    can_show_getstarted_guide = false;
  // Skip the default first-run behavior for public accounts.
  } else if (!user_manager->IsLoggedInAsPublicAccount()) {
    if (AccessibilityManager::Get()->IsSpokenFeedbackEnabled()) {
      const char* url = kChromeVoxTutorialURLPattern;
      PrefService* prefs = g_browser_process->local_state();
      const std::string current_locale =
          base::StringToLowerASCII(prefs->GetString(prefs::kApplicationLocale));
      std::string vox_url = base::StringPrintf(url, current_locale.c_str());
      start_urls.push_back(vox_url);
      can_show_getstarted_guide = false;
    }
  }

  // Only show getting started guide for a new user.
  const bool should_show_getstarted_guide = user_manager->IsCurrentUserNew();

  if (can_show_getstarted_guide && should_show_getstarted_guide) {
    // Don't open default Chrome window if we're going to launch the first-run
    // app. Because we dont' want the first-run app to be hidden in the
    // background.
    CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kSilentLaunch);
    first_run::MaybeLaunchDialogAfterSessionStart();
  } else {
    for (size_t i = 0; i < start_urls.size(); ++i) {
      CommandLine::ForCurrentProcess()->AppendArg(start_urls[i]);
    }
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
  bool is_offline = !network_state_helper_->IsConnected();
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

  if (error_id == IDS_LOGIN_ERROR_AUTHENTICATING) {
    if (num_login_attempts_ > 1) {
      const user_manager::User* user =
          user_manager::UserManager::Get()->FindUser(
              last_login_attempt_username_);
      if (user && (user->GetType() == user_manager::USER_TYPE_SUPERVISED))
        error_id = IDS_LOGIN_ERROR_AUTHENTICATING_2ND_TIME_SUPERVISED;
    }
  }

  login_display_->ShowError(error_id, num_login_attempts_, help_topic_id);
}

void ExistingUserController::ShowGaiaPasswordChanged(
    const std::string& username) {
  // Invalidate OAuth token, since it can't be correct after password is
  // changed.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      username, user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);

  login_display_->SetUIEnabled(true);
  login_display_->ShowGaiaPasswordChanged(username);
}

void ExistingUserController::SendAccessibilityAlert(
    const std::string& alert_text) {
  AccessibilityAlertInfo event(ProfileHelper::GetSigninProfile(), alert_text);
  SendControlAccessibilityNotification(
      ui::AX_EVENT_VALUE_CHANGED, &event);
}

void ExistingUserController::SetPublicSessionKeyboardLayoutAndLogin(
    const UserContext& user_context,
    scoped_ptr<base::ListValue> keyboard_layouts) {
  UserContext new_user_context = user_context;
  std::string keyboard_layout;
  for (size_t i = 0; i < keyboard_layouts->GetSize(); ++i) {
    base::DictionaryValue* entry = NULL;
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
  login_performer_.reset(NULL);
  login_performer_.reset(new LoginPerformer(this));
  is_login_in_progress_ = true;
  login_performer_->LoginAsPublicSession(user_context);
  SendAccessibilityAlert(
      l10n_util::GetStringUTF8(IDS_CHROMEOS_ACC_LOGIN_SIGNIN_PUBLIC_ACCOUNT));
}

}  // namespace chromeos
