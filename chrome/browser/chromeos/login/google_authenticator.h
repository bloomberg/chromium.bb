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
#include "base/scoped_ptr.h"
#include "base/sha2.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/auth_response_handler.h"
#include "chrome/browser/net/url_fetcher.h"

class LoginStatusConsumer;

// Authenticates a Chromium OS user against the Google Accounts ClientLogin API.

class GoogleAuthenticator : public Authenticator,
                            public URLFetcher::Delegate {
 public:
  GoogleAuthenticator(LoginStatusConsumer* consumer,
                      AuthResponseHandler* cl_handler,
                      AuthResponseHandler* i_handler)
      : Authenticator(consumer),
        fetcher_(NULL),
        getter_(NULL),
        client_login_handler_(cl_handler),
        issue_handler_(i_handler) {
    CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
    library_ = chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();
  }

  explicit GoogleAuthenticator(LoginStatusConsumer* consumer)
      : Authenticator(consumer),
        fetcher_(NULL),
        getter_(NULL),
        client_login_handler_(NULL),
        issue_handler_(NULL) {
    CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
    library_ = chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();
  }

  virtual ~GoogleAuthenticator() {}

  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.  The ultimate result is either a callback to
  // consumer_->OnLoginSuccess() with the |username| and a vector of
  // authentication cookies or a callback to consumer_->OnLoginFailure() with
  // an error message.
  //
  // Returns true if the attempt gets sent successfully and false if not.
  bool Authenticate(const std::string& username,
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

  // Returns the ascii encoding of the system salt.
  std::string SaltAsAscii();

  // I need these static methods so I can PostTasks that use methods
  // of sublcasses of LoginStatusConsumer.  I can't seem to make
  // RunnableMethods out of methods belonging to subclasses without referring
  // to the subclasses specifically, and I want to allow mocked out
  // LoginStatusConsumers to be used here as well.
  static void OnLoginSuccess(LoginStatusConsumer* consumer,
                             const std::string& username,
                             const std::string& passhash,
                             const ResponseCookies& cookies);
  static void CheckOffline(LoginStatusConsumer* consumer,
                           const std::string& username,
                           const std::string& passhash,
                           const URLRequestStatus& status);
  static void OnLoginFailure(LoginStatusConsumer* consumer,
                             const std::string& data);

  // Meant for testing.
  void set_system_salt(const std::vector<unsigned char>& fake_salt) {
    system_salt_ = fake_salt;
  }
  void set_username(const std::string& fake_user) { username_ = fake_user; }
  void set_password_hash(const std::string& fake_hash) {
    ascii_hash_ = fake_hash;
  }

 private:
  static const char kCookiePersistence[];
  static const char kAccountType[];
  static const char kSource[];
  static const char kFormat[];
  static const int kHttpSuccess;
  static const char kSystemSalt[];
  static const char kOpenSSLMagic[];

  chromeos::CryptohomeLibrary* library_;
  scoped_ptr<URLFetcher> fetcher_;
  URLRequestContextGetter* getter_;
  scoped_ptr<AuthResponseHandler> client_login_handler_;
  scoped_ptr<AuthResponseHandler> issue_handler_;
  std::vector<unsigned char> system_salt_;
  std::string username_;
  std::string ascii_hash_;

  // If we don't have the system salt yet, loads it from |path|.
  void LoadSystemSalt(FilePath& path);

  // Stores a hash of |password|, salted with the ascii of |system_salt_|.
  void StoreHashedPassword(const std::string& password);

  // Converts the binary data |binary| into an ascii hex string and stores
  // it in |hex_string|.  Not guaranteed to be NULL-terminated.
  // Returns false if |hex_string| is too small, true otherwise.
  static bool BinaryToHex(const std::vector<unsigned char>& binary,
                          const unsigned int binary_len,
                          char* hex_string,
                          const unsigned int len);

  DISALLOW_COPY_AND_ASSIGN(GoogleAuthenticator);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_GOOGLE_AUTHENTICATOR_H_
