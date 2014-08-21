// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_CRYPTOHOME_AUTHENTICATOR_H_
#define CHROMEOS_LOGIN_AUTH_CRYPTOHOME_AUTHENTICATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/login/auth/auth_attempt_state.h"
#include "chromeos/login/auth/auth_attempt_state_resolver.h"
#include "chromeos/login/auth/authenticator.h"
#include "chromeos/login/auth/test_attempt_state.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class AuthFailure;
class Profile;

namespace chromeos {

class AuthStatusConsumer;

// Authenticates a Chromium OS user against cryptohome.
// Relies on the fact that online authentications has been already performed
// (i.e. using_oauth_ is true).
//
// At a high, level, here's what happens:
// AuthenticateToLogin() calls a Cryptohome's method to perform offline login.
// Resultes are stored in a AuthAttemptState owned by CryptohomeAuthenticator
// and then call Resolve().  Resolve() will attempt to
// determine which AuthState we're in, based on the info at hand.
// It then triggers further action based on the calculated AuthState; this
// further action might include calling back the passed-in AuthStatusConsumer
// to signal that login succeeded or failed, waiting for more outstanding
// operations to complete, or triggering some more Cryptohome method calls.
//
// Typical flows
// -------------
// Add new user: CONTINUE > CONTINUE > CREATE_NEW > CONTINUE > ONLINE_LOGIN
// Login as existing user: CONTINUE > OFFLINE_LOGIN
// Login as existing user (failure): CONTINUE > FAILED_MOUNT
// Change password detected:
//   GAIA online ok: CONTINUE > CONTINUE > NEED_OLD_PW
//     Recreate: CREATE_NEW > CONTINUE > ONLINE_LOGIN
//     Old password failure: NEED_OLD_PW
//     Old password ok: RECOVER_MOUNT > CONTINUE > ONLINE_LOGIN
//
class CHROMEOS_EXPORT CryptohomeAuthenticator
    : public Authenticator,
      public AuthAttemptStateResolver {
 public:
  enum AuthState {
    CONTINUE = 0,            // State indeterminate; try again with more info.
    NO_MOUNT = 1,            // Cryptohome doesn't exist yet.
    FAILED_MOUNT = 2,        // Failed to mount existing cryptohome.
    FAILED_REMOVE = 3,       // Failed to remove existing cryptohome.
    FAILED_TMPFS = 4,        // Failed to mount tmpfs for guest user.
    FAILED_TPM = 5,          // Failed to mount/create cryptohome, TPM error.
    CREATE_NEW = 6,          // Need to create cryptohome for a new user.
    RECOVER_MOUNT = 7,       // After RecoverEncryptedData, mount cryptohome.
    POSSIBLE_PW_CHANGE = 8,  // Offline login failed, user may have changed pw.
    NEED_NEW_PW = 9,         // Obsolete (ClientLogin): user changed pw,
                             // we have the old one.
    NEED_OLD_PW = 10,        // User changed pw, and we have the new one
                             // (GAIA auth is OK).
    HAVE_NEW_PW = 11,        // Obsolete (ClientLogin): We have verified new pw,
                             // time to migrate key.
    OFFLINE_LOGIN = 12,      // Login succeeded offline.
    DEMO_LOGIN = 13,         // Logged in as the demo user.
    ONLINE_LOGIN = 14,       // Offline and online login succeeded.
    UNLOCK = 15,             // Screen unlock succeeded.
    ONLINE_FAILED = 16,      // Obsolete (ClientLogin): Online login disallowed,
                             // but offline succeeded.
    GUEST_LOGIN = 17,        // Logged in guest mode.
    PUBLIC_ACCOUNT_LOGIN = 18,        // Logged into a public account.
    SUPERVISED_USER_LOGIN = 19,       // Logged in as a supervised user.
    LOGIN_FAILED = 20,                // Login denied.
    OWNER_REQUIRED = 21,              // Login is restricted to the owner only.
    FAILED_USERNAME_HASH = 22,        // Failed GetSanitizedUsername request.
    KIOSK_ACCOUNT_LOGIN = 23,         // Logged into a kiosk account.
    REMOVED_DATA_AFTER_FAILURE = 24,  // Successfully removed the user's
                                      // cryptohome after a login failure.
  };

  CryptohomeAuthenticator(scoped_refptr<base::TaskRunner> task_runner,
                          AuthStatusConsumer* consumer);

  // Authenticator overrides.
  virtual void CompleteLogin(Profile* profile,
                             const UserContext& user_context) OVERRIDE;

  // Given |user_context|, this method attempts to authenticate to your
  // Chrome OS device. As soon as we have successfully mounted the encrypted
  // home directory for the user, we will call consumer_->OnAuthSuccess()
  // with the username.
  // Upon failure to login consumer_->OnAuthFailure() is called
  // with an error message.
  //
  // Uses |profile| when doing URL fetches.
  virtual void AuthenticateToLogin(Profile* profile,
                                   const UserContext& user_context) OVERRIDE;

  // Given |user_context|, this method attempts to authenticate to the cached
  // user_context. This will never contact the server even if it's online.
  // The auth result is sent to AuthStatusConsumer in a same way as
  // AuthenticateToLogin does.
  virtual void AuthenticateToUnlock(const UserContext& user_context) OVERRIDE;

  // Initiates supervised user login.
  // Creates cryptohome if missing or mounts existing one and
  // notifies consumer on the success/failure.
  virtual void LoginAsSupervisedUser(const UserContext& user_context) OVERRIDE;

  // Initiates retail mode login.
  // Mounts tmpfs and notifies consumer on the success/failure.
  virtual void LoginRetailMode() OVERRIDE;

  // Initiates incognito ("browse without signing in") login.
  // Mounts tmpfs and notifies consumer on the success/failure.
  virtual void LoginOffTheRecord() OVERRIDE;

  // Initiates login into a public session.
  // Mounts an ephemeral cryptohome and notifies consumer on the
  // success/failure.
  virtual void LoginAsPublicSession(const UserContext& user_context) OVERRIDE;

  // Initiates login into the kiosk mode account identified by |app_user_id|.
  // Mounts an ephemeral guest cryptohome if |use_guest_mount| is |true|.
  // Otherwise, mounts a public cryptohome, which will be ephemeral if the
  // |DeviceEphemeralUsersEnabled| policy is enabled and non-ephemeral
  // otherwise.
  virtual void LoginAsKioskAccount(const std::string& app_user_id,
                                   bool use_guest_mount) OVERRIDE;

  // These methods must be called on the UI thread, as they make DBus calls
  // and also call back to the login UI.
  virtual void OnRetailModeAuthSuccess() OVERRIDE;
  virtual void OnAuthSuccess() OVERRIDE;
  virtual void OnAuthFailure(const AuthFailure& error) OVERRIDE;
  virtual void RecoverEncryptedData(const std::string& old_password) OVERRIDE;
  virtual void ResyncEncryptedData() OVERRIDE;

  // AuthAttemptStateResolver overrides.
  // Attempts to make a decision and call back |consumer_| based on
  // the state we have gathered at the time of call.  If a decision
  // can't be made, defers until the next time this is called.
  // When a decision is made, will call back to |consumer_| on the UI thread.
  //
  // Must be called on the UI thread.
  virtual void Resolve() OVERRIDE;

  void OnOffTheRecordAuthSuccess();
  void OnPasswordChangeDetected();

 protected:
  virtual ~CryptohomeAuthenticator();

  typedef base::Callback<void(bool is_owner)> IsOwnerCallback;

  // Method to be implemented in child. Return |true| if user specified in
  // |context| exists on device.
  virtual bool IsKnownUser(const UserContext& context) = 0;

  // Method to be implemented in child. Return |true| if device is running
  // in safe mode.
  virtual bool IsSafeMode() = 0;

  // Method to be implemented in child. Have to call |callback| with boolean
  // parameter that indicates if user in |context| can act as an owner in
  // safe mode.
  virtual void CheckSafeModeOwnership(const UserContext& context,
                                      const IsOwnerCallback& callback) = 0;

 private:
  friend class CryptohomeAuthenticatorTest;
  FRIEND_TEST_ALL_PREFIXES(CryptohomeAuthenticatorTest,
                           ResolveOwnerNeededDirectFailedMount);
  FRIEND_TEST_ALL_PREFIXES(CryptohomeAuthenticatorTest,
                           ResolveOwnerNeededMount);
  FRIEND_TEST_ALL_PREFIXES(CryptohomeAuthenticatorTest,
                           ResolveOwnerNeededFailedMount);

  // Removes the cryptohome of the user.
  void RemoveEncryptedData();

  // Returns the AuthState we're in, given the status info we have at
  // the time of call.
  // Must be called on the IO thread.
  AuthState ResolveState();

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
  // Given that some online auth operation has succeeded, determine which of
  // the possible success states we're in.
  // Must be called on the IO thread.
  AuthState ResolveOnlineSuccessState(AuthState offline_state);

  // Used for testing.
  void set_attempt_state(TestAttemptState* new_state) {  // takes ownership.
    current_state_.reset(new_state);
  }

  // Used for testing to set the expected state of an owner check.
  void SetOwnerState(bool owner_check_finished, bool check_result);

  // checks if the current mounted home contains the owner case and either
  // continues or fails the log-in. Used for policy lost mitigation "safe-mode".
  // Returns true if the owner check has been successful or if it is not needed.
  bool VerifyOwner();

  // Handles completion of the ownership check and continues login.
  void OnOwnershipChecked(bool is_owner);

  // Signal login completion status for cases when a new user is added via
  // an external authentication provider (i.e. GAIA extension).
  void ResolveLoginCompletionStatus();

  scoped_refptr<base::TaskRunner> task_runner_;

  scoped_ptr<AuthAttemptState> current_state_;
  bool migrate_attempted_;
  bool remove_attempted_;
  bool resync_attempted_;
  bool ephemeral_mount_attempted_;
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

  // Flag indicating to delete the user's cryptohome the login fails.
  bool remove_user_data_on_failure_;

  // When |remove_user_data_on_failure_| is set, we delay calling
  // consumer_->OnAuthFailure() until we removed the user cryptohome.
  const AuthFailure* delayed_login_failure_;

  DISALLOW_COPY_AND_ASSIGN(CryptohomeAuthenticator);
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_CRYPTOHOME_AUTHENTICATOR_H_
