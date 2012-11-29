// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/wm/cursor_manager.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace chromeos {

class LoginTestBase : public CrosInProcessBrowserTest {
 public:
  LoginTestBase()
    : mock_cryptohome_library_(NULL),
      mock_network_library_(NULL) {
  }

 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
    cros_mock_->InitMockCryptohomeLibrary();
    mock_cryptohome_library_ = cros_mock_->mock_cryptohome_library();
    mock_network_library_ = cros_mock_->mock_network_library();
    EXPECT_CALL(*mock_cryptohome_library_, GetSystemSalt())
        .WillRepeatedly(Return(std::string("stub_system_salt")));
    EXPECT_CALL(*mock_cryptohome_library_, InstallAttributesIsReady())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_network_library_, AddUserActionObserver(_))
        .Times(AnyNumber());
    EXPECT_CALL(*mock_network_library_, LoadOncNetworks(_, _, _, _, _))
        .WillRepeatedly(Return(true));
  }

  MockCryptohomeLibrary* mock_cryptohome_library_;
  MockNetworkLibrary* mock_network_library_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginTestBase);
};

class LoginUserTest : public LoginTestBase {
 protected:
  virtual void SetUpInProcessBrowserTestFixture() {
    LoginTestBase::SetUpInProcessBrowserTestFixture();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchASCII(switches::kLoginUser, "TestUser@gmail.com");
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kNoFirstRun);
  }
};

class LoginGuestTest : public LoginTestBase {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kGuestSession);
    command_line->AppendSwitch(switches::kIncognito);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kNoFirstRun);
  }
};

class LoginCursorTest : public LoginTestBase {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kLoginManager);
  }
};

// After a chrome crash, the session manager will restart chrome with
// the -login-user flag indicating that the user is already logged in.
// This profile should NOT be an OTR profile.
IN_PROC_BROWSER_TEST_F(LoginUserTest, UserPassed) {
  Profile* profile = browser()->profile();
  EXPECT_EQ("user", profile->GetPath().BaseName().value());
  EXPECT_FALSE(profile->IsOffTheRecord());
}

// Verifies the cursor is not hidden at startup when user is logged in.
IN_PROC_BROWSER_TEST_F(LoginUserTest, CursorShown) {
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->cursor_visible());
}

// After a guest login, we should get the OTR default profile.
IN_PROC_BROWSER_TEST_F(LoginGuestTest, GuestIsOTR) {
  Profile* profile = browser()->profile();
  EXPECT_EQ("Default", profile->GetPath().BaseName().value());
  EXPECT_TRUE(profile->IsOffTheRecord());
  // Ensure there's extension service for this profile.
  EXPECT_TRUE(profile->GetExtensionService());
}

// Verifies the cursor is not hidden at startup when running guest session.
IN_PROC_BROWSER_TEST_F(LoginGuestTest, CursorShown) {
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->cursor_visible());
}

// Verifies the cursor is hidden at startup on login screen.
IN_PROC_BROWSER_TEST_F(LoginCursorTest, CursorHidden) {
  // Login screen needs to be shown explicitly when running test.
  ShowLoginWizard(WizardController::kLoginScreenName, gfx::Size());

  // Cursor should be hidden at startup
  EXPECT_FALSE(ash::Shell::GetInstance()->cursor_manager()->cursor_visible());

  // Cursor should be shown after cursor is moved.
  EXPECT_TRUE(ui_test_utils::SendMouseMoveSync(gfx::Point()));
  EXPECT_TRUE(ash::Shell::GetInstance()->cursor_manager()->cursor_visible());

  MessageLoop::current()->DeleteSoon(FROM_HERE,
                                     BaseLoginDisplayHost::default_host());
}

} // namespace chromeos
