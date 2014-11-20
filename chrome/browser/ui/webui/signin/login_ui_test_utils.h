// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_

#include <string>

class Browser;

namespace login_ui_test_utils {

// Blocks until the login UI is available and ready for authorization.
void WaitUntilUIReady(Browser* browser);

// Executes JavaScript code to sign in a user with email and password to the
// auth iframe hosted by gaia_auth extension.
void ExecuteJsToSigninInSigninFrame(Browser* browser,
                                    const std::string& email,
                                    const std::string& password);

// A function to sign in a user using Chrome sign-in UI interface.
// This will block until a signin succeeded or failed notification is observed.
bool SignInWithUI(Browser* browser,
                  const std::string& email,
                  const std::string& password);

}  // namespace login_ui_test_utils

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
