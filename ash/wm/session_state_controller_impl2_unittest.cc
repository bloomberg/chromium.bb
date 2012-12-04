// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller_impl2.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/cursor_manager.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_controller.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace ash {
namespace test {
namespace {
bool cursor_visible() {
  return ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible();
}

void CheckCalledCallback(bool* flag) {
  (*flag) = true;
}
}

// Fake implementation of PowerButtonControllerDelegate that just logs requests
// to lock the screen and shut down the device.
class TestSessionStateControllerDelegate :
    public SessionStateControllerDelegate {
 public:
  TestSessionStateControllerDelegate()
      : num_lock_requests_(0),
        num_shutdown_requests_(0) {}

  int num_lock_requests() const { return num_lock_requests_; }
  int num_shutdown_requests() const { return num_shutdown_requests_; }

  // SessionStateControllerDelegate implementation.
  virtual void RequestLockScreen() OVERRIDE {
    num_lock_requests_++;
  }
  virtual void RequestShutdown() OVERRIDE {
    num_shutdown_requests_++;
  }

 private:
  int num_lock_requests_;
  int num_shutdown_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionStateControllerDelegate);
};

class SessionStateControllerImpl2Test : public AshTestBase {
 public:
  SessionStateControllerImpl2Test() : controller_(NULL), delegate_(NULL) {}
  virtual ~SessionStateControllerImpl2Test() {}

  void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshNewLockAnimationsEnabled);

    AshTestBase::SetUp();
    delegate_ = new TestSessionStateControllerDelegate;
    controller_ = Shell::GetInstance()->power_button_controller();
    state_controller_ = static_cast<SessionStateControllerImpl2*>(
        Shell::GetInstance()->session_state_controller());
    state_controller_->SetDelegate(delegate_);  // transfers ownership
    test_api_.reset(
        new SessionStateControllerImpl2::TestApi(state_controller_));
    animator_api_.reset(
        new internal::SessionStateAnimator::TestApi(state_controller_->
            animator_.get()));
    shell_delegate_ = reinterpret_cast<TestShellDelegate*>(
        ash::Shell::GetInstance()->delegate());
  }

 protected:
  void GenerateMouseMoveEvent() {
    aura::test::EventGenerator generator(
        Shell::GetPrimaryRootWindow());
    generator.MoveMouseTo(10, 10);
  }

  int NumShutdownRequests() {
    return delegate_->num_shutdown_requests() +
           shell_delegate_->num_exit_requests();
  }

  PowerButtonController* controller_;  // not owned
  SessionStateControllerImpl2* state_controller_;  // not owned
  TestSessionStateControllerDelegate* delegate_;  // not owned
  TestShellDelegate* shell_delegate_;  // not owned

  scoped_ptr<SessionStateControllerImpl2::TestApi> test_api_;
  scoped_ptr<internal::SessionStateAnimator::TestApi> animator_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateControllerImpl2Test);
};

// Test the lock-to-shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should lock immediately the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
TEST_F(SessionStateControllerImpl2Test, LegacyLockAndShutDown) {
  controller_->set_has_legacy_power_button_for_test(true);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(false);

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));

  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_EQ(1, delegate_->num_lock_requests());

  // Notify that we locked successfully.
  state_controller_->OnStartingLock();

  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));


  // Notify that the lock window is visible.  We should make it fade in.
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();

  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));

  // We shouldn't progress towards the shutdown state, however.
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());

  // Hold the button again and check that we start shutting down.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_EQ(0, NumShutdownRequests());

  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  // Make sure a mouse move event won't show the cursor.
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're not logged in on an unofficial system.
TEST_F(SessionStateControllerImpl2Test, LegacyNotLoggedIn) {
  controller_->set_has_legacy_power_button_for_test(true);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller_->OnLockStateChanged(false);
  SetUserLoggedIn(false);
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're logged in as a guest on an unofficial system.
TEST_F(SessionStateControllerImpl2Test, LegacyGuest) {
  controller_->set_has_legacy_power_button_for_test(true);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_GUEST);
  state_controller_->OnLockStateChanged(false);
  SetCanLockScreen(false);
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// When we hold the power button while the user isn't logged in, we should shut
// down the machine directly.
TEST_F(SessionStateControllerImpl2Test, ShutdownWhenNotLoggedIn) {
  controller_->set_has_legacy_power_button_for_test(false);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller_->OnLockStateChanged(false);
  SetUserLoggedIn(false);

  // Press the power button and check that we start the shutdown timer.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));

  // Release the power button before the shutdown timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS));

  // Press the button again and make the shutdown timeout fire this time.
  // Check that we start the timer for actually requesting the shutdown.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  test_api_->trigger_shutdown_timeout();
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));

  // When the timout fires, we should request a shutdown.
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we lock the screen and deal with unlocking correctly.
TEST_F(SessionStateControllerImpl2Test, LockAndUnlock) {
  controller_->set_has_legacy_power_button_for_test(false);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(false);

  // We should initially be showing the screen locker containers, since they
  // also contain login-related windows that we want to show during the
  // logging-in animation.
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_RESTORE));

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));

  // Release the button before the lock timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_DROP));

  // Press the button and fire the lock timer.  We should request that the
  // screen be locked, but we should still be in the lift animation.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_EQ(0, delegate_->num_lock_requests());
  test_api_->trigger_lock_timeout();
  EXPECT_EQ(1, delegate_->num_lock_requests());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));

  // Notify that we locked successfully.
  state_controller_->OnStartingLock();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));

  // Notify that the lock window is visible.  We should make it raise in.
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));

  // When we release the power button, the lock-to-shutdown timer should be
  // stopped.
  EXPECT_TRUE(test_api_->lock_to_shutdown_timer_is_running());
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Notify that the screen has been unlocked.  We should show the
  // non-screen-locker windows.
  state_controller_->OnLockStateChanged(false);
  shell_delegate_->UnlockScreen();
  bool called = false;
  base::Closure closure = base::Bind(&CheckCalledCallback, &called);
  state_controller_->OnLockScreenHide(closure);
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_LIFT));
  EXPECT_TRUE(called);
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_DROP));
}

// Hold the power button down from the unlocked state to eventual shutdown.
TEST_F(SessionStateControllerImpl2Test, LockToShutdown) {
  controller_->set_has_legacy_power_button_for_test(false);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(false);

  // Hold the power button and lock the screen.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  test_api_->trigger_lock_timeout();
  state_controller_->OnStartingLock();
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();

  // When the lock-to-shutdown timeout fires, we should start the shutdown
  // timer.
  EXPECT_TRUE(test_api_->lock_to_shutdown_timer_is_running());
  test_api_->trigger_lock_to_shutdown_timeout();
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));

  // Fire the shutdown timeout and check that we request shutdown.
  test_api_->trigger_shutdown_timeout();
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}


// Hold the power button down from the unlocked state to eventual shutdown,
// then release the button while system does locking.
TEST_F(SessionStateControllerImpl2Test, CancelLockToShutdown) {
  controller_->set_has_legacy_power_button_for_test(false);

  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(false);

  // Hold the power button and lock the screen.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  test_api_->trigger_lock_timeout();
  state_controller_->OnStartingLock();

  // Power button is released while system attempts to lock.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();

  EXPECT_FALSE(state_controller_->ShutdownRequested());
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
}

// Test that we handle the case where lock requests are ignored.
TEST_F(SessionStateControllerImpl2Test, Lock) {
  // We require animations to have a duration for this test.
  ui::LayerAnimator::set_disable_animations_for_test(false);

  controller_->set_has_legacy_power_button_for_test(false);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(false);

  // Hold the power button and lock the screen.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));
  test_api_->trigger_lock_timeout();
  EXPECT_EQ(1, delegate_->num_lock_requests());
  EXPECT_TRUE(test_api_->lock_fail_timer_is_running());

  // We shouldn't start the lock-to-shutdown timer until the screen has actually
  // been locked.
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Act as if the request timed out.  We should restore the windows.
  test_api_->trigger_lock_fail_timeout();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_DROP));
}

// Test the basic operation of the lock button.
TEST_F(SessionStateControllerImpl2Test, LockButtonBasic) {
  controller_->set_has_legacy_power_button_for_test(false);
  // The lock button shouldn't do anything if we aren't logged in.
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller_->OnLockStateChanged(false);
  SetUserLoggedIn(false);
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  EXPECT_EQ(0, delegate_->num_lock_requests());

  // Ditto for when we're logged in as a guest.
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_GUEST);
  SetUserLoggedIn(true);
  SetCanLockScreen(false);
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  EXPECT_EQ(0, delegate_->num_lock_requests());

  // If we're logged in as a regular user, we should start the lock timer and
  // the pre-lock animation.
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  SetCanLockScreen(true);
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));

  // If the button is released immediately, we shouldn't lock the screen.
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_DROP));
  EXPECT_EQ(0, delegate_->num_lock_requests());

  // Press the button again and let the lock timeout fire.  We should request
  // that the screen be locked.
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  test_api_->trigger_lock_timeout();
  EXPECT_EQ(1, delegate_->num_lock_requests());

  // Pressing the lock button while we have a pending lock request shouldn't do
  // anything.
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());

  // Pressing the button also shouldn't do anything after the screen is locked.
  state_controller_->OnStartingLock();
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
}

// Test that the power button takes priority over the lock button.
TEST_F(SessionStateControllerImpl2Test, PowerButtonPreemptsLockButton) {
  controller_->set_has_legacy_power_button_for_test(false);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(false);

  // While the lock button is down, hold the power button.
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());

  // The lock timer shouldn't be stopped when the lock button is released.
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());

  // Now press the power button first and then the lock button.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());

  // Releasing the power button should stop the lock timer.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
}

// When the screen is locked without going through the usual power-button
// slow-close path (e.g. via the wrench menu), test that we still show the
// fast-close animation.
TEST_F(SessionStateControllerImpl2Test, LockWithoutButton) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnStartingLock();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS |
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_LIFT));
}

// When we hear that the process is exiting but we haven't had a chance to
// display an animation, we should just blank the screen.
TEST_F(SessionStateControllerImpl2Test, ShutdownWithoutButton) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnAppTerminating();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllContainersMask,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());
}

// Test that we display the fast-close animation and shut down when we get an
// outside request to shut down (e.g. from the login or lock screen).
TEST_F(SessionStateControllerImpl2Test, RequestShutdownFromLoginScreen) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller_->OnLockStateChanged(false);
  SetUserLoggedIn(false);
  state_controller_->RequestShutdown();
  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(SessionStateControllerImpl2Test, RequestShutdownFromLockScreen) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();
  state_controller_->RequestShutdown();
  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(SessionStateControllerImpl2Test,
       RequestAndCancelShutdownFromLockScreen) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();

  // Press the power button and check that we start the shutdown timer.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));

  // Release the power button before the shutdown timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->RootWindowIsAnimated(
          internal::SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS));
}

// Test that we ignore power button presses when the screen is turned off.
TEST_F(SessionStateControllerImpl2Test, IgnorePowerButtonIfScreenIsOff) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);

  // When the screen brightness is at 0%, we shouldn't do anything in response
  // to power button presses.
  controller_->OnScreenBrightnessChanged(0.0);
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());

  // After increasing the brightness to 10%, we should start the timer like
  // usual.
  controller_->OnScreenBrightnessChanged(10.0);
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
}

}  // namespace test
}  // namespace ash
