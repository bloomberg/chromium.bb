// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/online_attempt_host.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"

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
// TODO(nkostylev): Cleanup ClientLogin related code, update class description.
class LoginPerformer : public LoginStatusConsumer,
                       public content::NotificationObserver,
                       public OnlineAttemptHost::Delegate {
 public:
  typedef enum AuthorizationMode {
    // Authorization performed internally by Chrome.
    AUTH_MODE_INTERNAL,
    // Authorization performed by an extension.
    AUTH_MODE_EXTENSION
  } AuthorizationMode;

  // Delegate class to get notifications from the LoginPerformer.
  class Delegate : public LoginStatusConsumer {
   public:
    virtual ~Delegate() {}
    virtual void WhiteListCheckFailed(const std::string& email) = 0;
    virtual void PolicyLoadFailed() = 0;
    virtual void OnOnlineChecked(const std::string& email, bool success) = 0;
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
  virtual void OnLoginFailure(const LoginFailure& error) OVERRIDE;
  virtual void OnRetailModeLoginSuccess(
      const UserContext& user_context) OVERRIDE;
  virtual void OnLoginSuccess(
      const UserContext& user_context,
      bool pending_requests,
      bool using_oauth) OVERRIDE;
  virtual void OnOffTheRecordLoginSuccess() OVERRIDE;
  virtual void OnPasswordChangeDetected() OVERRIDE;

  // Performs a login for |user_context|.
  // If auth_mode is AUTH_MODE_EXTENSION, there are no further auth checks,
  // AUTH_MODE_INTERNAL will perform auth checks.
  void PerformLogin(const UserContext& user_context,
                    AuthorizationMode auth_mode);

  // Performs locally managed user login with a given |user_context|.
  void LoginAsLocallyManagedUser(const UserContext& user_context);

  // Performs retail mode login.
  void LoginRetailMode();

  // Performs actions to prepare guest mode login.
  void LoginOffTheRecord();

  // Performs a login into the public account identified by |username|.
  void LoginAsPublicAccount(const std::string& username);

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

  // True if password change has been detected.
  bool password_changed() { return password_changed_; }

  // Number of times we've been called with OnPasswordChangeDetected().
  // If user enters incorrect old password, same LoginPerformer instance will
  // be called so callback count makes it possible to distinguish initial
  // "password changed detected" event from further attempts to enter old
  // password for cryptohome migration (when > 1).
  int password_changed_callback_count() {
    return password_changed_callback_count_;
  }

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  AuthorizationMode auth_mode() const { return auth_mode_; }

 protected:
  // Implements OnlineAttemptHost::Delegate.
  virtual void OnChecked(const std::string& username, bool success) OVERRIDE;

 private:
  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

  // Starts login completion of externally authenticated user.
  void StartLoginCompletion();

  // Starts authentication.
  void StartAuthentication();

  // Default performer. Will be used by ScreenLocker.
  static LoginPerformer* default_performer_;

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;

  // Used to make auxiliary online check.
  OnlineAttemptHost online_attempt_host_;

  // Represents last login failure that was encountered when communicating to
  // sign-in server. LoginFailure.LoginFailureNone() by default.
  LoginFailure last_login_failure_;

  // User credentials for the current login attempt.
  UserContext user_context_;

  // Notifications receiver.
  Delegate* delegate_;

  // True if password change has been detected.
  // Once correct password is entered homedir migration is executed.
  bool password_changed_;
  int password_changed_callback_count_;

  // Used for ScreenLock notifications.
  content::NotificationRegistrar registrar_;

  // True if LoginPerformer has requested screen lock. Used to distinguish
  // such requests with cases when screen is locked on its own.
  bool screen_lock_requested_;

  // True if LoginPerformer instance is waiting for the initial (very first one)
  // online authentication response. Used to distinguish cases when screen
  // is locked during that stage. No need to resolve screen lock action then.
  bool initial_online_auth_pending_;

  // Authorization mode type.
  AuthorizationMode auth_mode_;

  base::WeakPtrFactory<LoginPerformer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginPerformer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_PERFORMER_H_
