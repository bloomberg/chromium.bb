// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PARALLEL_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PARALLEL_AUTHENTICATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/auth_attempt_state_resolver.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/online_attempt.h"
#include "chrome/browser/chromeos/login/test_attempt_state.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class LoginFailure;
class Profile;

namespace chromeos {

class LoginStatusConsumer;

// Authenticates a Chromium OS user against the Google Accounts ClientLogin API.
//
// Simultaneously attempts authentication both offline and online.
//
// At a high, level, here's what happens:
// AuthenticateToLogin() creates an OnlineAttempt and calls a Cryptohome's
// method to perform online and offline login simultaneously.  When one of
// these completes, it will store results in a AuthAttemptState owned by
// ParallelAuthenticator and then call Resolve().  Resolve() will attempt to
// determine which AuthState we're in, based on the info at hand.
// It then triggers further action based on the calculated AuthState; this
// further action might include calling back the passed-in LoginStatusConsumer
// to signal that login succeeded or failed, waiting for more outstanding
// operations to complete, or triggering some more Cryptohome method calls.
class ParallelAuthenticator : public Authenticator,
                              public AuthAttemptStateResolver {
 public:
  enum AuthState {
    CONTINUE,        // State indeterminate; try again when more info available.
    NO_MOUNT,        // Cryptohome doesn't exist yet.
    FAILED_MOUNT,    // Failed to mount existing cryptohome.
    FAILED_REMOVE,   // Failed to remove existing cryptohome.
    FAILED_TMPFS,    // Failed to mount tmpfs for guest user
    CREATE_NEW,      // Need to create cryptohome for a new user.
    RECOVER_MOUNT,   // After RecoverEncryptedData, mount cryptohome.
    POSSIBLE_PW_CHANGE,  // Offline login failed, user may have changed pw.
    NEED_NEW_PW,     // User changed pw, and we have the old one.
    NEED_OLD_PW,     // User changed pw, and we have the new one.
    HAVE_NEW_PW,     // We have verified new pw, time to migrate key.
    OFFLINE_LOGIN,   // Login succeeded offline.
    DEMO_LOGIN,      // Logged in as the demo user.
    ONLINE_LOGIN,    // Offline and online login succeeded.
    UNLOCK,          // Screen unlock succeeded.
    ONLINE_FAILED,   // Online login disallowed, but offline succeeded.
    GUEST_LOGIN,     // Logged in guest mode.
    LOGIN_FAILED,    // Login denied.
    OWNER_REQUIRED   // Login is restricted to the owner only.
  };

  explicit ParallelAuthenticator(LoginStatusConsumer* consumer);

  // Authenticator overrides.
  virtual void CompleteLogin(Profile* profile,
                             const std::string& username,
                             const std::string& password) OVERRIDE;

  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers and your Chrome OS device simultaneously.
  // As soon as we have successfully mounted the encrypted home directory for
  // |username|, we will call consumer_->OnLoginSuccess() with |username| and a
  // vector of authentication cookies.  If we're still waiting for an online
  // result at that time, we'll also pass back a flag indicating that more
  // callbacks are on the way; if not, we pass back false.  When the pending
  // request completes, either consumer_->OnLoginSuccess() with an indication
  // that no more requests are outstanding will be called, or
  // consumer_->OnLoginFailure() if appropriate.
  //
  // Upon failure to login (online fails, then offline fails;
  // offline fails, then online fails) consumer_->OnLoginFailure() is called
  // with an error message.
  //
  // In the event that we see an online success and then an offline failure,
  // consumer_->OnPasswordChangeDetected() is called.
  //
  // Uses |profile| when doing URL fetches.
  // Optionally could pass CAPTCHA challenge token - |login_token| and
  // |login_captcha| string that user has entered.
  //
  // NOTE: We do not allow HOSTED accounts to log in.  In the event that
  // we are asked to authenticate valid HOSTED account creds, we will
  // call OnLoginFailure() with HOSTED_NOT_ALLOWED.
  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha) OVERRIDE;

  // Given a |username| and |password|, this method attempts to
  // authenticate to the cached credentials. This will never contact
  // the server even if it's online. The auth result is sent to
  // LoginStatusConsumer in a same way as AuthenticateToLogin does.
  virtual void AuthenticateToUnlock(const std::string& username,
                                    const std::string& password) OVERRIDE;

  // Initiates demo user login.
  // Mounts tmpfs and notifies consumer on the success/failure.
  virtual void LoginDemoUser() OVERRIDE;

  // Initiates incognito ("browse without signing in") login.
  // Mounts tmpfs and notifies consumer on the success/failure.
  virtual void LoginOffTheRecord() OVERRIDE;

  // These methods must be called on the UI thread, as they make DBus calls
  // and also call back to the login UI.
  virtual void OnDemoUserLoginSuccess()  OVERRIDE;
  virtual void OnLoginSuccess(bool request_pending)  OVERRIDE;
  virtual void OnLoginFailure(const LoginFailure& error) OVERRIDE;
  virtual void RecoverEncryptedData(
      const std::string& old_password) OVERRIDE;
  virtual void ResyncEncryptedData() OVERRIDE;
  virtual void RetryAuth(Profile* profile,
                         const std::string& username,
                         const std::string& password,
                         const std::string& login_token,
                         const std::string& login_captcha) OVERRIDE;
  // AuthAttemptStateResolver overrides.
  // Attempts to make a decision and call back |consumer_| based on
  // the state we have gathered at the time of call.  If a decision
  // can't be made, defers until the next time this is called.
  // When a decision is made, will call back to |consumer_| on the UI thread.
  //
  // Must be called on the UI thread.
  virtual void Resolve() OVERRIDE;

  void OnOffTheRecordLoginSuccess();
  void OnPasswordChangeDetected();

 protected:
  virtual ~ParallelAuthenticator();

 private:
  friend class ParallelAuthenticatorTest;
  FRIEND_TEST_ALL_PREFIXES(ParallelAuthenticatorTest,
                           ResolveOwnerNeededDirectFailedMount);
  FRIEND_TEST_ALL_PREFIXES(ParallelAuthenticatorTest, ResolveOwnerNeededMount);
  FRIEND_TEST_ALL_PREFIXES(ParallelAuthenticatorTest,
                           ResolveOwnerNeededFailedMount);

  // Returns the AuthState we're in, given the status info we have at
  // the time of call.
  // Must be called on the IO thread.
  AuthState ResolveState();

  // Helper for ResolveState().
  // Given that we're attempting to auth the user again, with a new password,
  // determine which state we're in.  Returns CONTINUE if no resolution.
  // Must be called on the IO thread.
  AuthState ResolveReauthState();

  // Helper for ResolveState().
  // Given that some cryptohome operation has failed, determine which of the
  // possible failure states we're in.
  // Must be called on the IO thread.
  AuthState ResolveCryptohomeFailureState();

  // Helper for ResolveState().
  // Given that some cryptohome operation has succeeded, determine which of
  // the possible states we're in.
  // Must be called on the IO thread.
  AuthState ResolveCryptohomeSuccessState();

  // Helper for ResolveState().
  // Given that some online auth operation has failed, determine which of the
  // possible failure states we're in.  Handles both failure to complete and
  // actual failure responses from the server.
  // Must be called on the IO thread.
  AuthState ResolveOnlineFailureState(AuthState offline_state);

  // Helper for ResolveState().
  // Given that some online auth operation has succeeded, determine which of
  // the possible success states we're in.
  // Must be called on the IO thread.
  AuthState ResolveOnlineSuccessState(AuthState offline_state);

  // Used to disable oauth, used for testing.
  void set_using_oauth(bool value) {
    using_oauth_ = value;
  }

  // Used for testing.
  void set_attempt_state(TestAttemptState* new_state) {  // takes ownership.
    current_state_.reset(new_state);
  }

  // Sets an online attemp for testing.
  void set_online_attempt(OnlineAttempt* attempt) {
    current_online_.reset(attempt);
  }

  // Used for testing to set the expected state of an owner check.
  void SetOwnerState(bool owner_check_finished, bool check_result);

  // If we don't have the system salt yet, loads it from the CryptohomeLibrary.
  void LoadSystemSalt();
  // If we don't have supplemental_user_key_ yet, loads it from the NSS DB.
  // Returns false if the key can not be loaded/created.
  bool LoadSupplementalUserKey();

  // checks if the current mounted home contains the owner case and either
  // continues or fails the log-in. Used for policy lost mitigation "safe-mode".
  // Returns true if the owner check has been successful or if it is not needed.
  bool VerifyOwner();

  // Handles completion of the ownership check and continues login.
  void OnOwnershipChecked(DeviceSettingsService::OwnershipStatus status,
                          bool is_owner);

  // Records OAuth1 access token verification failure for |user_account|.
  void RecordOAuthCheckFailure(const std::string& user_account);

  // Signal login completion status for cases when a new user is added via
  // an external authentication provider (i.e. GAIA extension).
  void ResolveLoginCompletionStatus();

  // Used when we need to try online authentication again, after successful
  // mount, but failed online login.
  scoped_ptr<AuthAttemptState> reauth_state_;

  scoped_ptr<AuthAttemptState> current_state_;
  scoped_ptr<OnlineAttempt> current_online_;
  bool migrate_attempted_;
  bool remove_attempted_;
  bool mount_guest_attempted_;
  bool check_key_attempted_;

  // When the user has changed her password, but gives us the old one, we will
  // be able to mount her cryptohome, but online authentication will fail.
  // This allows us to present the same behavior to the caller, regardless
  // of the order in which we receive these results.
  bool already_reported_success_;
  base::Lock success_lock_;  // A lock around |already_reported_success_|.

  // Flags signaling whether the owner verification has been done and the result
  // of it.
  bool owner_is_verified_;
  bool user_can_login_;

  // True if we use OAuth-based authentication flow.
  bool using_oauth_;

  DISALLOW_COPY_AND_ASSIGN(ParallelAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PARALLEL_AUTHENTICATOR_H_
