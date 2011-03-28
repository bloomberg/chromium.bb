// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/mock_screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;

namespace chromeos {

class EnterpriseEnrollmentScreenTest : public WizardInProcessBrowserTest {
 public:
  EnterpriseEnrollmentScreenTest()
      : WizardInProcessBrowserTest(
            WizardController::kEnterpriseEnrollmentScreenName) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EnterpriseEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(EnterpriseEnrollmentScreenTest, TestCancel) {
  ASSERT_TRUE(controller() != NULL);
  MockScreenObserver mock_screen_observer;
  controller()->set_observer(&mock_screen_observer);
  EnterpriseEnrollmentScreen* enterprise_enrollment_screen =
      controller()->GetEnterpriseEnrollmentScreen();
  ASSERT_TRUE(enterprise_enrollment_screen != NULL);
  ASSERT_EQ(controller()->current_screen(), enterprise_enrollment_screen);

  EXPECT_CALL(mock_screen_observer,
              OnExit(ScreenObserver::ENTERPRISE_ENROLLMENT_CANCELLED));
  enterprise_enrollment_screen->CancelEnrollment();
  Mock::VerifyAndClearExpectations(&mock_screen_observer);

  controller()->set_observer(NULL);
}

}  // namespace chromeos
