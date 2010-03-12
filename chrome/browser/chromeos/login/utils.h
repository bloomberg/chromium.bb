// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UTILS_H_

#include <string>

class Authenticator;
class LoginStatusConsumer;

namespace views {
class Widget;
}

namespace chromeos {

namespace login_utils {

// Invoked after the user has successfully logged in. This launches a browser
// and does other bookkeeping after logging in.
void CompleteLogin(const std::string& username);

// Creates and returns the authenticator to use. The caller owns the returned
// Authenticator and must delete it when done.
Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer);

}  // namespace login_utils

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UTILS_H_
