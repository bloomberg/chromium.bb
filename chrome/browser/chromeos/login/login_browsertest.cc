// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/time.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_input_method_library.h"
#include "chrome/browser/chromeos/cros/mock_keyboard_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/mock_power_library.h"
#include "chrome/browser/chromeos/cros/mock_screen_lock_library.h"
#include "chrome/browser/chromeos/cros/mock_synaptics_library.h"
#include "chrome/browser/chromeos/cros/mock_system_library.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::NiceMock;

class LoginTestBase : public InProcessBrowserTest {
 public:
  LoginTestBase() {
    testApi_ = chromeos::CrosLibrary::Get()->GetTestApi();
    testApi_->SetLibraryLoader(&loader_, false);
    EXPECT_CALL(loader_, Load(_))
        .WillRepeatedly(Return(true));

    testApi_->SetKeyboardLibrary(&mock_keyboard_library_, false);
    testApi_->SetInputMethodLibrary(&mock_input_method_library_, false);
    EXPECT_CALL(mock_input_method_library_, GetActiveInputMethods())
        .WillRepeatedly(
            InvokeWithoutArgs(CreateFallbackInputMethodDescriptors));
    EXPECT_CALL(mock_input_method_library_, current_ime_properties())
        .WillOnce((ReturnRef(ime_properties_)));

    testApi_->SetNetworkLibrary(&mock_network_library_, false);

    testApi_->SetPowerLibrary(&mock_power_library_, false);
    EXPECT_CALL(mock_power_library_, battery_time_to_empty())
        .WillRepeatedly((Return(base::TimeDelta::FromMinutes(42))));
    EXPECT_CALL(mock_power_library_, battery_time_to_full())
        .WillRepeatedly((Return(base::TimeDelta::FromMinutes(24))));

    testApi_->SetSynapticsLibrary(&mock_synaptics_library_, false);
    testApi_->SetCryptohomeLibrary(&mock_cryptohome_library_, false);
    testApi_->SetScreenLockLibrary(&mock_screen_lock_library_, false);
    testApi_->SetSystemLibrary(&mock_system_library_, false);
  }

 protected:
  NiceMock<MockLibraryLoader> loader_;
  NiceMock<MockCryptohomeLibrary> mock_cryptohome_library_;
  NiceMock<MockKeyboardLibrary> mock_keyboard_library_;
  NiceMock<MockInputMethodLibrary> mock_input_method_library_;
  NiceMock<MockNetworkLibrary> mock_network_library_;
  NiceMock<MockPowerLibrary> mock_power_library_;
  NiceMock<MockScreenLockLibrary> mock_screen_lock_library_;
  NiceMock<MockSynapticsLibrary> mock_synaptics_library_;
  NiceMock<MockSystemLibrary> mock_system_library_;
  ImePropertyList ime_properties_;
  chromeos::CrosLibrary::TestApi* testApi_;
};

class LoginUserTest : public LoginTestBase {
 public:
  LoginUserTest() {
    EXPECT_CALL(mock_cryptohome_library_, IsMounted())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_screen_lock_library_, AddObserver(_))
        .WillOnce(Return());
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchASCII(switches::kLoginUser, "TestUser@gmail.com");
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kNoFirstRun);
  }
};

class LoginProfileTest : public LoginTestBase {
 public:
  LoginProfileTest() {
    EXPECT_CALL(mock_cryptohome_library_, IsMounted())
        .WillRepeatedly(Return(true));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kNoFirstRun);
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

// On initial launch, we should get the OTR default profile.
IN_PROC_BROWSER_TEST_F(LoginProfileTest, UserNotPassed) {
  Profile* profile = browser()->profile();
  EXPECT_EQ("Default", profile->GetPath().BaseName().value());
  EXPECT_TRUE(profile->IsOffTheRecord());
  // Ensure there's no extension service for this profile.
  EXPECT_EQ(NULL, profile->GetExtensionsService());
}

} // namespace chromeos
