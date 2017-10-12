// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/config.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shutdown_controller.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/power_button_test_base.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/wm/lock_state_controller_test_api.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/test_session_state_animator.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/display/manager/fake_display_snapshot.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

bool cursor_visible() {
  return Shell::Get()->cursor_manager()->IsCursorVisible();
}

void CheckCalledCallback(bool* flag) {
  if (flag)
    (*flag) = true;
}

// ShutdownController that tracks how many shutdown requests have been made.
class TestShutdownController : public ShutdownController {
 public:
  TestShutdownController() = default;
  ~TestShutdownController() override = default;

  int num_shutdown_requests() const { return num_shutdown_requests_; }

 private:
  // ShutdownController:
  void ShutDownOrReboot(ShutdownReason reason) override {
    num_shutdown_requests_++;
  }

  int num_shutdown_requests_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShutdownController);
};

}  // namespace

class LockStateControllerTest : public PowerButtonTestBase {
 public:
  LockStateControllerTest() = default;
  ~LockStateControllerTest() override = default;

  void SetUp() override {
    PowerButtonTestBase::SetUp();
    InitPowerButtonControllerMembers(false /* SendAccelerometerUpdate */);

    test_animator_ = new TestSessionStateAnimator;
    lock_state_controller_->set_animator_for_test(test_animator_);
    lock_state_test_api_->set_shutdown_controller(&test_shutdown_controller_);
  }

 protected:
  int NumShutdownRequests() {
    return test_shutdown_controller_.num_shutdown_requests();
  }

  void Advance(SessionStateAnimator::AnimationSpeed speed) {
    test_animator_->Advance(test_animator_->GetDuration(speed));
  }

  void AdvancePartially(SessionStateAnimator::AnimationSpeed speed,
                        float factor) {
    test_animator_->Advance(test_animator_->GetDuration(speed) * factor);
  }

  void ExpectPreLockAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
    EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationRunning() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationRunning");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationCancel() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationCancel");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_UNDO_LIFT));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPreLockAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationFinished");
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  }

  void ExpectPostLockAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectPostLockAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPostLockAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectPostLockAnimationFinished");
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectUnlockBeforeUIDestroyedAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectUnlockBeforeUIDestroyedAnimationFinished");
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockAfterUIDestroyedAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectUnlockAfterUIDestroyedAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_DROP));
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockAfterUIDestroyedAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectUnlockAfterUIDestroyedAnimationFinished");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        SessionStateAnimator::ANIMATION_DROP));
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::SHELF, SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectShutdownAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationFinished");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationCancel() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationCancel");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::ROOT_CONTAINER,
        SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectWallpaperIsShowing() {
    SCOPED_TRACE("Failure in ExpectWallpaperIsShowing");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectWallpaperIsHiding() {
    SCOPED_TRACE("Failure in ExpectWallpaperIsHiding");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_OUT));
  }

  void ExpectRestoringWallpaperVisibility() {
    SCOPED_TRACE("Failure in ExpectRestoringWallpaperVisibility");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(test_animator_->AreContainersAnimated(
        SessionStateAnimator::WALLPAPER,
        SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockedState() {
    SCOPED_TRACE("Failure in ExpectUnlockedState");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  }

  void ExpectLockedState() {
    SCOPED_TRACE("Failure in ExpectLockedState");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
  }

  void HideWallpaper() { test_animator_->HideWallpaper(); }

  void PressLockButton() {
    power_button_controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleaseLockButton() {
    power_button_controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  }

  void SuccessfulAuthentication(bool* call_flag) {
    base::Closure closure = base::Bind(&CheckCalledCallback, call_flag);
    lock_state_controller_->OnLockScreenHide(closure);
  }

  TestShutdownController test_shutdown_controller_;
  TestSessionStateAnimator* test_animator_ = nullptr;  // not owned

 private:
  DISALLOW_COPY_AND_ASSIGN(LockStateControllerTest);
};

// Test the lock-to-shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should lock immediately the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
TEST_F(LockStateControllerTest, LegacyLockAndShutDown) {
  Initialize(ButtonType::LEGACY, LoginStatus::USER);

  ExpectUnlockedState();

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  PressPowerButton();

  EXPECT_FALSE(lock_state_test_api_->is_lock_cancellable());

  ExpectPreLockAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectPreLockAnimationFinished();

  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());

  // Notify that we locked successfully.
  lock_state_controller_->OnStartingLock();
  EXPECT_EQ(0u, test_animator_->GetAnimationCount());

  LockScreen();

  ExpectPostLockAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectPostLockAnimationFinished();

  // We shouldn't progress towards the shutdown state, however.
  EXPECT_FALSE(lock_state_test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());

  ReleasePowerButton();

  // Hold the button again and check that we start shutting down.
  PressPowerButton();

  ExpectShutdownAnimationStarted();

  EXPECT_EQ(0, NumShutdownRequests());
  // Make sure a mouse move event won't show the cursor.
  GenerateMouseMoveEvent();
  // TODO: CursorManager not created in mash. http://crbug.com/631103.
  if (Shell::GetAshConfig() != Config::MASH)
    EXPECT_FALSE(cursor_visible());

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're not logged in on an unofficial system.
TEST_F(LockStateControllerTest, LegacyNotLoggedIn) {
  Initialize(ButtonType::LEGACY, LoginStatus::NOT_LOGGED_IN);

  PressPowerButton();
  ExpectShutdownAnimationStarted();

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're logged in as a guest on an unofficial system.
TEST_F(LockStateControllerTest, LegacyGuest) {
  Initialize(ButtonType::LEGACY, LoginStatus::GUEST);

  PressPowerButton();
  ExpectShutdownAnimationStarted();

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
}

// When we hold the power button while the user isn't logged in, we should shut
// down the machine directly.
TEST_F(LockStateControllerTest, ShutdownWhenNotLoggedIn) {
  Initialize(ButtonType::NORMAL, LoginStatus::NOT_LOGGED_IN);

  // Press the power button and check that we start the shutdown timer.
  PressPowerButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());
  ExpectShutdownAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN, 0.5f);

  // Release the power button before the shutdown timer fires.
  ReleasePowerButton();

  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
  ExpectShutdownAnimationCancel();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_REVERT, 0.5f);

  // Press the button again and make the shutdown timeout fire this time.
  // Check that we start the timer for actually requesting the shutdown.
  PressPowerButton();

  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());

  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectShutdownAnimationFinished();
  lock_state_test_api_->trigger_shutdown_timeout();

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());

  // When the timout fires, we should request a shutdown.
  lock_state_test_api_->trigger_real_shutdown_timeout();

  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we lock the screen and deal with unlocking correctly.
TEST_F(LockStateControllerTest, LockAndUnlock) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(lock_state_test_api_->is_lock_cancellable());
  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());

  test_animator_->CompleteAllAnimations(true);
  ExpectPreLockAnimationFinished();

  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());

  // Notify that we locked successfully.
  lock_state_controller_->OnStartingLock();
  // We had that animation already.
  EXPECT_EQ(0u, test_animator_->GetAnimationCount());

  LockScreen();

  ExpectPostLockAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectPostLockAnimationFinished();

  // When we release the power button, the lock-to-shutdown timer should be
  // stopped.
  ExpectLockedState();
  EXPECT_TRUE(lock_state_test_api_->lock_to_shutdown_timer_is_running());
  ReleasePowerButton();
  ExpectLockedState();
  EXPECT_FALSE(lock_state_test_api_->lock_to_shutdown_timer_is_running());

  // The backlights shouldn't be forced off when clamshell power button behavior
  // isn't explicitly requested.
  EXPECT_FALSE(power_button_controller_->TriggerDisplayOffTimerForTesting());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // Notify that the screen has been unlocked.  We should show the
  // non-screen-locker windows.
  bool called = false;
  SuccessfulAuthentication(&called);

  ExpectUnlockBeforeUIDestroyedAnimationStarted();
  EXPECT_FALSE(called);
  test_animator_->CompleteAllAnimations(true);
  ExpectUnlockBeforeUIDestroyedAnimationFinished();

  EXPECT_TRUE(called);

  UnlockScreen();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectUnlockAfterUIDestroyedAnimationFinished();

  ExpectUnlockedState();
}

// Test that we deal with cancelling lock correctly.
TEST_F(LockStateControllerTest, LockAndCancel) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(lock_state_test_api_->is_lock_cancellable());

  // forward only half way through
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleasePowerButton();

  ExpectPreLockAnimationCancel();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockedState();
  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());
}

// Test that we deal with cancelling lock correctly.
TEST_F(LockStateControllerTest, LockAndCancelAndLockAgain) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(lock_state_test_api_->is_lock_cancellable());

  // forward only half way through
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
                   0.5f);

  PressPowerButton();
  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(lock_state_test_api_->is_lock_cancellable());

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.6f);

  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.6f);
  ExpectPreLockAnimationFinished();
  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());
}

// Hold the power button down from the unlocked state to eventual shutdown.
TEST_F(LockStateControllerTest, LockToShutdown) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  // Hold the power button and lock the screen.
  PressPowerButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  LockScreen();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  // When the lock-to-shutdown timeout fires, we should start the shutdown
  // timer.
  EXPECT_TRUE(lock_state_test_api_->lock_to_shutdown_timer_is_running());

  lock_state_test_api_->trigger_lock_to_shutdown_timeout();

  ExpectShutdownAnimationStarted();
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());

  // Fire the shutdown timeout and check that we request shutdown.
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectShutdownAnimationFinished();
  lock_state_test_api_->trigger_shutdown_timeout();

  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Hold the power button down from the unlocked state to eventual shutdown,
// then release the button while system does locking.
TEST_F(LockStateControllerTest, CancelLockToShutdown) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  PressPowerButton();

  // Hold the power button and lock the screen.
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  LockScreen();
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, 0.5f);

  // Power button is released while system attempts to lock.
  ReleasePowerButton();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  EXPECT_FALSE(lock_state_controller_->ShutdownRequested());
  EXPECT_FALSE(lock_state_test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());
}

// Test that we handle the case where lock requests are ignored.
TEST_F(LockStateControllerTest, Lock) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  // Hold the power button and lock the screen.
  PressPowerButton();
  ExpectPreLockAnimationStarted();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());
  EXPECT_TRUE(lock_state_test_api_->lock_fail_timer_is_running());
  // We shouldn't start the lock-to-shutdown timer until the screen has actually
  // been locked and this was animated.
  EXPECT_FALSE(lock_state_test_api_->lock_to_shutdown_timer_is_running());

  // Act as if the request timed out.
  EXPECT_DEATH(lock_state_test_api_->trigger_lock_fail_timeout(), "");
}

// Test the basic operation of the lock button (not logged in).
TEST_F(LockStateControllerTest, LockButtonBasicNotLoggedIn) {
  // The lock button shouldn't do anything if we aren't logged in.
  Initialize(ButtonType::NORMAL, LoginStatus::NOT_LOGGED_IN);

  PressLockButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());
}

// Test the basic operation of the lock button (guest).
TEST_F(LockStateControllerTest, LockButtonBasicGuest) {
  // The lock button shouldn't do anything when we're logged in as a guest.
  Initialize(ButtonType::NORMAL, LoginStatus::GUEST);

  PressLockButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());
}

// Test the basic operation of the lock button.
TEST_F(LockStateControllerTest, LockButtonBasic) {
  // If we're logged in as a regular user, we should start the lock timer and
  // the pre-lock animation.
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  PressLockButton();
  ExpectPreLockAnimationStarted();
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // If the button is released immediately, we shouldn't lock the screen.
  ReleaseLockButton();
  ExpectPreLockAnimationCancel();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  ExpectUnlockedState();
  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());

  // Press the button again and let the lock timeout fire.  We should request
  // that the screen be locked.
  PressLockButton();
  ExpectPreLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  EXPECT_EQ(1, session_manager_client_->request_lock_screen_call_count());

  // Pressing the lock button while we have a pending lock request shouldn't do
  // anything.
  ReleaseLockButton();
  PressLockButton();
  ExpectPreLockAnimationFinished();
  ReleaseLockButton();

  // Pressing the button also shouldn't do anything after the screen is locked.
  LockScreen();
  ExpectPostLockAnimationStarted();

  PressLockButton();
  ReleaseLockButton();
  ExpectPostLockAnimationStarted();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPostLockAnimationFinished();

  PressLockButton();
  ReleaseLockButton();
  ExpectPostLockAnimationFinished();
}

// Test that the power button takes priority over the lock button.
TEST_F(LockStateControllerTest, PowerButtonPreemptsLockButton) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  // While the lock button is down, hold the power button.
  PressLockButton();
  ExpectPreLockAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.1f);
  ExpectPreLockAnimationRunning();

  PressPowerButton();
  ExpectPreLockAnimationRunning();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.1f);
  ExpectPreLockAnimationRunning();

  // The lock timer shouldn't be stopped when the lock button is released.
  ReleaseLockButton();
  ExpectPreLockAnimationRunning();

  ReleasePowerButton();
  ExpectPreLockAnimationCancel();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockedState();

  // Now press the power button first and then the lock button.
  PressPowerButton();
  ExpectPreLockAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.1f);

  PressLockButton();
  ExpectPreLockAnimationRunning();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.1f);

  // Releasing the power button should stop the lock timer.
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.1f);

  ReleaseLockButton();
  ExpectPreLockAnimationCancel();
}

// When the screen is locked without going through the usual power-button
// slow-close path (e.g. via the wrench menu), test that we still show the
// fast-close animation.
TEST_F(LockStateControllerTest, LockWithoutButton) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  lock_state_controller_->OnStartingLock();

  ExpectPreLockAnimationStarted();
  EXPECT_FALSE(lock_state_test_api_->is_lock_cancellable());
  EXPECT_LT(0u, test_animator_->GetAnimationCount());

  test_animator_->CompleteAllAnimations(true);
  EXPECT_EQ(0, session_manager_client_->request_lock_screen_call_count());
}

// When we hear that the process is exiting but we haven't had a chance to
// display an animation, we should just blank the screen.
TEST_F(LockStateControllerTest, ShutdownWithoutButton) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  lock_state_controller_->OnChromeTerminating();

  EXPECT_TRUE(test_animator_->AreContainersAnimated(
      SessionStateAnimator::kAllNonRootContainersMask,
      SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  GenerateMouseMoveEvent();
  // TODO: CursorManager not created in mash. http://crbug.com/631103.
  if (Shell::GetAshConfig() != Config::MASH)
    EXPECT_FALSE(cursor_visible());
}

// Test that we display the fast-close animation and shut down when we get an
// outside request to shut down (e.g. from the login or lock screen).
TEST_F(LockStateControllerTest, RequestShutdownFromLoginScreen) {
  Initialize(ButtonType::NORMAL, LoginStatus::NOT_LOGGED_IN);

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  // TODO: CursorManager not created in mash. http://crbug.com/631103.
  if (Shell::GetAshConfig() != Config::MASH)
    EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(LockStateControllerTest, RequestShutdownFromLockScreen) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  LockScreen();

  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectPostLockAnimationFinished();

  lock_state_controller_->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  // TODO: CursorManager not created in mash. http://crbug.com/631103.
  if (Shell::GetAshConfig() != Config::MASH)
    EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(lock_state_test_api_->real_shutdown_timer_is_running());
  lock_state_test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(LockStateControllerTest, RequestAndCancelShutdownFromLockScreen) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  LockScreen();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectLockedState();

  // Press the power button and check that we start the shutdown timer.
  PressPowerButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  EXPECT_TRUE(lock_state_test_api_->shutdown_timer_is_running());

  ExpectShutdownAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN, 0.5f);

  // Release the power button before the shutdown timer fires.
  ReleasePowerButton();

  EXPECT_FALSE(lock_state_test_api_->shutdown_timer_is_running());

  ExpectShutdownAnimationCancel();

  Advance(SessionStateAnimator::ANIMATION_SPEED_REVERT_SHUTDOWN);
  ExpectLockedState();
}

// Test that we ignore power button presses when the screen is turned off.
TEST_F(LockStateControllerTest, IgnorePowerButtonIfScreenIsOff) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  // When the screen brightness is at 0%, we shouldn't do anything in response
  // to power button presses.
  power_manager_client_->SendBrightnessChanged(0, true);
  PressPowerButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  ReleasePowerButton();

  // After increasing the brightness to 10%, we should start the timer like
  // usual.
  power_manager_client_->SendBrightnessChanged(10, true);
  PressPowerButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  ReleasePowerButton();
}

TEST_F(LockStateControllerTest, HonorPowerButtonInDockedMode) {
  // Create two outputs, the first internal and the second external.
  display::DisplayConfigurator::DisplayStateList outputs;

  std::unique_ptr<display::DisplaySnapshot> internal_display =
      display::FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(gfx::Size(1, 1))
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .Build();
  outputs.push_back(internal_display.get());

  std::unique_ptr<display::DisplaySnapshot> external_display =
      display::FakeDisplaySnapshot::Builder()
          .SetId(456)
          .SetNativeMode(gfx::Size(1, 1))
          .SetType(display::DISPLAY_CONNECTION_TYPE_HDMI)
          .Build();
  outputs.push_back(external_display.get());

  // When all of the displays are turned off (e.g. due to user inactivity), the
  // power button should be ignored.
  power_manager_client_->SendBrightnessChanged(0, true);
  internal_display->set_current_mode(nullptr);
  external_display->set_current_mode(nullptr);
  power_button_controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_FALSE(lock_state_test_api_->is_animating_lock());
  ReleasePowerButton();

  // When the screen brightness is 0% but the external display is still turned
  // on (indicating either docked mode or the user having manually decreased the
  // brightness to 0%), the power button should still be handled.
  external_display->set_current_mode(external_display->modes().back().get());
  power_button_controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_TRUE(lock_state_test_api_->is_animating_lock());
  ReleasePowerButton();
}

// Test that hidden wallpaper appears and revers correctly on lock/cancel.
TEST_F(LockStateControllerTest, TestHiddenWallpaperLockCancel) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  HideWallpaper();

  ExpectUnlockedState();
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  ExpectWallpaperIsShowing();

  // Forward only half way through.
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();
  ExpectWallpaperIsHiding();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);

  // When the CancelPrelockAnimation sequence finishes it queues up a
  // restore wallpaper visibility sequence when the wallpaper is hidden.
  ExpectRestoringWallpaperVisibility();

  Advance(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockedState();
}

// Test that hidden wallpaper appears and revers correctly on lock/unlock.
TEST_F(LockStateControllerTest, TestHiddenWallpaperLockUnlock) {
  Initialize(ButtonType::NORMAL, LoginStatus::USER);
  HideWallpaper();

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  ExpectWallpaperIsShowing();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  ExpectPreLockAnimationFinished();

  LockScreen();

  ReleasePowerButton();

  ExpectPostLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPostLockAnimationFinished();

  ExpectLockedState();

  SuccessfulAuthentication(NULL);

  ExpectUnlockBeforeUIDestroyedAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished();

  UnlockScreen();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  ExpectWallpaperIsHiding();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  // When the StartUnlockAnimationAfterUIDestroyed sequence finishes it queues
  // up a restore wallpaper visibility sequence when the wallpaper is hidden.
  ExpectRestoringWallpaperVisibility();

  Advance(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockAfterUIDestroyedAnimationFinished();

  ExpectUnlockedState();
}

// Test that backlights are forced off shortly after the screen is locked when
// clamshell-style power button behavior is forced.
TEST_F(LockStateControllerTest, ClamshellDisplayOffAfterLock) {
  ForceClamshellPowerButton();
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  // If the power button isn't held long enough for the screen to be locked, the
  // backlights shouldn't be forced off.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_FALSE(power_button_controller_->TriggerDisplayOffTimerForTesting());

  // Now hold the power button long enough to lock the screen.
  PressPowerButton();
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  LockScreen();
  EXPECT_FALSE(power_button_controller_->TriggerDisplayOffTimerForTesting());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());

  // After releasing the power button, the display should still be on, but it
  // should be forced off after the display-off timer fires.
  ReleasePowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  EXPECT_TRUE(power_button_controller_->TriggerDisplayOffTimerForTesting());
  EXPECT_TRUE(power_manager_client_->backlights_forced_off());

  // Pressing the power button should turn the display back on.
  PressPowerButton();
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  ReleasePowerButton();

  // Now that the screen is already locked, the timer shouldn't be started if
  // the power button is pressed and released again.
  PressPowerButton();
  ReleasePowerButton();
  ASSERT_FALSE(power_button_controller_->TriggerDisplayOffTimerForTesting());
  ASSERT_FALSE(power_manager_client_->backlights_forced_off());
}

// Test that user activity prevents backlights from being forced off after the
// screen is locked.
TEST_F(LockStateControllerTest, CancelClamshellDisplayOffAfterLock) {
  ForceClamshellPowerButton();
  Initialize(ButtonType::NORMAL, LoginStatus::USER);

  // If a key is pressed shortly after locking, the display-off timer should be
  // stopped.
  PressPowerButton();
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  LockScreen();
  ReleasePowerButton();
  PressKey(ui::VKEY_A);
  ReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(power_button_controller_->TriggerDisplayOffTimerForTesting());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  UnlockScreen();

  // Mouse events should also stop the timer.
  PressPowerButton();
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  LockScreen();
  ReleasePowerButton();
  GenerateMouseMoveEvent();
  EXPECT_FALSE(power_button_controller_->TriggerDisplayOffTimerForTesting());
  EXPECT_FALSE(power_manager_client_->backlights_forced_off());
  UnlockScreen();
}

TEST_F(LockStateControllerTest, Screenshot) {
  TestScreenshotDelegate* delegate = GetScreenshotDelegate();
  delegate->set_can_take_screenshot(true);

  EnableTabletMode(false);

  // Screenshot handling should not be active when not in tablet mode.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressKey(ui::VKEY_VOLUME_DOWN);
  PressPowerButton();
  ReleasePowerButton();
  ReleaseKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  EnableTabletMode(true);

  // Pressing power alone does not take a screenshot.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Press & release volume then pressing power does not take a screenshot.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressKey(ui::VKEY_VOLUME_DOWN);
  ReleaseKey(ui::VKEY_VOLUME_DOWN);
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Pressing power and then volume does not take a screenshot.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressPowerButton();
  ReleasePowerButton();
  PressKey(ui::VKEY_VOLUME_DOWN);
  ReleaseKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Holding volume down and pressing power takes a screenshot.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressKey(ui::VKEY_VOLUME_DOWN);
  PressPowerButton();
  ReleasePowerButton();
  ReleaseKey(ui::VKEY_VOLUME_DOWN);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
}

// Tests that volume down key event is properly handled by power button
// controller when system tray bubble is shown. This is a regression test for
// crbug.com/765473.
TEST_F(LockStateControllerTest, VolumeDownKeyWithTrayBubbleShown) {
  TestScreenshotDelegate* delegate = GetScreenshotDelegate();
  delegate->set_can_take_screenshot(true);
  EnableTabletMode(true);

  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  // Simulate that pressing volume down key triggers volume bubble view.
  PressKey(ui::VKEY_VOLUME_DOWN);
  SystemTray* tray = GetPrimarySystemTray();
  tray->ShowDefaultView(BUBBLE_CREATE_NEW, false /* show_by_click */);
  // Release volume down key while tray bubble is still shown.
  ASSERT_TRUE(tray->IsSystemBubbleVisible());
  ReleaseKey(ui::VKEY_VOLUME_DOWN);
  tray->CloseBubble();
  EXPECT_FALSE(tray->IsSystemBubbleVisible());
  // Now press power button, verify that it doesn't do screenshot.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());
}

}  // namespace ash
