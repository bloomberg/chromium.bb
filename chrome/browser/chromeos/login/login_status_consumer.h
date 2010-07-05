// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_

#include <string>
#include "chrome/common/net/gaia/gaia_auth_consumer.h"

namespace chromeos {

// An interface that defines the callbacks for objects that the
// Authenticator class will call to report the success/failure of
// authentication for Chromium OS.
class LoginStatusConsumer {
 public:
  virtual ~LoginStatusConsumer() {}
  virtual void OnLoginFailure(const std::string& error) = 0;
  virtual void OnLoginSuccess(const std::string& username,
      const GaiaAuthConsumer::ClientLoginResult& credentials) = 0;
  virtual void OnOffTheRecordLoginSuccess() {}
  virtual void OnPasswordChangeDetected(
      const GaiaAuthConsumer::ClientLoginResult& credentials) {
    NOTREACHED();
  };
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_
