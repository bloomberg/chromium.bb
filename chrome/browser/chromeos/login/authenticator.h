// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class Profile;

namespace chromeos {

// An interface for objects that will authenticate a Chromium OS user.
// When authentication successfully completes, will call
// consumer_->OnLoginSuccess() on the UI thread.
// On failure, will call consumer_->OnLoginFailure() on the UI thread.
// On password change detected, will call
// consumer_->OnPasswordChangeDetected() on the UI thread.
class Authenticator : public base::RefCountedThreadSafe<Authenticator> {
 public:
  explicit Authenticator(LoginStatusConsumer* consumer);

  // Given externally authenticated |username| and |password|, this method
  // attempts to complete authentication process.
  virtual void CompleteLogin(Profile* profile,
                             const std::string& username,
                             const std::string& password) = 0;

  // Given a |username| and |password|, this method attempts to authenticate
  // to login.
  // Optionally |login_token| and |login_captcha| could be provided.
  // Must be called on the UI thread.
  virtual void AuthenticateToLogin(Profile* profile,
                                   const std::string& username,
                                   const std::string& password,
                                   const std::string& login_token,
                                   const std::string& login_captcha) = 0;

  // Given a |username| and |password|, this method attempts to
  // authenticate to unlock the computer.
  // Must be called on the UI thread.
  virtual void AuthenticateToUnlock(const std::string& username,
                                    const std::string& password) = 0;

  // Initiates demo user login.
  virtual void LoginDemoUser() = 0;

  // Initiates incognito ("browse without signing in") login.
  virtual void LoginOffTheRecord() = 0;

  // Initiates a demo user login.
  virtual void OnDemoUserLoginSuccess() = 0;

  // |request_pending| is true if we still plan to call consumer_ with the
  // results of more requests.
  // Must be called on the UI thread.
  virtual void OnLoginSuccess(bool request_pending) = 0;

  // Must be called on the UI thread.
  virtual void OnLoginFailure(const LoginFailure& error) = 0;

  // Call these methods on the UI thread.
  // If a password logs the user in online, but cannot be used to
  // mount his cryptohome, we expect that a password change has
  // occurred.
  // Call this method to migrate the user's encrypted data
  // forward to use his new password.  |old_password| is the password
  // his data was last encrypted with.
  virtual void RecoverEncryptedData(
      const std::string& old_password) = 0;

  // Call this method to erase the user's encrypted data
  // and create a new cryptohome.
  virtual void ResyncEncryptedData() = 0;

  // Attempt to authenticate online again.
  virtual void RetryAuth(Profile* profile,
                         const std::string& username,
                         const std::string& password,
                         const std::string& login_token,
                         const std::string& login_captcha) = 0;

  // Profile (usually off the record ) that was used to perform the last
  // authentication process.
  Profile* authentication_profile() { return authentication_profile_; }

 protected:
  virtual ~Authenticator();

  LoginStatusConsumer* consumer_;
  Profile* authentication_profile_;

 private:
  friend class base::RefCountedThreadSafe<Authenticator>;

  DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
