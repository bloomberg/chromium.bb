// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_input_method_library.h"
#include "chrome/browser/chromeos/cros/mock_screen_lock_library.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/textfield/textfield.h"
#include "views/window/window_gtk.h"

namespace {

// An object that wait for lock state and fullscreen state.
class Waiter : public NotificationObserver {
 public:
  explicit Waiter(Browser* browser)
      : browser_(browser),
        running_(false) {
    registrar_.Add(this,
                   NotificationType::SCREEN_LOCK_STATE_CHANGED,
                   NotificationService::AllSources());
    handler_id_ = g_signal_connect(
        G_OBJECT(browser_->window()->GetNativeHandle()),
        "window-state-event",
        G_CALLBACK(OnWindowStateEventThunk),
        this);
  }

  ~Waiter() {
    g_signal_handler_disconnect(
        G_OBJECT(browser_->window()->GetNativeHandle()),
        handler_id_);
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::SCREEN_LOCK_STATE_CHANGED);
    if (running_)
      MessageLoop::current()->Quit();
  }

  // Wait until the two conditions are met.
  void Wait(bool locker_state, bool fullscreen) {
    running_ = true;
    scoped_ptr<chromeos::test::ScreenLockerTester>
        tester(chromeos::ScreenLocker::GetTester());
    while (tester->IsLocked() != locker_state ||
           browser_->window()->IsFullscreen() != fullscreen) {
      ui_test_utils::RunMessageLoop();
    }
    // Make sure all pending tasks are executed.
    ui_test_utils::RunAllPendingInMessageLoop();
    running_ = false;
  }

  CHROMEGTK_CALLBACK_1(Waiter, gboolean, OnWindowStateEvent,
                       GdkEventWindowState*);

 private:
  Browser* browser_;
  gulong handler_id_;
  NotificationRegistrar registrar_;

  // Are we currently running the message loop?
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

gboolean Waiter::OnWindowStateEvent(GtkWidget* widget,
                                    GdkEventWindowState* event) {
  MessageLoop::current()->Quit();
  return false;
}

}  // namespace

namespace chromeos {

class ScreenLockerTest : public CrosInProcessBrowserTest {
 public:
  ScreenLockerTest() : mock_screen_lock_library_(NULL),
                       mock_input_method_library_(NULL) {
  }

 protected:
  MockScreenLockLibrary *mock_screen_lock_library_;
  MockInputMethodLibrary *mock_input_method_library_;

  // Test the no password mode with different unlock scheme given by
  // |unlock| function.
  void TestNoPassword(void (unlock)(views::Widget*)) {
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenUnlockRequested())
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenLockCompleted())
        .Times(1)
        .RetiresOnSaturation();
    UserManager::Get()->OffTheRecordUserLoggedIn();
    ScreenLocker::Show();
    scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
    tester->EmulateWindowManagerReady();
    if (!chromeos::ScreenLocker::GetTester()->IsLocked())
      ui_test_utils::WaitForNotification(
          NotificationType::SCREEN_LOCK_STATE_CHANGED);
    EXPECT_TRUE(tester->IsLocked());
    tester->InjectMockAuthenticator("", "");

    unlock(tester->GetWidget());

    ui_test_utils::RunAllPendingInMessageLoop();
    EXPECT_TRUE(tester->IsLocked());

    // Emulate LockScreen request from PowerManager (via SessionManager).
    ScreenLocker::Hide();
    ui_test_utils::RunAllPendingInMessageLoop();
    EXPECT_FALSE(tester->IsLocked());
  }

  void LockScreenWithUser(test::ScreenLockerTester* tester,
                          const std::string& user) {
    UserManager::Get()->UserLoggedIn(user);
    ScreenLocker::Show();
    tester->EmulateWindowManagerReady();
    if (!tester->IsLocked()) {
      ui_test_utils::WaitForNotification(
          NotificationType::SCREEN_LOCK_STATE_CHANGED);
    }
    EXPECT_TRUE(tester->IsLocked());
  }

 private:
  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->InitMockScreenLockLibrary();
    mock_screen_lock_library_ = cros_mock_->mock_screen_lock_library();
    mock_input_method_library_ = cros_mock_->mock_input_method_library();
    EXPECT_CALL(*mock_screen_lock_library_, AddObserver(testing::_))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenUnlockCompleted())
        .Times(1)
        .RetiresOnSaturation();
    // Expectations for the status are on the screen lock window.
    cros_mock_->SetStatusAreaMocksExpectations();
    // Expectations for the status area on the browser window.
    cros_mock_->SetStatusAreaMocksExpectations();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
    command_line->AppendSwitch(switches::kNoFirstRun);
  }

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTest);
};

// PulseAudioMixer sometimes crashes at exit. See http://crosbug.om/9303
IN_PROC_BROWSER_TEST_F(ScreenLockerTest, FLAKY_TestBasic) {
  EXPECT_CALL(*mock_input_method_library_, GetNumActiveInputMethods())
      .Times(1)
      .WillRepeatedly((testing::Return(0)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenUnlockRequested())
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenLockCompleted())
      .Times(1)
      .RetiresOnSaturation();
  UserManager::Get()->UserLoggedIn("user");
  ScreenLocker::Show();
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  tester->EmulateWindowManagerReady();
  if (!chromeos::ScreenLocker::GetTester()->IsLocked())
    ui_test_utils::WaitForNotification(
        NotificationType::SCREEN_LOCK_STATE_CHANGED);

  // Test to make sure that the widget is actually appearing and is of
  // reasonable size, preventing a regression of
  // http://code.google.com/p/chromium-os/issues/detail?id=5987
  gfx::Rect lock_bounds;
  tester->GetChildWidget()->GetBounds(&lock_bounds, true);
  EXPECT_GT(lock_bounds.width(), 10);
  EXPECT_GT(lock_bounds.height(), 10);

  tester->InjectMockAuthenticator("user", "pass");
  EXPECT_TRUE(tester->IsLocked());
  tester->EnterPassword("fail");
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_TRUE(tester->IsLocked());
  tester->EnterPassword("pass");
  ui_test_utils::RunAllPendingInMessageLoop();
  // Successful authentication simply send a unlock request to PowerManager.
  EXPECT_TRUE(tester->IsLocked());

  // Emulate LockScreen request from PowerManager (via SessionManager).
  // TODO(oshima): Find out better way to handle this in mock.
  ScreenLocker::Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestFullscreenExit) {
  EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenUnlockRequested())
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenLockCompleted())
      .Times(1)
      .RetiresOnSaturation();
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  {
    Waiter waiter(browser());
    browser()->ToggleFullscreenMode();
    waiter.Wait(false /* not locked */, true /* full screen */);
    EXPECT_TRUE(browser()->window()->IsFullscreen());
    EXPECT_FALSE(tester->IsLocked());
  }
  {
    Waiter waiter(browser());
    UserManager::Get()->UserLoggedIn("user");
    ScreenLocker::Show();
    tester->EmulateWindowManagerReady();
    waiter.Wait(true /* locked */, false /* full screen */);
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    EXPECT_TRUE(tester->IsLocked());
  }
  tester->InjectMockAuthenticator("user", "pass");
  tester->EnterPassword("pass");
  ui_test_utils::RunAllPendingInMessageLoop();
  ScreenLocker::Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
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
                            ui::VKEY_SPACE, false, false, false, false);
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestNoPasswordWithKeyPress) {
  TestNoPassword(KeyPress);
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestShowTwice) {
  EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenLockCompleted())
      .Times(2)
      .RetiresOnSaturation();
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  LockScreenWithUser(tester.get(), "user");

  // Ensure there's a profile or this test crashes.
  ProfileManager::GetDefaultProfile();

  // Calling Show again simply send LockCompleted signal.
  ScreenLocker::Show();
  EXPECT_TRUE(tester->IsLocked());

  // Close the locker to match expectations.
  ScreenLocker::Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestEscape) {
  EXPECT_CALL(*mock_screen_lock_library_, NotifyScreenLockCompleted())
      .Times(1)
      .RetiresOnSaturation();
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  LockScreenWithUser(tester.get(), "user");

  // Ensure there's a profile or this test crashes.
  ProfileManager::GetDefaultProfile();

  tester->SetPassword("password");
  EXPECT_EQ("password", tester->GetPassword());
  // Escape clears the password.
  ui_controls::SendKeyPress(GTK_WINDOW(tester->GetWidget()->GetNativeView()),
                            ui::VKEY_ESCAPE, false, false, false, false);
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_EQ("", tester->GetPassword());

  // Close the locker to match expectations.
  ScreenLocker::Hide();
  ui_test_utils::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
}

}  // namespace chromeos
