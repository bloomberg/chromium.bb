// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_base_screen_delegate.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/chromeos_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::_;

namespace chromeos {

class EnrollmentScreenTest : public WizardInProcessBrowserTest {
 public:
  EnrollmentScreenTest()
      : WizardInProcessBrowserTest(
            WizardController::kEnrollmentScreenName) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestCancel) {
  ASSERT_TRUE(WizardController::default_controller() != NULL);

  EnrollmentScreen* enrollment_screen =
      EnrollmentScreen::Get(WizardController::default_controller());
  ASSERT_TRUE(enrollment_screen != NULL);

  base::RunLoop run_loop;
  MockBaseScreenDelegate mock_base_screen_delegate;
  static_cast<BaseScreen*>(enrollment_screen)->base_screen_delegate_ =
      &mock_base_screen_delegate;

  ASSERT_EQ(WizardController::default_controller()->current_screen(),
            enrollment_screen);

  EXPECT_CALL(mock_base_screen_delegate,
              OnExit(_, BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  enrollment_screen->OnCancel();
  content::RunThisRunLoop(&run_loop);
  Mock::VerifyAndClearExpectations(&mock_base_screen_delegate);

  static_cast<BaseScreen*>(enrollment_screen)->base_screen_delegate_ =
      WizardController::default_controller();
}

// Flaky test: crbug.com/394069
IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, DISABLED_TestSuccess) {
  ASSERT_TRUE(WizardController::default_controller() != NULL);
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  EnrollmentScreen* enrollment_screen =
      EnrollmentScreen::Get(WizardController::default_controller());
  ASSERT_TRUE(enrollment_screen != NULL);

  base::RunLoop run_loop;
  MockBaseScreenDelegate mock_base_screen_delegate;
  static_cast<BaseScreen*>(enrollment_screen)->base_screen_delegate_ =
      &mock_base_screen_delegate;

  ASSERT_EQ(WizardController::default_controller()->current_screen(),
            enrollment_screen);

  enrollment_screen->OnDeviceEnrolled("");
  run_loop.RunUntilIdle();
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());

  static_cast<BaseScreen*>(enrollment_screen)->base_screen_delegate_ =
      WizardController::default_controller();
}

class ProvisionedEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ProvisionedEnrollmentScreenTest() {}

 private:
  // Overridden from InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
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
  ASSERT_TRUE(WizardController::default_controller() != NULL);

  EnrollmentScreen* enrollment_screen =
      EnrollmentScreen::Get(WizardController::default_controller());
  ASSERT_TRUE(enrollment_screen != NULL);

  base::RunLoop run_loop;
  MockBaseScreenDelegate mock_base_screen_delegate;
  static_cast<BaseScreen*>(enrollment_screen)->base_screen_delegate_ =
      &mock_base_screen_delegate;

  ASSERT_EQ(WizardController::default_controller()->current_screen(),
            enrollment_screen);

  EXPECT_CALL(mock_base_screen_delegate,
              OnExit(_, BaseScreenDelegate::ENTERPRISE_ENROLLMENT_BACK, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  enrollment_screen->OnCancel();
  content::RunThisRunLoop(&run_loop);
  Mock::VerifyAndClearExpectations(&mock_base_screen_delegate);

  static_cast<BaseScreen*>(enrollment_screen)->base_screen_delegate_ =
      WizardController::default_controller();
}

}  // namespace chromeos
