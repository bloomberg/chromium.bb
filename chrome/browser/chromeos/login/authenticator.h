// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_

#include <vector>

#include "base/logging.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"

// An interface for objects that will authenticate a Chromium OS user.
// When authentication successfully completes, will call
// consumer_->OnLoginSuccess(|username|).
// On failure, will call consumer_->OnLoginFailure().

class Authenticator {
 public:
  explicit Authenticator(LoginStatusConsumer* consumer)
      : consumer_(consumer) {
  }
  virtual ~Authenticator() {}

  // Given a |username| and |password|, this method attempts to authenticate
  // Returns true if we kick off the attempt successfully and false if we can't.
  virtual bool Authenticate(const std::string& username,
                            const std::string& password) = 0;

 protected:
  LoginStatusConsumer* consumer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

class StubAuthenticator : public Authenticator {
 public:
  explicit StubAuthenticator(LoginStatusConsumer* consumer)
      : Authenticator(consumer) {
  }
  virtual ~StubAuthenticator() {}

  // Returns true after calling OnLoginSuccess().
  virtual bool Authenticate(const std::string& username,
                            const std::string& password) {
    consumer_->OnLoginSuccess(username, std::vector<std::string>());
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubAuthenticator);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_AUTHENTICATOR_H_
