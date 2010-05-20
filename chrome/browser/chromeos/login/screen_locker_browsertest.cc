// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_screen_lock_library.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/textfield/textfield.h"

namespace chromeos {

class ScreenLockerTest : public CrosInProcessBrowserTest {
 public:
  ScreenLockerTest() {}

 private:
  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();
    InitMockScreenLockLibrary();
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenLockCompleted())
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenUnlockRequested())
        .Times(1)
        .RetiresOnSaturation();
    // Expectations for the status are on the screen lock window.
    SetStatusAreaMocksExpectations();
    // Expectations for the status area on the browser window.
    SetStatusAreaMocksExpectations();
  }

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTest);
};

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestBasic) {
  ScreenLocker::Show();
  ui_test_utils::RunAllPendingInMessageLoop();
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  tester->InjectMockAuthenticator("pass");
  EXPECT_TRUE(tester->IsOpen());
  tester->EnterPassword("fail");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(tester->IsOpen());
  tester->EnterPassword("pass");
  ui_test_utils::RunAllPendingInMessageLoop();
  // Successful authentication simply send a unlock request to PowerManager.
  EXPECT_TRUE(tester->IsOpen());

  // Emulate LockScreen request from PowerManager (via SessionManager).
  // TODO(oshima): Find out better way to handle this in mock.
  ScreenLocker::Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(!tester->IsOpen());
}

}  // namespace chromeos
