// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/session_state_controller.h"
#include "ash/wm/session_state_controller_impl.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
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
}

// Fake implementation of PowerButtonControllerDelegate that just logs requests
// to lock the screen and shut down the device.
class TestPowerButtonControllerDelegate :
    public SessionStateControllerDelegate {
 public:
  TestPowerButtonControllerDelegate()
      : num_lock_requests_(0),
        num_shutdown_requests_(0) {}

  int num_lock_requests() const { return num_lock_requests_; }
  int num_shutdown_requests() const { return num_shutdown_requests_; }

  // PowerButtonControllerDelegate implementation.
  virtual void RequestLockScreen() OVERRIDE {
    num_lock_requests_++;
  }
  virtual void RequestShutdown() OVERRIDE {
    num_shutdown_requests_++;
  }

 private:
  int num_lock_requests_;
  int num_shutdown_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestPowerButtonControllerDelegate);
};

class PowerButtonControllerTest : public AshTestBase {
 public:
  PowerButtonControllerTest() : controller_(NULL), delegate_(NULL) {}
  virtual ~PowerButtonControllerTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshDisableNewLockAnimations);

    AshTestBase::SetUp();
    delegate_ = new TestPowerButtonControllerDelegate;
    controller_ = Shell::GetInstance()->power_button_controller();
    state_controller_ = static_cast<SessionStateControllerImpl*>(
        Shell::GetInstance()->session_state_controller());
    state_controller_->SetDelegate(delegate_);  // transfers ownership
    test_api_.reset(new SessionStateControllerImpl::TestApi(state_controller_));
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
  SessionStateControllerImpl* state_controller_;  // not owned
  TestPowerButtonControllerDelegate* delegate_;  // not owned
  TestShellDelegate* shell_delegate_;  // not owned

  scoped_ptr<SessionStateControllerImpl::TestApi> test_api_;
  scoped_ptr<internal::SessionStateAnimator::TestApi> animator_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerButtonControllerTest);
};

// Test the lock-to-shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should lock immediately the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
TEST_F(PowerButtonControllerTest, LegacyLockAndShutDown) {
  controller_->set_has_legacy_power_button_for_test(true);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(false);

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE));
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_EQ(1, delegate_->num_lock_requests());

  // Notify that we locked successfully.
  state_controller_->OnStartingLock();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_FULL_CLOSE));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));

  // Notify that the lock window is visible.  We should make it fade in.
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllLockScreenContainersMask,
          internal::SessionStateAnimator::ANIMATION_FADE_IN));

  // We shouldn't progress towards the shutdown state, however.
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());

  // Hold the button again and check that we start shutting down.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_EQ(0, NumShutdownRequests());

  // Previously we're checking that the all containers group was animated which
  // was in fact checking that
  // 1. All user session containers have transform (including wallpaper).
  //    They're in this state after lock.
  // 2. Screen locker and related containers are in fact animating
  //    (as shutdown is in progress).
  // With http://crbug.com/144737 we no longer animate user session wallpaper
  // during lock so it makes sense only to check that screen lock and related
  // containers are animated during shutdown.
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllLockScreenContainersMask,
          internal::SessionStateAnimator::ANIMATION_FULL_CLOSE));
  // Make sure a mouse move event won't show the cursor.
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're not logged in on an unofficial system.
TEST_F(PowerButtonControllerTest, LegacyNotLoggedIn) {
  controller_->set_has_legacy_power_button_for_test(true);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller_->OnLockStateChanged(false);
  SetUserLoggedIn(false);
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're logged in as a guest on an unofficial system.
TEST_F(PowerButtonControllerTest, LegacyGuest) {
  controller_->set_has_legacy_power_button_for_test(true);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_GUEST);
  state_controller_->OnLockStateChanged(false);
  SetCanLockScreen(false);
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// When we hold the power button while the user isn't logged in, we should shut
// down the machine directly.
TEST_F(PowerButtonControllerTest, ShutdownWhenNotLoggedIn) {
  controller_->set_has_legacy_power_button_for_test(false);
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller_->OnLockStateChanged(false);
  SetUserLoggedIn(false);

  // Press the power button and check that we start the shutdown timer.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllContainersMask,
          internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE));

  // Release the power button before the shutdown timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllContainersMask,
          internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE));

  // Press the button again and make the shutdown timeout fire this time.
  // Check that we start the timer for actually requesting the shutdown.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  test_api_->trigger_shutdown_timeout();
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LAUNCHER |
              internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllLockScreenContainersMask,
          internal::SessionStateAnimator::ANIMATION_FULL_CLOSE));

  // When the timout fires, we should request a shutdown.
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we lock the screen and deal with unlocking correctly.
TEST_F(PowerButtonControllerTest, LockAndUnlock) {
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
  // start scaling the non-screen-locker containers.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE));

  // Release the button before the lock timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE));

  // Press the button and fire the lock timer.  We should request that the
  // screen be locked, but we should still be in the slow-close animation.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_EQ(0, delegate_->num_lock_requests());
  test_api_->trigger_lock_timeout();
  EXPECT_EQ(1, delegate_->num_lock_requests());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE));

  // Notify that we locked successfully.
  state_controller_->OnStartingLock();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LAUNCHER,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_FULL_CLOSE));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));

  // Notify that the lock window is visible.  We should make it fade in.
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllLockScreenContainersMask,
          internal::SessionStateAnimator::ANIMATION_FADE_IN));

  // When we release the power button, the lock-to-shutdown timer should be
  // stopped.
  EXPECT_TRUE(test_api_->lock_to_shutdown_timer_is_running());
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Notify that the screen has been unlocked.  We should show the
  // non-screen-locker windows.
  state_controller_->OnLockStateChanged(false);
  shell_delegate_->UnlockScreen();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::DESKTOP_BACKGROUND |
          internal::SessionStateAnimator::LAUNCHER |
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_RESTORE));
}

// Hold the power button down from the unlocked state to eventual shutdown.
TEST_F(PowerButtonControllerTest, LockToShutdown) {
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
          internal::SessionStateAnimator::kAllContainersMask,
          internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE));

  // Fire the shutdown timeout and check that we request shutdown.
  test_api_->trigger_shutdown_timeout();
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}


// Hold the power button down from the unlocked state to eventual shutdown,
// then release the button while system does locking.
TEST_F(PowerButtonControllerTest, CancelLockToShutdown) {
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
TEST_F(PowerButtonControllerTest, LockFail) {
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
          internal::SessionStateAnimator::DESKTOP_BACKGROUND |
          internal::SessionStateAnimator::LAUNCHER |
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_RESTORE));
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
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_RESTORE));
}

// Test the basic operation of the lock button.
TEST_F(PowerButtonControllerTest, LockButtonBasic) {
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
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE));

  // If the button is released immediately, we shouldn't lock the screen.
  controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE));
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
TEST_F(PowerButtonControllerTest, PowerButtonPreemptsLockButton) {
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
TEST_F(PowerButtonControllerTest, LockWithoutButton) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnStartingLock();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_FULL_CLOSE));
}

// When we hear that the process is exiting but we haven't had a chance to
// display an animation, we should just blank the screen.
TEST_F(PowerButtonControllerTest, ShutdownWithoutButton) {
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
TEST_F(PowerButtonControllerTest, RequestShutdownFromLoginScreen) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller_->OnLockStateChanged(false);
  SetUserLoggedIn(false);
  state_controller_->RequestShutdown();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllLockScreenContainersMask,
          internal::SessionStateAnimator::ANIMATION_FULL_CLOSE));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(PowerButtonControllerTest, RequestShutdownFromLockScreen) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();
  state_controller_->RequestShutdown();
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
          internal::SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllLockScreenContainersMask,
          internal::SessionStateAnimator::ANIMATION_FULL_CLOSE));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(PowerButtonControllerTest, RequestAndCancelShutdownFromLockScreen) {
  state_controller_->OnLoginStateChanged(user::LOGGED_IN_USER);
  state_controller_->OnLockStateChanged(true);
  shell_delegate_->LockScreen();

  // Press the power button and check that we start the shutdown timer.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllContainersMask,
          internal::SessionStateAnimator::ANIMATION_PARTIAL_CLOSE));

  // Release the power button before the shutdown timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::kAllLockScreenContainersMask,
          internal::SessionStateAnimator::ANIMATION_UNDO_PARTIAL_CLOSE));
  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          internal::SessionStateAnimator::DESKTOP_BACKGROUND,
          internal::SessionStateAnimator::ANIMATION_RESTORE));
}

// Test that we ignore power button presses when the screen is turned off.
TEST_F(PowerButtonControllerTest, IgnorePowerButtonIfScreenIsOff) {
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
