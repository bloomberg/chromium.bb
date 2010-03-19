// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_

#include <string>

// An interface that defines the callbacks for objects that the
// Authenticator class will call to report the success/failure of
// authentication for Chromium OS.
class LoginStatusConsumer {
 public:
  virtual ~LoginStatusConsumer() {}
  // These copy data in, so as to avoid potential object lifetime problems.
  virtual void OnLoginFailure(const std::string error) = 0;
  virtual void OnLoginSuccess(const std::string username,
                              std::vector<std::string> cookies) = 0;
};



#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_STATUS_CONSUMER_H_
