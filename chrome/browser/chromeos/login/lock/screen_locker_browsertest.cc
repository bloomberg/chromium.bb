// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/lock/screen_locker.h"

#include <memory>

#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace chromeos {
namespace {

// An object that wait for lock state and fullscreen state.
class Waiter : public content::NotificationObserver {
 public:
  explicit Waiter(Browser* browser) : browser_(browser) {
    registrar_.Add(this, chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_FULLSCREEN_CHANGED,
                   content::NotificationService::AllSources());
  }

  ~Waiter() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK(type == chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED ||
           type == chrome::NOTIFICATION_FULLSCREEN_CHANGED);
    if (quit_loop_)
      std::move(quit_loop_).Run();
  }

  // Wait until the two conditions are met.
  void Wait(bool locker_state, bool fullscreen) {
    std::unique_ptr<ScreenLockerTester> tester = ScreenLockerTester::Create();
    while (tester->IsLocked() != locker_state ||
           browser_->window()->IsFullscreen() != fullscreen) {
      base::RunLoop run_loop;
      quit_loop_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    // Make sure all pending tasks are executed.
    base::RunLoop().RunUntilIdle();
  }

 private:
  Browser* browser_;
  content::NotificationRegistrar registrar_;

  base::OnceClosure quit_loop_;

  DISALLOW_COPY_AND_ASSIGN(Waiter);
};

}  // namespace

class ScreenLockerTest : public InProcessBrowserTest {
 public:
  ScreenLockerTest() = default;
  ~ScreenLockerTest() override = default;

  void LockScreen(ScreenLockerTester* tester) {
    ScreenLocker::Show();
    content::WindowedNotificationObserver lock_state_observer(
        chrome::NOTIFICATION_SCREEN_LOCK_STATE_CHANGED,
        content::NotificationService::AllSources());
    if (!tester->IsLocked())
      lock_state_observer.Wait();
    EXPECT_TRUE(tester->IsLocked());
    EXPECT_EQ(session_manager::SessionState::LOCKED,
              session_manager::SessionManager::Get()->session_state());
  }

  FakeSessionManagerClient* session_manager_client() {
    return fake_session_manager_client_;
  }

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  }

  void SetUpInProcessBrowserTestFixture() override {
    fake_session_manager_client_ = new FakeSessionManagerClient;
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));

    zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  }

 private:
  FakeSessionManagerClient* fake_session_manager_client_ = nullptr;
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockerTest);
};

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestBadThenGoodPassword) {
  // Show lock screen and wait until it is shown.
  std::unique_ptr<ScreenLockerTester> tester = ScreenLockerTester::Create();
  LockScreen(tester.get());

  // Inject fake authentication credentials.
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           user_manager::StubAccountId());
  user_context.SetKey(Key("pass"));
  tester->InjectStubUserContext(user_context);
  EXPECT_TRUE(tester->IsLocked());

  // Submit a bad password.
  tester->EnterPassword(user_manager::StubAccountId(), "fail");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tester->IsLocked());

  // Submit the correct password. Successful authentication clears the lock
  // screen and tells the SessionManager to announce this over DBus.
  tester->EnterPassword(user_manager::StubAccountId(), "pass");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tester->IsLocked());
  EXPECT_EQ(1, session_manager_client()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());
  EXPECT_EQ(
      1, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

// Makes sure Chrome doesn't crash if we lock the screen during an add-user
// flow. Regression test for crbug.com/467111.
IN_PROC_BROWSER_TEST_F(ScreenLockerTest, LockScreenWhileAddingUser) {
  UserAddingScreen::Get()->Start();
  base::RunLoop().RunUntilIdle();
  ScreenLocker::HandleShowLockScreenRequest();
}

// Test how locking the screen affects an active fullscreen window.
IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestFullscreenExit) {
  // WebUiScreenLockerTest fails with Mash because of unexpected window
  // structure. Fortunately we will deprecate the WebUI-based screen locker
  // soon, so it is okay to skip it.  See https://crbug.com/888779
  if (features::IsUsingWindowService())
    return;
  // 1) If the active browser window is in fullscreen and the fullscreen window
  // does not have all the pixels (e.g. the shelf is auto hidden instead of
  // hidden), locking the screen should not exit fullscreen. The shelf is
  // auto hidden when in immersive fullscreen.
  std::unique_ptr<ScreenLockerTester> tester = ScreenLockerTester::Create();
  BrowserWindow* browser_window = browser()->window();
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(browser_window->GetNativeWindow());
  {
    Waiter waiter(browser());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->ToggleBrowserFullscreenMode();
    waiter.Wait(false /* not locked */, true /* full screen */);
    EXPECT_TRUE(browser_window->IsFullscreen());
    EXPECT_FALSE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_FALSE(tester->IsLocked());
  }
  {
    Waiter waiter(browser());
    ScreenLocker::Show();
    waiter.Wait(true /* locked */, true /* full screen */);
    EXPECT_TRUE(browser_window->IsFullscreen());
    EXPECT_FALSE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_TRUE(tester->IsLocked());
  }
  UserContext user_context(user_manager::UserType::USER_TYPE_REGULAR,
                           user_manager::StubAccountId());
  user_context.SetKey(Key("pass"));
  tester->InjectStubUserContext(user_context);
  tester->EnterPassword(user_manager::StubAccountId(), "pass");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tester->IsLocked());
  {
    Waiter waiter(browser());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->ToggleBrowserFullscreenMode();
    waiter.Wait(false /* not locked */, false /* fullscreen */);
    EXPECT_FALSE(browser_window->IsFullscreen());
  }

  // Browser window should be activated after screen locker is gone. Otherwise,
  // the rest of the test would fail.
  ASSERT_EQ(window_state, ash::wm::GetActiveWindowState());

  // 2) If the active browser window is in fullscreen and the fullscreen window
  // has all of the pixels, locking the screen should exit fullscreen. The
  // fullscreen window has all of the pixels when in tab fullscreen.
  {
    Waiter waiter(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents, GURL());
    waiter.Wait(false /* not locked */, true /* fullscreen */);
    EXPECT_TRUE(browser_window->IsFullscreen());
    EXPECT_TRUE(window_state->GetHideShelfWhenFullscreen());
    EXPECT_FALSE(tester->IsLocked());
  }
  {
    Waiter waiter(browser());
    ScreenLocker::Show();
    waiter.Wait(true /* locked */, false /* full screen */);
    EXPECT_FALSE(browser_window->IsFullscreen());
    EXPECT_TRUE(tester->IsLocked());
  }

  tester->EnterPassword(user_manager::StubAccountId(), "pass");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tester->IsLocked());

  EXPECT_EQ(2, session_manager_client()->notify_lock_screen_shown_call_count());
  EXPECT_EQ(
      2, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

IN_PROC_BROWSER_TEST_F(ScreenLockerTest, TestShowTwice) {
  std::unique_ptr<ScreenLockerTester> tester = ScreenLockerTester::Create();
  LockScreen(tester.get());

  // Calling Show again simply send LockCompleted signal.
  ScreenLocker::Show();
  EXPECT_TRUE(tester->IsLocked());
  EXPECT_EQ(2, session_manager_client()->notify_lock_screen_shown_call_count());

  // Close the locker to match expectations.
  ScreenLocker::Hide();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tester->IsLocked());
  EXPECT_EQ(
      1, session_manager_client()->notify_lock_screen_dismissed_call_count());
}

}  // namespace chromeos
