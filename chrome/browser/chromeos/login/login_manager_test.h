// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_

#include "chrome/browser/chromeos/login/mock_login_utils.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace chromeos {

class LoginManagerTest : public InProcessBrowserTest {
 public:
  explicit LoginManagerTest(bool should_launch_browser);

  // Overriden from InProcessBrowserTest.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

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

  MockLoginUtils& login_utils() { return *mock_login_utils_; }

 private:
  MockLoginUtils* mock_login_utils_;
  bool should_launch_browser_;

  DISALLOW_COPY_AND_ASSIGN(LoginManagerTest);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_MANAGER_TEST_H_
