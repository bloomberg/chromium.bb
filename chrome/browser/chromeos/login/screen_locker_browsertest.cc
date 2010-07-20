// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_screen_lock_library.h"
#include "chrome/browser/chromeos/cros/mock_input_method_library.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/views/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/textfield/textfield.h"

namespace chromeos {

class ScreenLockerTest : public CrosInProcessBrowserTest {
 public:
  ScreenLockerTest() {}

 protected:

  // Test the no password mode with different unlock scheme given by
  // |unlock| function.
  void TestNoPassword(void (unlock)(views::Widget*)) {
    UserManager::Get()->OffTheRecordUserLoggedIn();
    ScreenLocker::Show();
    scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
    tester->EmulateWindowManagerReady();
    ui_test_utils::WaitForNotification(
        NotificationType::SCREEN_LOCK_STATE_CHANGED);
    EXPECT_TRUE(tester->IsOpen());
    tester->InjectMockAuthenticator("", "");

    unlock(tester->GetWidget());

    ui_test_utils::RunAllPendingInMessageLoop();
    EXPECT_TRUE(tester->IsOpen());

    // Emulate LockScreen request from PowerManager (via SessionManager).
    ScreenLocker::Hide();
    ui_test_utils::RunAllPendingInMessageLoop();
    EXPECT_FALSE(tester->IsOpen());
  }

 private:
  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();
    InitMockScreenLockLibrary();
    EXPECT_CALL(*mock_screen_lock_library_, AddObserver(testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenLockCompleted())
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenUnlockRequested())
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenUnlockCompleted())
        .Times(1)
        .RetiresOnSaturation();
    // Expectations for the status are on the screen lock window.
    SetStatusAreaMocksExpectations();
    // Expectations for the status area on the browser window.
    SetStatusAreaMocksExpectations();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchWithValue(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kNoFirstRun);
  }

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTest);
};

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestBasic) {
  EXPECT_CALL(*mock_input_method_library_, GetNumActiveInputMethods())
      .Times(1)
      .WillRepeatedly((testing::Return(0)))
      .RetiresOnSaturation();
  UserManager::Get()->UserLoggedIn("user");
  ScreenLocker::Show();
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  tester->EmulateWindowManagerReady();
  ui_test_utils::WaitForNotification(
      NotificationType::SCREEN_LOCK_STATE_CHANGED);

  tester->InjectMockAuthenticator("user", "pass");
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
  EXPECT_FALSE(tester->IsOpen());
}

void MouseMove(views::Widget* widget) {
  ui_controls::SendMouseMove(10, 10);
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestNoPasswordWithMouseMove) {
  TestNoPassword(MouseMove);
}

void MouseClick(views::Widget* widget) {
  ui_controls::SendMouseClick(ui_controls::RIGHT);
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestNoPasswordWithMouseClick) {
  TestNoPassword(MouseClick);
}

void KeyPress(views::Widget* widget) {
  ui_controls::SendKeyPress(GTK_WINDOW(widget->GetNativeView()),
                            base::VKEY_SPACE, false, false, false, false);
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestNoPasswordWithKeyPress) {
  TestNoPassword(KeyPress);
}

}  // namespace chromeos
