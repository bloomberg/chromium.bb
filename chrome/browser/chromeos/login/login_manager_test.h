// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_

#include "chrome/browser/chromeos/login/mock_login_utils.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromeos {

// Base class for Chrome OS out-of-box/login WebUI tests.
// If no special configuration is done launches out-of-box WebUI.
// To launch login UI use PRE_* test that will register user(s) and mark
// out-of-box as completed.
// Guarantees that WebUI has been initialized by waiting for
// NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE notification.
class LoginManagerTest : public InProcessBrowserTest {
 public:
  explicit LoginManagerTest(bool should_launch_browser);

  // Overriden from InProcessBrowserTest.
  virtual void CleanUpOnMainThread() OVERRIDE;
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  // Registers user with given |username| on device.
  // Should be called in PRE_* test.
  // TODO(dzhioev): add ability to register users without PRE_* test.
  void RegisterUser(const std::string& username);

  // Set expected credentials for next login attempt.
  void SetExpectedCredentials(const std::string& username,
                              const std::string& password);

  // Tries to login with |username| and |password|. Returns false if attempt
  // has failed.
  bool TryToLogin(const std::string& username, const std::string& password);

  // Login user with |username|. User should be registered using RegisterUser().
  void LoginUser(const std::string& username);

  // Executes given JS |expression| in |web_contents_| and checks
  // that it is true.
  void JSExpect(const std::string& expression);

  MockLoginUtils& login_utils() { return *mock_login_utils_; }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  void InitializeWebContents();

  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }

  MockLoginUtils* mock_login_utils_;
  bool should_launch_browser_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
