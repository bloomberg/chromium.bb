// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"

// Authenticates a Chromium OS user against the Google Accounts ClientLogin API.

class Profile;
class GoogleServiceAuthError;
class LoginFailure;

namespace base {
class Lock;
}

namespace chromeos {

class GoogleAuthenticatorTest;
class LoginStatusConsumer;
class UserManager;

class GoogleAuthenticator : public Authenticator, public GaiaAuthConsumer {
 public:
  explicit GoogleAuthenticator(LoginStatusConsumer* consumer);
  virtual ~GoogleAuthenticator();

  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.  The ultimate result is either a callback to
  // consumer_->OnLoginSuccess() with the |username| and a vector of
  // authentication cookies or a callback to consumer_->OnLoginFailure() with
  // an error message.  Uses |profile| when doing URL fetches.
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

  // Public for testing.
  void set_system_salt(const chromeos::CryptohomeBlob& new_salt) {
    system_salt_ = new_salt;
  }
  void set_username(const std::string& fake_user) { username_ = fake_user; }
  void set_password(const std::string& fake_pass) { password_ = fake_pass; }
  void set_password_hash(const std::string& fake_hash) {
    ascii_hash_ = fake_hash;
  }
  void set_user_manager(UserManager* new_manager) {
    user_manager_ = new_manager;
  }
  void SetLocalaccount(const std::string& new_name);

  // These methods must be called on the UI thread, as they make DBus calls
  // and also call back to the login UI.
  void OnLoginSuccess(const GaiaAuthConsumer::ClientLoginResult& credentials,
                      bool request_pending);
  void CheckOffline(const LoginFailure& error);
  void CheckLocalaccount(const LoginFailure& error);
  void OnLoginFailure(const LoginFailure& error);

  // Call these methods on the UI thread.
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

  // Callbacks from GaiaAuthFetcher
  virtual void OnClientLoginFailure(
      const GoogleServiceAuthError& error);
  virtual void OnClientLoginSuccess(
      const GaiaAuthConsumer::ClientLoginResult& credentials);

 private:

  // If we don't have the system salt yet, loads it from the CryptohomeLibrary.
  void LoadSystemSalt();

  // If we haven't already, looks in a file called |filename| next to
  // the browser executable for a "localaccount" name, and retrieves it
  // if one is present.  If someone attempts to authenticate with this
  // username, we will mount a tmpfs for them and let them use the
  // browser.
  // Should only be called on the FILE thread.
  void LoadLocalaccount(const std::string& filename);

  // Stores a hash of |password|, salted with the ascii of |system_salt_|.
  std::string HashPassword(const std::string& password);

  // Returns the ascii encoding of the system salt.
  std::string SaltAsAscii();

  // Save the current login attempt for use on the next TryClientLogin
  // attempt.
  void PrepareClientLoginAttempt(const std::string& password,
                                 const std::string& login_token,
                                 const std::string& login_captcha);
  // Clear any cached credentials after we've given up trying to authenticate.
  void ClearClientLoginAttempt();

  // Start a client login attempt.  |hosted_policy_| governs whether we are
  // willing to authenticate accounts that are HOSTED or not.
  // You must set up |gaia_authenticator_| first.
  // Reuses existing credentials from the last attempt. You must
  // PrepareClientLoginAttempt before calling this.
   void TryClientLogin();

  // A callback for use on the UI thread. Cancel the current login
  // attempt, and produce a login failure.
  void CancelClientLogin();


  // Converts the binary data |binary| into an ascii hex string and stores
  // it in |hex_string|.  Not guaranteed to be NULL-terminated.
  // Returns false if |hex_string| is too small, true otherwise.
  static bool BinaryToHex(const std::vector<unsigned char>& binary,
                          const unsigned int binary_len,
                          char* hex_string,
                          const unsigned int len);

  void set_hosted_policy(GaiaAuthFetcher::HostedAccountsSetting policy) {
    hosted_policy_ = policy;
  }

  // The format of said POST body when CAPTCHA token & answer are specified.
  static const char kFormatCaptcha[];

  // Magic string indicating that, while a second factor is still
  // needed to complete authentication, the user provided the right password.
  static const char kSecondFactor[];

  // Name of a file, next to chrome, that contains a local account username.
  static const char kLocalaccountFile[];

  // Handles all net communications with Gaia.
  scoped_ptr<GaiaAuthFetcher> gaia_authenticator_;

  // Allows us to look up users of the device.
  UserManager* user_manager_;

  // Milliseconds until we timeout our attempt to hit ClientLogin.
  static const int kClientLoginTimeoutMs;

  // Milliseconds until we re-check whether we've gotten the localaccount name.
  static const int kLocalaccountRetryIntervalMs;

  // Whether or not we're accepting HOSTED accounts on this auth attempt.
  GaiaAuthFetcher::HostedAccountsSetting hosted_policy_;

  std::string username_;
  // These fields are saved so we can retry client login.
  std::string password_;
  std::string login_token_;
  std::string login_captcha_;

  std::string ascii_hash_;
  chromeos::CryptohomeBlob system_salt_;
  bool unlock_;  // True if authenticating to unlock the computer.
  bool try_again_;  // True if we're willing to retry the login attempt.

  std::string localaccount_;
  bool checked_for_localaccount_;  // Needed because empty localaccount_ is ok.
  base::Lock localaccount_lock_;  // A lock around checked_for_localaccount_.

  friend class GoogleAuthenticatorTest;
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, SaltToAscii);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, CheckTwoFactorResponse);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, CheckNormalErrorCode);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, EmailAddressNoOp);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, EmailAddressIgnoreCaps);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest,
                           EmailAddressIgnoreDomainCaps);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest,
                           EmailAddressIgnoreOneUsernameDot);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest,
                           EmailAddressIgnoreManyUsernameDots);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest,
                           EmailAddressIgnoreConsecutiveUsernameDots);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest,
                           EmailAddressDifferentOnesRejected);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest,
                           EmailAddressIgnorePlusSuffix);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest,
                           EmailAddressIgnoreMultiPlusSuffix);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, ReadSaltOnlyOnce);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, LocalaccountLogin);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, ReadLocalaccount);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, ReadLocalaccountTrailingWS);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, ReadNoLocalaccount);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, LoginNetFailure);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, LoginDenied);
  FRIEND_TEST_ALL_PREFIXES(GoogleAuthenticatorTest, TwoFactorLogin);

  DISALLOW_COPY_AND_ASSIGN(GoogleAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
