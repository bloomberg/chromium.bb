// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_PAM_GOOGLE_AUTHENTICATOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_PAM_GOOGLE_AUTHENTICATOR_H_

#include <string>
#include "chrome/browser/chromeos/login/authenticator.h"

class LoginStatusConsumer;

// Authenticates a Chromium OS user against the Google Accounts ClientLogin API.

class PamGoogleAuthenticator : public Authenticator {
 public:
  explicit PamGoogleAuthenticator(LoginStatusConsumer* consumer)
      : Authenticator(consumer) {
  }
  virtual ~PamGoogleAuthenticator() {}

  // Given a |username| and |password|, this method attempts to authenticate to
  // the Google accounts servers.
  // Returns true if the attempt gets sent successfully and false if not.
  bool Authenticate(const std::string& username,
                    const std::string& password);

 private:
  DISALLOW_COPY_AND_ASSIGN(PamGoogleAuthenticator);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_PAM_GOOGLE_AUTHENTICATOR_H_
