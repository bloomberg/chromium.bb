// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_performer.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/managed/locally_managed_user_login_flow.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/cros_settings_names.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/generated_resources.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::UserMetricsAction;

namespace chromeos {

// Initialize default LoginPerformer.
// static
LoginPerformer* LoginPerformer::default_performer_ = NULL;

LoginPerformer::LoginPerformer(Delegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(online_attempt_host_(this)),
      last_login_failure_(LoginFailure::LoginFailureNone()),
      delegate_(delegate),
      password_changed_(false),
      password_changed_callback_count_(0),
      screen_lock_requested_(false),
      initial_online_auth_pending_(false),
      auth_mode_(AUTH_MODE_INTERNAL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(default_performer_ == NULL)
      << "LoginPerformer should have only one instance.";
  default_performer_ = this;
}

LoginPerformer::~LoginPerformer() {
  DVLOG(1) << "Deleting LoginPerformer";
  DCHECK(default_performer_ != NULL) << "Default instance should exist.";
  default_performer_ = NULL;
  if (authenticator_)
    authenticator_->SetConsumer(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, LoginStatusConsumer implementation:

void LoginPerformer::OnLoginFailure(const LoginFailure& failure) {
  content::RecordAction(UserMetricsAction("Login_Failure"));
  UMA_HISTOGRAM_ENUMERATION("Login.FailureReason", failure.reason(),
                            LoginFailure::NUM_FAILURE_REASONS);

  DVLOG(1) << "failure.reason " << failure.reason();
  DVLOG(1) << "failure.error.state " << failure.error().state();

  last_login_failure_ = failure;
  if (delegate_) {
    delegate_->OnLoginFailure(failure);
    return;
  }

  // Consequent online login failure with blocking UI on.
  // No difference between cases whether screen was locked by the user or
  // by LoginPerformer except for the very first screen lock while waiting
  // for online auth. Otherwise it will be SL active > timeout > screen unlock.
  // Display recoverable error message using ScreenLocker,
  // force sign out otherwise.
  if (ScreenLocker::default_screen_locker() && !initial_online_auth_pending_) {
    ResolveLockLoginFailure();
    return;
  }
  initial_online_auth_pending_ = false;

  // Offline auth - OK, online auth - failed.
  if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED) {
    ResolveInitialNetworkAuthFailure();
  } else if (failure.reason() == LoginFailure::LOGIN_TIMED_OUT) {
    VLOG(1) << "Online login timed out. "
            << "Granting user access based on offline auth only.";
    // ScreenLock is not active, it's ok to delete itself.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  } else {
    // COULD_NOT_MOUNT_CRYPTOHOME, COULD_NOT_MOUNT_TMPFS:
    // happens during offline auth only.
    // UNLOCK_FAILED is used during normal screen lock case.
    // TODO(nkostylev) DATA_REMOVAL_FAILED - ?
    NOTREACHED();
  }
}

void LoginPerformer::OnRetailModeLoginSuccess() {
  content::RecordAction(
      UserMetricsAction("Login_DemoUserLoginSuccess"));

  LoginStatusConsumer::OnRetailModeLoginSuccess();
}

void LoginPerformer::OnLoginSuccess(
    const UserCredentials& credentials,
    bool pending_requests,
    bool using_oauth) {
  content::RecordAction(UserMetricsAction("Login_Success"));
  // The value of |pending_requests| indicates:
  // 0 - New regular user, login success offline and online.
  //     - or -
  //     Existing regular user, login success offline and online, offline
  //     authentication took longer than online authentication.
  //     - or -
  //     Public account user, login successful.
  // 1 - Existing regular user, login success offline only.
  UMA_HISTOGRAM_ENUMERATION("Login.SuccessReason", pending_requests, 2);

  VLOG(1) << "LoginSuccess, pending_requests " << pending_requests;
  DCHECK(delegate_);
  // After delegate_->OnLoginSuccess(...) is called, delegate_ releases
  // LoginPerformer ownership. LP now manages it's lifetime on its own.
  // 2 things could make it exist longer:
  // 1. ScreenLock active (pending correct new password input)
  // 2. Pending online auth request.
  if (!pending_requests)
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  else
    initial_online_auth_pending_ = true;

  delegate_->OnLoginSuccess(credentials,
                            pending_requests,
                            using_oauth);
}

void LoginPerformer::OnOffTheRecordLoginSuccess() {
  content::RecordAction(
      UserMetricsAction("Login_GuestLoginSuccess"));

  if (delegate_)
    delegate_->OnOffTheRecordLoginSuccess();
  else
    NOTREACHED();
}

void LoginPerformer::OnPasswordChangeDetected() {
  password_changed_ = true;
  password_changed_callback_count_++;
  if (delegate_) {
    delegate_->OnPasswordChangeDetected();
  } else {
    last_login_failure_ =
        LoginFailure::FromNetworkAuthFailure(GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    DVLOG(1) << "Password change detected - locking screen.";
    RequestScreenLock();
  }
}

void LoginPerformer::OnChecked(const std::string& username, bool success) {
  if (!delegate_) {
    // Delegate is reset in case of successful offline login.
    // See ExistingUserConstoller::OnLoginSuccess().
    // Case when user has changed password and enters old password
    // does not block user from sign in yet.
    return;
  }
  delegate_->OnOnlineChecked(username, success);
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, content::NotificationObserver implementation:
//

void LoginPerformer::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type != chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED)
    return;

  bool is_screen_locked = *content::Details<bool>(details).ptr();
  if (is_screen_locked) {
    if (screen_lock_requested_) {
      screen_lock_requested_ = false;
      ResolveScreenLocked();
    }
  } else {
    ResolveScreenUnlocked();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, public:

void LoginPerformer::PerformLogin(const UserCredentials& credentials,
                                  AuthorizationMode auth_mode) {
  auth_mode_ = auth_mode;
  credentials_ = credentials;

  CrosSettings* cros_settings = CrosSettings::Get();

  // Whitelist check is always performed during initial login and
  // should not be performed when ScreenLock is active (pending online auth).
  if (!ScreenLocker::default_screen_locker()) {
    CrosSettingsProvider::TrustedStatus status =
        cros_settings->PrepareTrustedValues(
            base::Bind(&LoginPerformer::PerformLogin,
                       weak_factory_.GetWeakPtr(),
                       credentials_, auth_mode));
    // Must not proceed without signature verification.
    if (status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
      if (delegate_)
        delegate_->PolicyLoadFailed();
      else
        NOTREACHED();
      return;
    } else if (status != CrosSettingsProvider::TRUSTED) {
      // Value of AllowNewUser setting is still not verified.
      // Another attempt will be invoked after verification completion.
      return;
    }
  }

  bool is_whitelisted = LoginUtils::IsWhitelisted(
      gaia::CanonicalizeEmail(credentials.username));
  if (ScreenLocker::default_screen_locker() || is_whitelisted) {
    switch (auth_mode_) {
      case AUTH_MODE_EXTENSION:
        StartLoginCompletion();
        break;
      case AUTH_MODE_INTERNAL:
        StartAuthentication();
        break;
    }
  } else {
    if (delegate_)
      delegate_->WhiteListCheckFailed(credentials.username);
    else
      NOTREACHED();
  }
}

void LoginPerformer::CreateLocallyManagedUser(const string16& display_name,
                                              const std::string& password) {
  std::string id = UserManager::Get()->GenerateUniqueLocallyManagedUserId();
  const User* user = UserManager::Get()->
      CreateLocallyManagedUserRecord(id, display_name);
  LoginAsLocallyManagedUser(UserCredentials(user->email(),
                                            password,
                                            std::string()));  // auth_code
}

void LoginPerformer::LoginAsLocallyManagedUser(
    const UserCredentials& credentials) {
  DCHECK_EQ(UserManager::kLocallyManagedUserDomain,
            gaia::ExtractDomainName(credentials.username));
  // TODO(nkostylev): Check that policy allows locally managed user login.

  UserManager::Get()->SetUserFlow(credentials.username,
                                  new LocallyManagedUserLoginFlow(
                                      credentials.username));
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginAsLocallyManagedUser,
                 authenticator_.get(),
                 credentials));
}

void LoginPerformer::LoginRetailMode() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginRetailMode, authenticator_.get()));
}

void LoginPerformer::LoginOffTheRecord() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginOffTheRecord, authenticator_.get()));
}

void LoginPerformer::LoginAsPublicAccount(const std::string& username) {
  // Login is not allowed if policy could not be loaded for the account.
  policy::DeviceLocalAccountPolicyService* policy_service =
      g_browser_process->browser_policy_connector()->
          GetDeviceLocalAccountPolicyService();
  if (!policy_service ||
      !policy_service->IsPolicyAvailableForAccount(username)) {
    DCHECK(delegate_);
    if (delegate_)
      delegate_->PolicyLoadFailed();
    return;
  }

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::LoginAsPublicAccount, authenticator_.get(),
                 username));
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::RecoverEncryptedData, authenticator_.get(),
                 old_password));
}

void LoginPerformer::ResyncEncryptedData() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::ResyncEncryptedData, authenticator_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::RequestScreenLock() {
  DVLOG(1) << "Screen lock requested";
  // Will receive notifications on screen unlock and delete itself.
  registrar_.Add(this,
                 chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                 content::NotificationService::AllSources());
  if (ScreenLocker::default_screen_locker()) {
    DVLOG(1) << "Screen already locked";
    ResolveScreenLocked();
  } else {
    screen_lock_requested_ = true;
    // TODO(antrim) : additional logging for crbug/173178
    LOG(WARNING) << "Requesting screen lock from LoginPerformer";
    DBusThreadManager::Get()->GetSessionManagerClient()->RequestLockScreen();
  }
}

void LoginPerformer::RequestScreenUnlock() {
  DVLOG(1) << "Screen unlock requested";
  if (ScreenLocker::default_screen_locker()) {
    DBusThreadManager::Get()->GetSessionManagerClient()->RequestUnlockScreen();
    // Will unsubscribe from notifications once unlock is successful.
  } else {
    LOG(ERROR) << "Screen is not locked";
    NOTREACHED();
  }
}

void LoginPerformer::ResolveInitialNetworkAuthFailure() {
  DVLOG(1) << "auth_error: " << last_login_failure_.error().state();

  switch (last_login_failure_.error().state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::REQUEST_CANCELED:
      // Offline auth already done. Online auth will be done next time
      // or once user accesses web property.
      VLOG(1) << "Granting user access based on offline auth only. "
              << "Online login failed with "
              << last_login_failure_.error().state();
      // Resolving initial online auth failure, no ScreenLock is active.
      MessageLoop::current()->DeleteSoon(FROM_HERE, this);
      return;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      // Offline auth OK, so it might be the case of changed password.
      password_changed_ = true;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      // Access not granted. User has to sign out.
      // Request screen lock & show error message there.
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    default:
      // Unless there's new GoogleServiceAuthErrors state has been added.
      NOTREACHED();
      return;
  }
}

void LoginPerformer::ResolveLockLoginFailure() {
  if (last_login_failure_.reason() == LoginFailure::LOGIN_TIMED_OUT) {
    LOG(WARNING) << "Online login timed out - unlocking screen. "
                 << "Granting user access based on offline auth only.";
    RequestScreenUnlock();
    return;
  } else if (last_login_failure_.reason() ==
             LoginFailure::NETWORK_AUTH_FAILED) {
    ResolveLockNetworkAuthFailure();
    return;
  } else if (last_login_failure_.reason() ==
             LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME ||
             last_login_failure_.reason() ==
             LoginFailure::DATA_REMOVAL_FAILED) {
    LOG(ERROR) << "Cryptohome error, forcing sign out.";
  } else {
    // COULD_NOT_MOUNT_TMPFS, UNLOCK_FAILED should not happen here.
    NOTREACHED();
  }
  ScreenLocker::default_screen_locker()->Signout();
}

void LoginPerformer::ResolveLockNetworkAuthFailure() {
  DCHECK(ScreenLocker::default_screen_locker())
      << "ScreenLocker instance doesn't exist.";
  DCHECK(last_login_failure_.reason() == LoginFailure::NETWORK_AUTH_FAILED);

  int msg_id = IDS_LOGIN_ERROR_AUTHENTICATING;
  HelpAppLauncher::HelpTopic help_topic =
      HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT;
  bool sign_out_only = false;

  DVLOG(1) << "auth_error: " << last_login_failure_.error().state();

  switch (last_login_failure_.error().state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::REQUEST_CANCELED:
      // Offline auth already done. Online auth will be done next time
      // or once user accesses web property.
      LOG(WARNING) << "Granting user access based on offline auth only. "
                   << "Online login failed with "
                   << last_login_failure_.error().state();
      RequestScreenUnlock();
      return;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      // Password change detected.
      msg_id = IDS_LOGIN_ERROR_PASSWORD_CHANGED;
      break;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      // Access not granted. User has to sign out.
      // Show error message using existing screen lock.
      msg_id = IDS_LOGIN_ERROR_RESTRICTED;
      help_topic = HelpAppLauncher::HELP_ACCOUNT_DISABLED;
      sign_out_only = true;
      break;
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    default:
      // Unless there's new GoogleServiceAuthError state has been added.
      NOTREACHED();
      break;
  }

  ScreenLocker::default_screen_locker()->ShowErrorMessage(msg_id,
                                                          help_topic,
                                                          sign_out_only);
}

void LoginPerformer::ResolveScreenLocked() {
  DVLOG(1) << "Screen locked";
  ResolveLockNetworkAuthFailure();
}

void LoginPerformer::ResolveScreenUnlocked() {
  DVLOG(1) << "Screen unlocked";
  registrar_.RemoveAll();
  // If screen was unlocked that was for a reason, should delete itself now.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void LoginPerformer::StartLoginCompletion() {
  DVLOG(1) << "Login completion started";
  BootTimesLoader::Get()->AddLoginTimeMarker("AuthStarted", false);
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Authenticator::CompleteLogin, authenticator_.get(),
                 profile,
                 credentials_));

  credentials_.password.clear();
  credentials_.auth_code.clear();
}

void LoginPerformer::StartAuthentication() {
  DVLOG(1) << "Auth started";
  BootTimesLoader::Get()->AddLoginTimeMarker("AuthStarted", false);
  Profile* profile;
  {
    // This should be the first place where GetDefaultProfile() is called with
    // logged_in_ = true. This will trigger a call to Profile::CreateProfile()
    // which requires IO access.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    profile = g_browser_process->profile_manager()->GetDefaultProfile();
  }
  if (delegate_) {
    authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Authenticator::AuthenticateToLogin, authenticator_.get(),
                   profile,
                   credentials_,
                   std::string(),
                   std::string()));
    // Make unobtrusive online check. It helps to determine password change
    // state in the case when offline login fails.
    online_attempt_host_.Check(profile, credentials_);
  } else {
    DCHECK(authenticator_.get())
        << "Authenticator instance doesn't exist for login attempt retry.";
    // At this point offline auth has been successful,
    // retry online auth, using existing Authenticator instance.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&Authenticator::RetryAuth, authenticator_.get(),
                   profile,
                   credentials_,
                   std::string(),
                   std::string()));
  }
  credentials_.password.clear();
  credentials_.auth_code.clear();
}

}  // namespace chromeos
