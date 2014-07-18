// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_LOGIN_PERFORMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_LOGIN_PERFORMER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/auth/authenticator.h"
#include "chrome/browser/chromeos/login/auth/extended_authenticator.h"
#include "chrome/browser/chromeos/login/auth/online_attempt_host.h"
#include "chrome/browser/chromeos/policy/wildcard_login_checker.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/user_context.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace policy {
class WildcardLoginChecker;
}

namespace chromeos {

// This class encapsulates sign in operations.
// Sign in is performed in a way that offline auth is executed first.
// Once offline auth is OK - user homedir is mounted, UI is launched.
// At this point LoginPerformer |delegate_| is destroyed and it releases
// LP instance ownership. LP waits for online login result.
// If auth is succeeded, cookie fetcher is executed, LP instance deletes itself.
//
// If |delegate_| is not NULL it will handle error messages, password input.
class LoginPerformer : public AuthStatusConsumer,
                       public OnlineAttemptHost::Delegate {
 public:
  typedef enum AuthorizationMode {
    // Authorization performed internally by Chrome.
    AUTH_MODE_INTERNAL,
    // Authorization performed by an extension.
    AUTH_MODE_EXTENSION
  } AuthorizationMode;

  // Delegate class to get notifications from the LoginPerformer.
  class Delegate : public AuthStatusConsumer {
   public:
    virtual ~Delegate() {}
    virtual void WhiteListCheckFailed(const std::string& email) = 0;
    virtual void PolicyLoadFailed() = 0;
    virtual void OnOnlineChecked(const std::string& email, bool success) = 0;
  };

  explicit LoginPerformer(Delegate* delegate);
  virtual ~LoginPerformer();

  // AuthStatusConsumer implementation:
  virtual void OnAuthFailure(const AuthFailure& error) OVERRIDE;
  virtual void OnRetailModeAuthSuccess(
      const UserContext& user_context) OVERRIDE;
  virtual void OnAuthSuccess(const UserContext& user_context) OVERRIDE;
  virtual void OnOffTheRecordAuthSuccess() OVERRIDE;
  virtual void OnPasswordChangeDetected() OVERRIDE;

  // Performs a login for |user_context|.
  // If auth_mode is AUTH_MODE_EXTENSION, there are no further auth checks,
  // AUTH_MODE_INTERNAL will perform auth checks.
  void PerformLogin(const UserContext& user_context,
                    AuthorizationMode auth_mode);

  // Performs supervised user login with a given |user_context|.
  void LoginAsSupervisedUser(const UserContext& user_context);

  // Performs retail mode login.
  void LoginRetailMode();

  // Performs actions to prepare guest mode login.
  void LoginOffTheRecord();

  // Performs a login into the public account identified by |username|.
  void LoginAsPublicAccount(const std::string& username);

  // Performs a login into the kiosk mode account with |app_user_id|.
  void LoginAsKioskAccount(const std::string& app_user_id,
                           bool use_guest_mount);

  // Migrates cryptohome using |old_password| specified.
  void RecoverEncryptedData(const std::string& old_password);

  // Reinitializes cryptohome with the new password.
  void ResyncEncryptedData();

  // Returns latest auth error.
  const GoogleServiceAuthError& error() const {
    return last_login_failure_.error();
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
  // Starts login completion of externally authenticated user.
  void StartLoginCompletion();

  // Starts authentication.
  void StartAuthentication();

  // Completion callback for the online wildcard login check for enterprise
  // devices. Continues the login process or signals whitelist check failure
  // depending on the value of |result|.
  void OnlineWildcardLoginCheckCompleted(
      policy::WildcardLoginChecker::Result result);

  // Used for logging in.
  scoped_refptr<Authenticator> authenticator_;
  scoped_refptr<ExtendedAuthenticator> extended_authenticator_;

  // Used to make auxiliary online check.
  OnlineAttemptHost online_attempt_host_;

  // Represents last login failure that was encountered when communicating to
  // sign-in server. AuthFailure.LoginFailureNone() by default.
  AuthFailure last_login_failure_;

  // User credentials for the current login attempt.
  UserContext user_context_;

  // Notifications receiver.
  Delegate* delegate_;

  // True if password change has been detected.
  // Once correct password is entered homedir migration is executed.
  bool password_changed_;
  int password_changed_callback_count_;

  // Authorization mode type.
  AuthorizationMode auth_mode_;

  // Used to verify logins that matched wildcard on the login whitelist.
  scoped_ptr<policy::WildcardLoginChecker> wildcard_login_checker_;

  base::WeakPtrFactory<LoginPerformer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginPerformer);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTH_LOGIN_PERFORMER_H_
