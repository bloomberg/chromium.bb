// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PARALLEL_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PARALLEL_AUTHENTICATOR_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/auth_attempt_state_resolver.h"
#include "chrome/browser/chromeos/login/test_attempt_state.h"
#include "chrome/browser/chromeos/login/cryptohome_op.h"
#include "chrome/browser/chromeos/login/online_attempt.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"

class GaiaAuthFetcher;
class LoginFailure;
class Profile;

namespace base {
class Lock;
}

namespace chromeos {

class LoginStatusConsumer;
class ParallelAuthenticator;
class ResolveChecker;

// Authenticates a Chromium OS user against the Google Accounts ClientLogin API.
//
// Simultaneously attempts authentication both offline and online, failing over
// to the "localaccount" in the event that authentication fails.
//
// At a high, level, here's what happens:
// AuthenticateToLogin() creates an OnlineAttempt and a CryptohomeOp that
// attempt to perform online and offline login simultaneously.  When one of
// these completes, it will store results in a AuthAttemptState owned by
// ParallelAuthenticator and then call Resolve().  Resolve() will attempt to
// determine which AuthState we're in, based on the info at hand.
// It then triggers further action based on the calculated AuthState; this
// further action might include calling back the passed-in LoginStatusConsumer
// to signal that login succeeded or failed, waiting for more outstanding
// operations to complete, or triggering some more CryptohomeOps.
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
    ONLINE_LOGIN,    // Offline and online login succeeded.
    UNLOCK,          // Screen unlock succeeded.
    LOCAL_LOGIN,     // Login with localaccount succeded.
    ONLINE_FAILED,   // Online login disallowed, but offline succeeded.
    LOGIN_FAILED     // Login denied.
  };

  explicit ParallelAuthenticator(LoginStatusConsumer* consumer);
  virtual ~ParallelAuthenticator();

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
  //
  // Returns true if the attempt gets sent successfully and false if not.
  bool AuthenticateToLogin(Profile* profile,
                           const std::string& username,
                           const std::string& password,
                           const std::string& login_token,
                           const std::string& login_captcha);

  // Given a |username| and |password|, this method attempts to
  // authenticate to the cached credentials. This will never contact
  // the server even if it's online. The auth result is sent to
  // LoginStatusConsumer in a same way as AuthenticateToLogin does.
  bool AuthenticateToUnlock(const std::string& username,
                            const std::string& password);

  // Initiates incognito ("browse without signing in") login.
  // Mounts tmpfs and notifies consumer on the success/failure.
  void LoginOffTheRecord();

  // These methods must be called on the UI thread, as they make DBus calls
  // and also call back to the login UI.
  void OnLoginSuccess(const GaiaAuthConsumer::ClientLoginResult& credentials,
                      bool request_pending);
  void OnOffTheRecordLoginSuccess();
  void OnPasswordChangeDetected(
      const GaiaAuthConsumer::ClientLoginResult& credentials);
  void OnLoginFailure(const LoginFailure& error);

  void RecoverEncryptedData(
      const std::string& old_password,
      const GaiaAuthConsumer::ClientLoginResult& credentials);
  void ResyncEncryptedData(
      const GaiaAuthConsumer::ClientLoginResult& credentials);
  void RetryAuth(Profile* profile,
                 const std::string& username,
                 const std::string& password,
                 const std::string& login_token,
                 const std::string& login_captcha);

  // Call this on the FILE thread.
  void CheckLocalaccount(const LoginFailure& error);

  // Attempts to make a decision and call back |consumer_| based on
  // the state we have gathered at the time of call.  If a decision
  // can't be made, defers until the next time this is called.
  // When a decision is made, will call back to |consumer_| on the UI thread.
  //
  // Must be called on the IO thread.
  void Resolve();

 private:
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

  // Used for testing.
  void set_attempt_state(TestAttemptState* new_state) {  // takes ownership.
    current_state_.reset(new_state);
  }

  // Resets |current_state_| and then posts a task to the UI thread to
  // Initiate() |to_initiate|.
  // Call this method on the IO thread.
  void ResyncRecoverHelper(CryptohomeOp* to_initiate);

  // If we don't have the system salt yet, loads it from the CryptohomeLibrary.
  void LoadSystemSalt();

  // If we haven't already, looks in a file called |filename| next to
  // the browser executable for a "localaccount" name, and retrieves it
  // if one is present.  If someone attempts to authenticate with this
  // username, we will mount a tmpfs for them and let them use the
  // browser.
  // Should only be called on the FILE thread.
  void LoadLocalaccount(const std::string& filename);

  void SetLocalaccount(const std::string& new_name);

  // Stores a hash of |password|, salted with the ascii of |system_salt_|.
  std::string HashPassword(const std::string& password);

  // Returns the ascii encoding of the system salt.
  std::string SaltAsAscii();

  // Converts the binary data |binary| into an ascii hex string and stores
  // it in |hex_string|.  Not guaranteed to be NULL-terminated.
  // Returns false if |hex_string| is too small, true otherwise.
  static bool BinaryToHex(const std::vector<unsigned char>& binary,
                          const unsigned int binary_len,
                          char* hex_string,
                          const unsigned int len);

  // Name of a file, next to chrome, that contains a local account username.
  static const char kLocalaccountFile[];

  // Milliseconds until we timeout our attempt to hit ClientLogin.
  static const int kClientLoginTimeoutMs;

  // Milliseconds until we re-check whether we've gotten the localaccount name.
  static const int kLocalaccountRetryIntervalMs;

  // Handles all net communications with Gaia.
  scoped_ptr<GaiaAuthFetcher> gaia_authenticator_;

  // Used when we need to try online authentication again, after successful
  // mount, but failed online login.
  scoped_ptr<AuthAttemptState> reauth_state_;

  scoped_ptr<AuthAttemptState> current_state_;
  scoped_refptr<OnlineAttempt> current_online_;
  scoped_refptr<CryptohomeOp> mounter_;
  scoped_refptr<CryptohomeOp> key_migrator_;
  scoped_refptr<CryptohomeOp> data_remover_;
  scoped_refptr<CryptohomeOp> guest_mounter_;
  scoped_refptr<CryptohomeOp> key_checker_;

  std::string ascii_hash_;
  chromeos::CryptohomeBlob system_salt_;

  // When the user has changed her password, but gives us the old one, we will
  // be able to mount her cryptohome, but online authentication will fail.
  // This allows us to present the same behavior to the caller, regardless
  // of the order in which we receive these results.
  bool already_reported_success_;
  base::Lock success_lock_;  // A lock around already_reported_success_.

  // Status relating to the local "backdoor" account.
  std::string localaccount_;
  bool checked_for_localaccount_;  // Needed because empty localaccount_ is ok.
  base::Lock localaccount_lock_;  // A lock around checked_for_localaccount_.

  friend class ResolveChecker;
  friend class ParallelAuthenticatorTest;
  FRIEND_TEST_ALL_PREFIXES(ParallelAuthenticatorTest, SaltToAscii);
  FRIEND_TEST_ALL_PREFIXES(ParallelAuthenticatorTest, ReadLocalaccount);
  FRIEND_TEST_ALL_PREFIXES(ParallelAuthenticatorTest,
                           ReadLocalaccountTrailingWS);
  FRIEND_TEST_ALL_PREFIXES(ParallelAuthenticatorTest, ReadNoLocalaccount);
  DISALLOW_COPY_AND_ASSIGN(ParallelAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PARALLEL_AUTHENTICATOR_H_
