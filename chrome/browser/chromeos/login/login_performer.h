// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/signed_settings_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace chromeos {

// This class encapsulates sign in operations.
// Sign in is performed in a way that offline auth is executed first.
// Once offline auth is OK - user homedir is mounted, UI is launched.
// At this point LoginPerformer |delegate_| is destroyed and it releases
// LP instance ownership. LP waits for online login result.
// If auth is succeeded, cookie fetcher is executed, LP instance deletes itself.
//
// If online login operation fails that means:
// (1) User password has changed. Ask user for the new password.
// (2) User password has changed and/or CAPTCHA input is required.
// (3) User account is deleted/disabled/not signed up.
// (4) Timeout/service unavailable/connection failed.
//
// Actions:
// (1)-(3): Request screen lock.
// (1) Ask for new user password.
// (2) Ask for new user password and/or CAPTCHA.
// (3) Display error message and allow "Sign Out" as the only action.
// (4) Delete LP instance since offline auth was OK.
//
// If |delegate_| is not NULL it will handle error messages,
// CAPTCHA dialog, password input.
// If |delegate_| is NULL that does mean that LoginPerformer instance
// is waiting for successful online login or blocked on online login failure.
// In case of failure password/captcha
// input & error messages display is dedicated to ScreenLocker instance.
//
// 2 things make LoginPerfrormer instance exist longer:
// 1. ScreenLock active (pending correct new password input)
// 2. Pending online auth request.
class LoginPerformer : public LoginStatusConsumer,
                       public SignedSettingsHelper::Callback,
                       public NotificationObserver,
                       public ProfileManager::Observer {
 public:
  // Delegate class to get notifications from the LoginPerformer.
  class Delegate : public LoginStatusConsumer {
   public:
    virtual ~Delegate() {}
    virtual void WhiteListCheckFailed(const std::string& email) = 0;
  };

  explicit LoginPerformer(Delegate* delegate);
  virtual ~LoginPerformer();

  // Returns the default instance if it has been created.
  // This instance is owned by delegate_ till it's destroyed.
  // When LP instance lives by itself it's used by ScreenLocker instance.
  static LoginPerformer* default_performer() {
    return default_performer_;
  }

  // LoginStatusConsumer implementation:
  virtual void OnLoginFailure(const LoginFailure& error);
  virtual void OnLoginSuccess(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests);
  virtual void OnOffTheRecordLoginSuccess();
  virtual void OnPasswordChangeDetected(
      const GaiaAuthConsumer::ClientLoginResult& credentials);

  // SignedSettingsHelper::Callback implementation:
  virtual void OnCheckWhitelistCompleted(SignedSettings::ReturnCode code,
                                         const std::string& email);

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Performs login with the |username| and |password| specified.
  void Login(const std::string& username, const std::string& password);

  // Performs actions to prepare Guest mode login.
  void LoginOffTheRecord();

  // Migrates cryptohome using |old_password| specified.
  void RecoverEncryptedData(const std::string& old_password);

  // Reinitializes cryptohome with the new password.
  void ResyncEncryptedData();

  // Returns latest auth error.
  const GoogleServiceAuthError& error() const {
    return last_login_failure_.error();
  }

  // True if last login operation has timed out.
  bool login_timed_out() {
    return last_login_failure_.reason() == LoginFailure::LOGIN_TIMED_OUT;
  }

  void set_captcha(const std::string& captcha) { captcha_ = captcha; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

 private:
  // ProfeleManager::Observer implementation:
  void OnProfileCreated(Profile* profile);

  // Requests screen lock and subscribes to screen lock notifications.
  void RequestScreenLock();

  // Requests screen unlock.
  void RequestScreenUnlock();

  // Resolves initial LoginFailure::NETWORK_AUTH_FAILED error i.e.
  // when screen is not locked yet.
  void ResolveInitialNetworkAuthFailure();

  // Resolves LoginFailure when screen is locked.
  void ResolveLockLoginFailure();

  // Resolves LoginFailure::NETWORK_AUTH_FAILED error when screen is locked.
  // Uses ScreenLocker to show error message based on |last_login_failure_|.
  void ResolveLockNetworkAuthFailure();

  // Resolve ScreenLock changed state.
  void ResolveScreenLocked();
  void ResolveScreenUnlocked();

  // Starts authentication.
  void StartAuthentication();

  // Default performer. Will be used by ScreenLocker.
  static LoginPerformer* default_performer_;

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;

  // Represents last login failure that was encountered when communicating to
  // sign-in server. LoginFailure.None() by default.
  LoginFailure last_login_failure_;

  // String entered by the user as an answer to a CAPTCHA challenge.
  std::string captcha_;

  // Token representing the specific CAPTCHA challenge.
  std::string captcha_token_;

  // Cached credentials data when password change is detected.
  GaiaAuthConsumer::ClientLoginResult cached_credentials_;

  // Username and password for the current login attempt.
  std::string username_;
  std::string password_;

  // Notifications receiver.
  Delegate* delegate_;

  // True if password change has been detected.
  // Once correct password is entered homedir migration is executed.
  bool password_changed_;

  // Used for ScreenLock notifications.
  NotificationRegistrar registrar_;

  // True if LoginPerformer has requested screen lock. Used to distinguish
  // such requests with cases when screen is locked on its own.
  bool screen_lock_requested_;

  // True if LoginPerformer instance is waiting for the initial (very first one)
  // online authentication response. Used to distinguish cases when screen
  // is locked during that stage. No need to resolve screen lock action then.
  bool initial_online_auth_pending_;

  GaiaAuthConsumer::ClientLoginResult credentials_;

  ScopedRunnableMethodFactory<LoginPerformer> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginPerformer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
