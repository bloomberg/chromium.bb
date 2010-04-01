// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_

#include <string>
#include <vector>

class Profile;

namespace chromeos {

class Authenticator;
class LoginStatusConsumer;

class LoginUtils {
 public:
  // Get LoginUtils singleton object. If it was not set before, new default
  // instance will be created.
  static LoginUtils* Get();

  // Set LoginUtils singleton object for test purpose only!
  static void Set(LoginUtils* ptr);

  // Thin wrapper around BrowserInit::LaunchBrowser().  Meant to be used in a
  // Task posted to the UI thread.
  static void DoBrowserLaunch(Profile* profile);

  virtual ~LoginUtils() {}

  // Invoked after the user has successfully logged in. This launches a browser
  // and does other bookkeeping after logging in.
  virtual void CompleteLogin(const std::string& username,
                             const std::string& credentials) = 0;

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(LoginStatusConsumer* consumer) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_UTILS_H_
