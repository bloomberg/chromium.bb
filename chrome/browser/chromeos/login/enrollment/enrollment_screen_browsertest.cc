// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_base_screen_delegate.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::_;

namespace chromeos {

class EnrollmentScreenTest : public InProcessBrowserTest {
 public:
  EnrollmentScreenTest() = default;
  ~EnrollmentScreenTest() override = default;

  // Overridden from InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendArg(switches::kLoginManager);
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ShowLoginWizard(OobeScreen::SCREEN_OOBE_ENROLLMENT);
    EXPECT_EQ(WizardController::default_controller()->current_screen(),
              enrollment_screen());
    static_cast<BaseScreen*>(enrollment_screen())->base_screen_delegate_ =
        &mock_base_screen_delegate_;
  }
  void TearDownOnMainThread() override {
    static_cast<BaseScreen*>(enrollment_screen())->base_screen_delegate_ =
        WizardController::default_controller();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  EnrollmentScreen* enrollment_screen() {
    EXPECT_TRUE(WizardController::default_controller());
    EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
    EXPECT_TRUE(enrollment_screen);
    return enrollment_screen;
  }

  MockBaseScreenDelegate* mock_base_screen_delegate() {
    return &mock_base_screen_delegate_;
  }

 private:
  MockBaseScreenDelegate mock_base_screen_delegate_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestCancel) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_base_screen_delegate(),
              OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  enrollment_screen()->OnCancel();
  run_loop.Run();
}

// Flaky test: crbug.com/394069
IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, DISABLED_TestSuccess) {
  WizardController::SkipEnrollmentPromptsForTesting();
  base::RunLoop run_loop;
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_CALL(*mock_base_screen_delegate(),
              OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  enrollment_screen()->OnDeviceAttributeUpdatePermission(false /* granted */);
  run_loop.Run();
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

class AttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  AttestationAuthEnrollmentScreenTest() = default;
  ~AttestationAuthEnrollmentScreenTest() override = default;

 private:
  // Overridden from EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnterpriseEnableZeroTouchEnrollment);
  }

  DISALLOW_COPY_AND_ASSIGN(AttestationAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(AttestationAuthEnrollmentScreenTest, TestCancel) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_base_screen_delegate(),
              OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_screen()->OnCancel();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, EnrollmentSpinner) {
  EnrollmentScreenView* view = enrollment_screen()->GetView();
  ASSERT_TRUE(view);

  test::JSChecker checker(
      LoginDisplayHost::default_host()->GetOobeWebContents());

  // Run through the flow
  view->Show();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();
  checker.ExpectTrue(
      "window.getComputedStyle(document.getElementById('oauth-enroll-step-"
      "signin')).display !== 'none'");

  view->ShowEnrollmentSpinnerScreen();
  checker.ExpectTrue(
      "window.getComputedStyle(document.getElementById('oauth-enroll-step-"
      "working')).display !== 'none'");

  view->ShowAttestationBasedEnrollmentSuccessScreen("fake domain");
  checker.ExpectTrue(
      "window.getComputedStyle(document.getElementById('oauth-enroll-step-"
      "success')).display !== 'none'");
}

class ForcedAttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ForcedAttestationAuthEnrollmentScreenTest() = default;
  ~ForcedAttestationAuthEnrollmentScreenTest() override = default;

 private:
  // Overridden from EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "forced");
  }

  DISALLOW_COPY_AND_ASSIGN(ForcedAttestationAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(ForcedAttestationAuthEnrollmentScreenTest, TestCancel) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_base_screen_delegate(),
              OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_screen()->OnCancel();
  run_loop.Run();
}

class MultiAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  MultiAuthEnrollmentScreenTest() = default;
  ~MultiAuthEnrollmentScreenTest() override = default;

 private:
  // Overridden from EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnterpriseEnableZeroTouchEnrollment);
    // Kiosk mode will force OAuth enrollment.
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }

  DISALLOW_COPY_AND_ASSIGN(MultiAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(MultiAuthEnrollmentScreenTest, TestCancel) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_base_screen_delegate(),
              OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  ASSERT_TRUE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_screen()->OnCancel();
  run_loop.Run();
}

class ProvisionedEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ProvisionedEnrollmentScreenTest() = default;
  ~ProvisionedEnrollmentScreenTest() override = default;

 private:
  // Overridden from EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }

  DISALLOW_COPY_AND_ASSIGN(ProvisionedEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(ProvisionedEnrollmentScreenTest, TestBackButton) {
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_base_screen_delegate(),
              OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  enrollment_screen()->OnCancel();
  run_loop.Run();
}

}  // namespace chromeos
