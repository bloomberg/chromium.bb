// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/network_screen.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_base_screen_delegate.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/views/controls/button/button.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;
using views::Button;

namespace chromeos {

class DummyButtonListener : public views::ButtonListener {
 public:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {}
};

namespace login {

class MockNetworkStateHelper : public NetworkStateHelper {
 public:
  MOCK_CONST_METHOD0(GetCurrentNetworkName, base::string16(void));
  MOCK_CONST_METHOD0(IsConnected, bool(void));
  MOCK_CONST_METHOD0(IsConnecting, bool(void));
};

}  // namespace login

class NetworkScreenTest : public WizardInProcessBrowserTest {
 public:
  NetworkScreenTest(): WizardInProcessBrowserTest("network"),
                       fake_session_manager_client_(NULL) {
  }

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    fake_session_manager_client_ = new FakeSessionManagerClient;
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));
  }

  void SetUpOnMainThread() override {
    WizardInProcessBrowserTest::SetUpOnMainThread();
    mock_base_screen_delegate_.reset(new MockBaseScreenDelegate());
    ASSERT_TRUE(WizardController::default_controller() != NULL);
    network_screen_ =
        NetworkScreen::Get(WizardController::default_controller());
    ASSERT_TRUE(network_screen_ != NULL);
    ASSERT_EQ(WizardController::default_controller()->current_screen(),
              network_screen_);
    network_screen_->base_screen_delegate_ = mock_base_screen_delegate_.get();
    ASSERT_TRUE(network_screen_->view_ != nullptr);

    mock_network_state_helper_ = new login::MockNetworkStateHelper;
    SetDefaultNetworkStateHelperExpectations();
    network_screen_->SetNetworkStateHelperForTest(mock_network_state_helper_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void EmulateContinueButtonExit(NetworkScreen* network_screen) {
    EXPECT_CALL(*mock_base_screen_delegate_,
                OnExit(_, BaseScreenDelegate::NETWORK_CONNECTED, _)).Times(1);
    EXPECT_CALL(*mock_network_state_helper_, IsConnected())
        .WillOnce(Return(true));
    network_screen->OnContinueButtonPressed();
    content::RunAllPendingInMessageLoop();
  }

  void SetDefaultNetworkStateHelperExpectations() {
    EXPECT_CALL(*mock_network_state_helper_, GetCurrentNetworkName())
        .Times(AnyNumber())
        .WillRepeatedly((Return(base::string16())));
    EXPECT_CALL(*mock_network_state_helper_, IsConnected())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*mock_network_state_helper_, IsConnecting())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
  }

  std::unique_ptr<MockBaseScreenDelegate> mock_base_screen_delegate_;
  login::MockNetworkStateHelper* mock_network_state_helper_;
  NetworkScreen* network_screen_;
  FakeSessionManagerClient* fake_session_manager_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkScreenTest);
};

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, CanConnect) {
  EXPECT_CALL(*mock_network_state_helper_, IsConnecting())
      .WillOnce((Return(true)));
  // EXPECT_FALSE(actor_->IsContinueEnabled());
  network_screen_->UpdateStatus();

  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .Times(2)
      .WillRepeatedly(Return(true));
  // TODO(nkostylev): Add integration with WebUI actor http://crosbug.com/22570
  // EXPECT_FALSE(actor_->IsContinueEnabled());
  // EXPECT_FALSE(actor_->IsConnecting());
  network_screen_->UpdateStatus();

  // EXPECT_TRUE(actor_->IsContinueEnabled());
  EmulateContinueButtonExit(network_screen_);
}

IN_PROC_BROWSER_TEST_F(NetworkScreenTest, Timeout) {
  EXPECT_CALL(*mock_network_state_helper_, IsConnecting())
      .WillOnce((Return(true)));
  // EXPECT_FALSE(actor_->IsContinueEnabled());
  network_screen_->UpdateStatus();

  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .Times(2)
      .WillRepeatedly(Return(false));
  // TODO(nkostylev): Add integration with WebUI actor http://crosbug.com/22570
  // EXPECT_FALSE(actor_->IsContinueEnabled());
  // EXPECT_FALSE(actor_->IsConnecting());
  network_screen_->OnConnectionTimeout();

  // Close infobubble with error message - it makes the test stable.
  // EXPECT_FALSE(actor_->IsContinueEnabled());
  // EXPECT_FALSE(actor_->IsConnecting());
  // actor_->ClearErrors();
}

class HandsOffNetworkScreenTest : public NetworkScreenTest {
 public:
  HandsOffNetworkScreenTest() {}

 private:
  // Overridden from InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
  }

  DISALLOW_COPY_AND_ASSIGN(HandsOffNetworkScreenTest);
};

#if defined(OS_CHROMEOS)
#define MAYBE_RequiresNoInput DISABLED_RequiresNoInput
#else
#define MAYBE_RequiresNoInput RequiresNoInput
#endif

IN_PROC_BROWSER_TEST_F(HandsOffNetworkScreenTest, MAYBE_RequiresNoInput) {
  WizardController* wizard_controller = WizardController::default_controller();

  // Allow the WizardController to advance throught the enrollment flow.
  network_screen_->base_screen_delegate_ = wizard_controller;

  // Simulate a network connection.
  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .WillRepeatedly((Return(true)));
  network_screen_->UpdateStatus();

  // Check if NetworkScreen::OnContinueButtonPressed() is called
  // Note that checking network_screen_->continue_pressed_ is not
  // sufficient since there are cases where OnContinueButtonPressed()
  // is called but where this variable is not set to true.
  ASSERT_TRUE((!network_screen_->is_network_subscribed_) &&
              network_screen_->network_state_helper_->IsConnected());

  // Check that we reach the enrollment screen.
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();

  // Check that attestation-based enrollment finishes
  // with either success or error.
  bool done = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      LoginDisplayHost::default_host()->GetOobeUI()->web_ui()->GetWebContents(),
      "var count = 0;"
      "function isVisible(el) {"
      "  return window.getComputedStyle(document.getElementById(el)).display"
      "    !== 'none';"
      "}"
      "function SendReplyIfEnrollmentDone() {"
      "  if (isVisible('oauth-enroll-step-abe-success') ||"
      "      isVisible('oauth-enroll-step-error')) {"
      "    domAutomationController.send(true);"
      "  } else if (count > 10) {"
      "    domAutomationController.send(false);"
      "  } else {"
      "    count++;"
      "    setTimeout(SendReplyIfEnrollmentDone, 1000);"
      "  }"
      "}"
      "SendReplyIfEnrollmentDone();",
      &done));

  // Reset the enrollment helper so there is no side effect with other tests.
  static_cast<EnrollmentScreen*>(wizard_controller->current_screen())
      ->enrollment_helper_.reset();
}

}  // namespace chromeos
