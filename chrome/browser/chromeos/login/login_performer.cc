// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_performer.h"

#include <string>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

// Initialize default LoginPerformer.
// static
LoginPerformer* LoginPerformer::default_performer_ = NULL;

LoginPerformer::LoginPerformer(Delegate* delegate)
    : last_login_failure_(LoginFailure::None()),
      delegate_(delegate),
      password_changed_(false),
      screen_lock_requested_(false),
      initial_online_auth_pending_(false),
      method_factory_(this) {
  DCHECK(default_performer_ == NULL)
      << "LoginPerformer should have only one instance.";
  default_performer_ = this;
}

LoginPerformer::~LoginPerformer() {
  DVLOG(1) << "Deleting LoginPerformer";
  DCHECK(default_performer_ != NULL) << "Default instance should exist.";
  default_performer_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, LoginStatusConsumer implementation:

void LoginPerformer::OnLoginFailure(const LoginFailure& failure) {
  UserMetrics::RecordAction(UserMetricsAction("Login_Failure"));
  UMA_HISTOGRAM_ENUMERATION("Login.FailureReason", failure.reason(),
                            LoginFailure::NUM_FAILURE_REASONS);

  DVLOG(1) << "failure.reason " << failure.reason();
  DVLOG(1) << "failure.error.state " << failure.error().state();

  last_login_failure_ = failure;
  if (delegate_) {
    captcha_.clear();
    captcha_token_.clear();
    if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
        failure.error().state() == GoogleServiceAuthError::CAPTCHA_REQUIRED) {
      captcha_token_ = failure.error().captcha().token;
    }
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

void LoginPerformer::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {
  UserMetrics::RecordAction(UserMetricsAction("Login_Success"));
  // 0 - Login success offline and online. It's a new user. or it's an
  //     existing user and offline auth took longer than online auth.
  // 1 - Login success offline only. It's an existing user login.
  UMA_HISTOGRAM_ENUMERATION("Login.SuccessReason", pending_requests, 2);

  VLOG(1) << "LoginSuccess, pending_requests " << pending_requests;
  if (delegate_) {
    // After delegate_->OnLoginSuccess(...) is called, delegate_ releases
    // LoginPerformer ownership. LP now manages it's lifetime on its own.
    // 2 things could make it exist longer:
    // 1. ScreenLock active (pending correct new password input)
    // 2. Pending online auth request.
    if (!pending_requests)
      MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    else
      initial_online_auth_pending_ = true;

    delegate_->OnLoginSuccess(username,
                              password,
                              credentials,
                              pending_requests);
    return;
  } else {
    DCHECK(!pending_requests)
        << "Pending request w/o delegate_ should not happen!";
    // Online login has succeeded.
    Profile* profile =
        g_browser_process->profile_manager()->GetDefaultProfile();
    LoginUtils::Get()->FetchCookies(profile, credentials);
    LoginUtils::Get()->FetchTokens(profile, credentials);

    // Don't unlock screen if it was locked while we're waiting
    // for initial online auth.
    if (ScreenLocker::default_screen_locker() &&
        !initial_online_auth_pending_) {
      DVLOG(1) << "Online login OK - unlocking screen.";
      RequestScreenUnlock();
      // Do not delete itself just yet, wait for unlock.
      // See ResolveScreenUnlocked().
      return;
    }
    initial_online_auth_pending_ = false;
    // There's nothing else that's holding LP from deleting itself -
    // no ScreenLock, no pending requests.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void LoginPerformer::OnOffTheRecordLoginSuccess() {
  UserMetrics::RecordAction(
      UserMetricsAction("Login_GuestLoginSuccess"));

  if (delegate_)
    delegate_->OnOffTheRecordLoginSuccess();
  else
    NOTREACHED();
}

void LoginPerformer::OnPasswordChangeDetected(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  cached_credentials_ = credentials;
  if (delegate_) {
    delegate_->OnPasswordChangeDetected(credentials);
  } else {
    last_login_failure_ =
        LoginFailure::FromNetworkAuthFailure(GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    password_changed_ = true;
    DVLOG(1) << "Password change detected - locking screen.";
    RequestScreenLock();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, SignedSettingsHelper::Callback implementation:

void LoginPerformer::OnCheckWhitelistCompleted(SignedSettings::ReturnCode code,
                                               const std::string& email) {
  if (code == SignedSettings::SUCCESS) {
    // Whitelist check passed, continue with authentication.
    StartAuthentication();
  } else {
    if (delegate_)
      delegate_->WhiteListCheckFailed(email);
    else
      NOTREACHED();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, NotificationObserver implementation:
//

void LoginPerformer::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type != NotificationType::SCREEN_LOCK_STATE_CHANGED)
    return;

  bool is_screen_locked = *Details<bool>(details).ptr();
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

void LoginPerformer::Login(const std::string& username,
                           const std::string& password) {
  username_ = username;
  password_ = password;

  // Whitelist check is always performed during initial login and
  // should not be performed when ScreenLock is active (pending online auth).
  if (!ScreenLocker::default_screen_locker()) {
    // Must not proceed without signature verification.
    UserCrosSettingsProvider user_settings;
    bool trusted_setting_available = user_settings.RequestTrustedAllowNewUser(
        method_factory_.NewRunnableMethod(&LoginPerformer::Login,
                                          username,
                                          password));
    if (!trusted_setting_available) {
      // Value of AllowNewUser setting is still not verified.
      // Another attempt will be invoked after verification completion.
      return;
    }
  }

  if (ScreenLocker::default_screen_locker() ||
      UserCrosSettingsProvider::cached_allow_new_user()) {
    // Starts authentication if guest login is allowed or online auth pending.
    StartAuthentication();
  } else {
    // Otherwise, do whitelist check first.
    SignedSettingsHelper::Get()->StartCheckWhitelistOp(
        username, this);
  }
}

void LoginPerformer::LoginOffTheRecord() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::LoginOffTheRecord));
}

void LoginPerformer::RecoverEncryptedData(const std::string& old_password) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::RecoverEncryptedData,
                        old_password,
                        cached_credentials_));
  cached_credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

void LoginPerformer::ResyncEncryptedData() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::ResyncEncryptedData,
                        cached_credentials_));
  cached_credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

////////////////////////////////////////////////////////////////////////////////
// LoginPerformer, private:

void LoginPerformer::RequestScreenLock() {
  DVLOG(1) << "Screen lock requested";
  // Will receive notifications on screen unlock and delete itself.
  registrar_.Add(this,
                 NotificationType::SCREEN_LOCK_STATE_CHANGED,
                 NotificationService::AllSources());
  if (ScreenLocker::default_screen_locker()) {
    DVLOG(1) << "Screen already locked";
    ResolveScreenLocked();
  } else {
    screen_lock_requested_ = true;
    chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->
        NotifyScreenLockRequested();
  }
}

void LoginPerformer::RequestScreenUnlock() {
  DVLOG(1) << "Screen unlock requested";
  if (ScreenLocker::default_screen_locker()) {
    chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->
        NotifyScreenUnlockRequested();
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
      // User is requested to enter CAPTCHA challenge.
      captcha_token_ = last_login_failure_.error().captcha().token;
      RequestScreenLock();
      return;
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

  string16 msg;
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
      msg = l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_PASSWORD_CHANGED);
      break;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      // Access not granted. User has to sign out.
      // Show error message using existing screen lock.
      msg = l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_RESTRICTED);
      sign_out_only = true;
      break;
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
      // User is requested to enter CAPTCHA challenge.
      captcha_token_ = last_login_failure_.error().captcha().token;
      msg = l10n_util::GetStringUTF16(IDS_LOGIN_ERROR_PASSWORD_CHANGED);
      ScreenLocker::default_screen_locker()->ShowCaptchaAndErrorMessage(
          last_login_failure_.error().captcha().image_url,
          UTF16ToWide(msg));
      return;
    default:
      // Unless there's new GoogleServiceAuthError state has been added.
      NOTREACHED();
      break;
  }

  ScreenLocker::default_screen_locker()->ShowErrorMessage(UTF16ToWide(msg),
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

void LoginPerformer::StartAuthentication() {
  DVLOG(1) << "Auth started";
  BootTimesLoader::Get()->AddLoginTimeMarker("AuthStarted", false);
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  if (delegate_) {
    authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(authenticator_.get(),
                          &Authenticator::AuthenticateToLogin,
                          profile,
                          username_,
                          password_,
                          captcha_token_,
                          captcha_));
  } else {
    DCHECK(authenticator_.get())
        << "Authenticator instance doesn't exist for login attempt retry.";
    // At this point offline auth has been successful,
    // retry online auth, using existing Authenticator instance.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(authenticator_.get(),
                          &Authenticator::RetryAuth,
                          profile,
                          username_,
                          password_,
                          captcha_token_,
                          captcha_));
  }
  password_.clear();
}

}  // namespace chromeos
