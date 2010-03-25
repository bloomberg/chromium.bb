// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;

class NetworkScreenTest : public CrosInProcessBrowserTest {
 public:
  NetworkScreenTest() {
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitchWithValue(switches::kLoginScreen, "network");
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    CrosInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();

    mock_login_library_ = new MockLoginLibrary();
    test_api->SetLoginLibrary(mock_login_library_);
    EXPECT_CALL(*mock_login_library_, EmitLoginPromptReady())
        .Times(1);

    EXPECT_CALL(*mock_network_library_, ethernet_connecting())
      .Times(AnyNumber())
      .WillRepeatedly((Return(false)));
    EXPECT_CALL(*mock_network_library_, wifi_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(wifi_networks_)));
    EXPECT_CALL(*mock_network_library_, cellular_networks())
      .Times(AnyNumber())
      .WillRepeatedly((ReturnRef(cellular_networks_)));
  }

  virtual void TearDownInProcessBrowserTestFixture() {
    CrosInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api-> SetLoginLibrary(NULL);
  }

  virtual Browser* CreateBrowser(Profile* profile) {
    return NULL;
  }

  MockLoginLibrary* mock_login_library_;

  CellularNetworkVector cellular_networks_;
  WifiNetworkVector wifi_networks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkScreenTest);
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, TestInit) {
  WizardController* controller = WizardController::default_controller();
  ASSERT_TRUE(controller != NULL);
  ASSERT_EQ(controller->GetNetworkScreen(), controller->current_screen());
  // Close login manager windows.
  MessageLoop::current()->DeleteSoon(FROM_HERE, controller);
  // End the message loop to quit the test.
  MessageLoop::current()->Quit();
}

}  // namespace chromeos
