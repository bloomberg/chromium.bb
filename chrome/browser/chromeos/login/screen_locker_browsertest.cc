// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_locker.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/mock_authenticator.h"
#include "chrome/browser/chromeos/login/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

// An object that wait for lock state and fullscreen state.
class Waiter : public content::NotificationObserver {
 public:
  explicit Waiter(Browser* browser)
      : browser_(browser),
        running_(false) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   chrome::NOTIFICATION_FULLSCREEN_CHANGED,
                   content::NotificationService::AllSources());
  }

  virtual ~Waiter() {
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(type == chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED ||
           type == chrome::NOTIFICATION_FULLSCREEN_CHANGED);
    if (running_)
      base::MessageLoop::current()->Quit();
  }

  // Wait until the two conditions are met.
  void Wait(bool locker_state, bool fullscreen) {
    running_ = true;
    scoped_ptr<chromeos::test::ScreenLockerTester>
        tester(chromeos::ScreenLocker::GetTester());
    while (tester->IsLocked() != locker_state ||
           browser_->window()->IsFullscreen() != fullscreen) {
      content::RunMessageLoop();
    }
    // Make sure all pending tasks are executed.
    content::RunAllPendingInMessageLoop();
    running_ = false;
  }

 private:
  Browser* browser_;
  content::NotificationRegistrar registrar_;

  // Are we currently running the message loop?
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

}  // namespace

namespace chromeos {

class ScreenLockerTest : public InProcessBrowserTest {
 public:
  ScreenLockerTest() : fake_session_manager_client_(NULL) {
  }

 protected:
  FakeSessionManagerClient* fake_session_manager_client_;

  void LockScreen(test::ScreenLockerTester* tester) {
    ScreenLocker::Show();
    tester->EmulateWindowManagerReady();
    content::WindowedNotificationObserver lock_state_observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    if (!tester->IsLocked())
      lock_state_observer.Wait();
    EXPECT_TRUE(tester->IsLocked());
  }

  // Verifies if LockScreenDismissed() was called once.
  bool VerifyLockScreenDismissed() {
    return 1 == fake_session_manager_client_->
                    notify_lock_screen_dismissed_call_count();
  }

 private:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    MockDBusThreadManagerWithoutGMock* mock_dbus_thread_manager =
        new MockDBusThreadManagerWithoutGMock;
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    fake_session_manager_client_ =
        mock_dbus_thread_manager->fake_session_manager_client();
    zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  }

  scoped_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTest);
};

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestBasic) {
  ScreenLocker::Show();
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  tester->EmulateWindowManagerReady();
  content::WindowedNotificationObserver lock_state_observer(
      chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
      content::NotificationService::AllSources());
  if (!chromeos::ScreenLocker::GetTester()->IsLocked())
    lock_state_observer.Wait();

  // Test to make sure that the widget is actually appearing and is of
  // reasonable size, preventing a regression of
  // http://code.google.com/p/chromium-os/issues/detail?id=5987
  gfx::Rect lock_bounds = tester->GetChildWidget()->GetWindowBoundsInScreen();
  EXPECT_GT(lock_bounds.width(), 10);
  EXPECT_GT(lock_bounds.height(), 10);

  tester->InjectMockAuthenticator(UserManager::kStubUser, "pass");
  EXPECT_TRUE(tester->IsLocked());
  tester->EnterPassword("fail");
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(tester->IsLocked());
  tester->EnterPassword("pass");
  content::RunAllPendingInMessageLoop();
  // Successful authentication simply send a unlock request to PowerManager.
  EXPECT_TRUE(tester->IsLocked());
  EXPECT_EQ(
      1,
      fake_session_manager_client_->notify_lock_screen_shown_call_count());

  // Emulate LockScreen request from SessionManager.
  // TODO(oshima): Find out better way to handle this in mock.
  ScreenLocker::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
  EXPECT_TRUE(VerifyLockScreenDismissed());
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestFullscreenExit) {
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  {
    Waiter waiter(browser());
    browser()->fullscreen_controller()->ToggleFullscreenMode();
    waiter.Wait(false /* not locked */, true /* full screen */);
    EXPECT_TRUE(browser()->window()->IsFullscreen());
    EXPECT_FALSE(tester->IsLocked());
  }
  {
    Waiter waiter(browser());
    ScreenLocker::Show();
    tester->EmulateWindowManagerReady();
    waiter.Wait(true /* locked */, false /* full screen */);
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    EXPECT_TRUE(tester->IsLocked());
  }
  EXPECT_EQ(
      1,
      fake_session_manager_client_->notify_lock_screen_shown_call_count());

  tester->InjectMockAuthenticator(UserManager::kStubUser, "pass");
  tester->EnterPassword("pass");
  content::RunAllPendingInMessageLoop();
  ScreenLocker::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
  EXPECT_TRUE(VerifyLockScreenDismissed());
}

void SimulateKeyPress(views::Widget* widget, ui::KeyboardCode key_code) {
  ui_controls::SendKeyPress(widget->GetNativeWindow(),
                            key_code, false, false, false, false);
}

void UnlockKeyPress(views::Widget* widget) {
  SimulateKeyPress(widget, ui::VKEY_SPACE);
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestShowTwice) {
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  LockScreen(tester.get());

  // Ensure there's a profile or this test crashes.
  ProfileManager::GetDefaultProfile();

  // Calling Show again simply send LockCompleted signal.
  ScreenLocker::Show();
  EXPECT_TRUE(tester->IsLocked());
  EXPECT_EQ(
      2,
      fake_session_manager_client_->notify_lock_screen_shown_call_count());


  // Close the locker to match expectations.
  ScreenLocker::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
  EXPECT_TRUE(VerifyLockScreenDismissed());
}

// TODO(flackr): Find out why the RenderView isn't getting the escape press
// and re-enable this test (currently this test is flaky).
IN_PROC_BROWSER_TEST_F(ScreenLockerTest, DISABLED_TestEscape) {
  scoped_ptr<test::ScreenLockerTester> tester(ScreenLocker::GetTester());
  LockScreen(tester.get());

  // Ensure there's a profile or this test crashes.
  ProfileManager::GetDefaultProfile();

  EXPECT_EQ(
      1,
      fake_session_manager_client_->notify_lock_screen_shown_call_count());

  tester->SetPassword("password");
  EXPECT_EQ("password", tester->GetPassword());
  // Escape clears the password.
  SimulateKeyPress(tester->GetWidget(), ui::VKEY_ESCAPE);
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ("", tester->GetPassword());

  // Close the locker to match expectations.
  ScreenLocker::Hide();
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(tester->IsLocked());
  EXPECT_TRUE(VerifyLockScreenDismissed());
}

}  // namespace chromeos
