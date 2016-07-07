// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_

#include <string>

#include "base/time/time.h"
#include "components/signin/core/browser/signin_metrics.h"

class Browser;

namespace login_ui_test_utils {

// Blocks until the login UI is available and ready for authorization.
void WaitUntilUIReady(Browser* browser);

// Blocks until an element with id |element_id| exists in the signin page.
void WaitUntilElementExistsInSigninFrame(Browser* browser,
                                         const std::string& element_id);

// Returns whether an element with id |element_id| exists in the signin page.
bool ElementExistsInSigninFrame(Browser* browser,
                                const std::string& element_id);

// Executes JavaScript code to sign in a user with email and password to the
// auth iframe hosted by gaia_auth extension. This function automatically
// detects the version of GAIA sign in page to use.
void ExecuteJsToSigninInSigninFrame(Browser* browser,
                                    const std::string& email,
                                    const std::string& password);

// Executes JS to sign in the user in the new GAIA sign in flow.
void SigninInNewGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password);

// Executes JS to sign in the user in the old GAIA sign in flow.
void SigninInOldGaiaFlow(Browser* browser,
                         const std::string& email,
                         const std::string& password);

// A function to sign in a user using Chrome sign-in UI interface.
// This will block until a signin succeeded or failed notification is observed.
// In case |wait_for_account_cookies|, the call will block until the account
// cookies have been written to the cookie jar.
// |access_point| identifies the access point used to load the signin page.
bool SignInWithUI(Browser* browser,
                  const std::string& email,
                  const std::string& password,
                  bool wait_for_account_cookies,
                  signin_metrics::AccessPoint access_point,
                  signin_metrics::Reason signin_reason);

// Most common way to sign in a user, it does not wait for cookies to be set
// and uses the SOURCE_START_PAGE as signin source.
bool SignInWithUI(Browser* browser,
                  const std::string& email,
                  const std::string& password);

// Waits for sync confirmation dialog to get displayed, then executes javascript
// to click on confirm button. Returns false if dialog wasn't dismissed before
// |timeout|.
bool DismissSyncConfirmationDialog(Browser* browser, base::TimeDelta timeout);

}  // namespace login_ui_test_utils

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_LOGIN_UI_TEST_UTILS_H_
