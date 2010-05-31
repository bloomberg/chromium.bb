// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/sha2.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/common/net/url_fetcher.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

// Authenticates a Chromium OS user against the Google Accounts ClientLogin API.

class Profile;

namespace chromeos {

class GoogleAuthenticatorTest;
class LoginStatusConsumer;

class GoogleAuthenticator : public Authenticator,
                            public URLFetcher::Delegate {
 public:
  explicit GoogleAuthenticator(LoginStatusConsumer* consumer);
  virtual ~GoogleAuthenticator();

  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.  The ultimate result is either a callback to
  // consumer_->OnLoginSuccess() with the |username| and a vector of
  // authentication cookies or a callback to consumer_->OnLoginFailure() with
  // an error message.  Uses |profile| when doing URL fetches.
  // Should be called on the FILE thread!
  //
  // Returns true if the attempt gets sent successfully and false if not.
  bool AuthenticateToLogin(Profile* profile,
                           const std::string& username,
                           const std::string& password);

  // Given a |username| and |password|, this method attempts to
  // authenticate to the cached credentials. This will never contact
  // the server even if it's online. The auth result is sent to
  // LoginStatusConsumer in a same way as AuthenticateToLogin does.
  bool AuthenticateToUnlock(const std::string& username,
                            const std::string& password);

  // Overridden from URLFetcher::Delegate
  // When the authentication attempt comes back, will call
  // consumer_->OnLoginSuccess(|username|).
  // On any failure, will call consumer_->OnLoginFailure().
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);



  // Public for testing.
  void set_system_salt(const std::vector<unsigned char>& new_salt) {
    system_salt_ = new_salt;
  }
  void set_localaccount(const std::string& new_name) {
    localaccount_ = new_name;
    checked_for_localaccount_ = true;
  }
  void set_username(const std::string& fake_user) { username_ = fake_user; }
  void set_password_hash(const std::string& fake_hash) {
    ascii_hash_ = fake_hash;
  }

  // These methods must be called on the UI thread, as they make DBus calls
  // and also call back to the login UI.
  void OnLoginSuccess(const std::string& credentials);
  void CheckOffline(const std::string& error);
  void CheckLocalaccount(const std::string& error);
  void OnLoginFailure(const std::string& data);

  // Perform basic canonicalization of |email_address|, taking into account
  // that gmail does not consider '.' or caps inside a username to matter.
  // For example, c.masone@gmail.com == cMaSone@gmail.com, per
  // http://mail.google.com/support/bin/answer.py?hl=en&ctx=mail&answer=10313#
  static std::string Canonicalize(const std::string& email_address);

  // The signal to cryptohomed that we want a tmpfs.
  // TODO(cmasone): revisit this after cryptohome re-impl
  static const char kTmpfsTrigger[];

 private:
  // If we don't have the system salt yet, loads it from |path|.
  // Should only be called on the FILE thread.
  void LoadSystemSalt(const FilePath& path);

  // If we haven't already, looks in a file called |filename| next to
  // the browser executable for a "localaccount" name, and retrieves it
  // if one is present.  If someone attempts to authenticate with this
  // username, we will mount a tmpfs for them and let them use the
  // browser.
  // Should only be called on the FILE thread.
  void LoadLocalaccount(const std::string& filename);

  // Stores a hash of |password|, salted with the ascii of |system_salt_|.
  void StoreHashedPassword(const std::string& password);

  // Returns the ascii encoding of the system salt.
  std::string SaltAsAscii();

  // Kicks off a new ClientLogin attempt, canceling any existing attempts.
  // It is an error to call this before populating |getter_| and |request_body_|
  void TryClientLogin();

  // To support internal two-factor authentication, check to see if
  // |alleged_error| is actually indicating the successful completion
  // of the first half of a two-factor login.
  static bool IsSecondFactorSuccess(const std::string& alleged_error);

  // Converts the binary data |binary| into an ascii hex string and stores
  // it in |hex_string|.  Not guaranteed to be NULL-terminated.
  // Returns false if |hex_string| is too small, true otherwise.
  static bool BinaryToHex(const std::vector<unsigned char>& binary,
                          const unsigned int binary_len,
                          char* hex_string,
                          const unsigned int len);

  // Create and return a URLFetcher that is set up to perform a ClientLogin
  // authentication attempt.  Caller takes ownership.
  static URLFetcher* CreateClientLoginFetcher(URLRequestContextGetter* getter,
                                              const std::string& body,
                                              URLFetcher::Delegate* delegate);

  // Constants to use in the ClientLogin request POST body.
  static const char kCookiePersistence[];
  static const char kAccountType[];
  static const char kSource[];
  static const char kService[];

  // The format of said POST body.
  static const char kFormat[];

  // Magic string indicating that, while a second factor is still
  // needed to complete authentication, the user provided the right password.
  static const char kSecondFactor[];

  // Chromium OS system salt stored here.
  static const char kSystemSalt[];
  // String that appears at the start of OpenSSL cipher text with embedded salt.
  static const char kOpenSSLMagic[];

  // Name of a file, next to chrome, that contains a local account username.
  static const char kLocalaccountFile[];

  URLFetcher* fetcher_;
  URLRequestContextGetter* getter_;
  std::string username_;
  std::string ascii_hash_;
  std::string request_body_;
  std::vector<unsigned char> system_salt_;
  std::string localaccount_;
  bool checked_for_localaccount_;  // needed becasuse empty localaccount_ is ok.
  bool unlock_;  // True if authenticating to unlock the computer.
  bool try_again_;  // True if we're willing to retry the login attempt.

  friend class GoogleAuthenticatorTest;
  FRIEND_TEST(GoogleAuthenticatorTest, SaltToAsciiTest);
  FRIEND_TEST(GoogleAuthenticatorTest, CheckTwoFactorResponse);
  FRIEND_TEST(GoogleAuthenticatorTest, CheckNormalErrorCode);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressNoOp);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressIgnoreCaps);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressIgnoreDomainCaps);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressIgnoreOneUsernameDot);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressIgnoreManyUsernameDots);
  FRIEND_TEST(GoogleAuthenticatorTest,
              EmailAddressIgnoreConsecutiveUsernameDots);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressDifferentOnesRejected);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressIgnorePlusSuffix);
  FRIEND_TEST(GoogleAuthenticatorTest, EmailAddressIgnoreMultiPlusSuffix);
  FRIEND_TEST(GoogleAuthenticatorTest, ReadSaltTest);
  FRIEND_TEST(GoogleAuthenticatorTest, ReadLocalaccountTest);
  FRIEND_TEST(GoogleAuthenticatorTest, ReadLocalaccountTrailingWSTest);
  FRIEND_TEST(GoogleAuthenticatorTest, ReadNoLocalaccountTest);
  FRIEND_TEST(GoogleAuthenticatorTest, LoginNetFailureTest);
  FRIEND_TEST(GoogleAuthenticatorTest, LoginDeniedTest);
  FRIEND_TEST(GoogleAuthenticatorTest, TwoFactorLoginTest);

  DISALLOW_COPY_AND_ASSIGN(GoogleAuthenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
