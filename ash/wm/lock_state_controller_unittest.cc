// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_lock_state_controller_delegate.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ash/test/test_session_state_animator.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/session_state_animator.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time/time.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/size.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/types/display_constants.h"
#endif

namespace ash {
namespace test {
namespace {

bool cursor_visible() {
  return ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible();
}

void CheckCalledCallback(bool* flag) {
  if (flag)
    (*flag) = true;
}

}  // namespace

class LockStateControllerTest : public AshTestBase {
 public:
  LockStateControllerTest() : power_button_controller_(NULL),
                              lock_state_controller_(NULL),
                              lock_state_controller_delegate_(NULL),
                              test_animator_(NULL) {
  }
  virtual ~LockStateControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    scoped_ptr<LockStateControllerDelegate> lock_state_controller_delegate(
        lock_state_controller_delegate_ = new TestLockStateControllerDelegate);
    test_animator_ = new TestSessionStateAnimator;

    lock_state_controller_ = Shell::GetInstance()->lock_state_controller();
    lock_state_controller_->SetDelegate(lock_state_controller_delegate.Pass());
    lock_state_controller_->set_animator_for_test(test_animator_);

    test_api_.reset(new LockStateController::TestApi(lock_state_controller_));

    power_button_controller_ = Shell::GetInstance()->power_button_controller();
    session_state_delegate_ = Shell::GetInstance()->session_state_delegate();

    shell_delegate_ = reinterpret_cast<TestShellDelegate*>(
        ash::Shell::GetInstance()->delegate());
  }

 protected:
  void GenerateMouseMoveEvent() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.MoveMouseTo(10, 10);
  }

  int NumShutdownRequests() {
    return lock_state_controller_delegate_->num_shutdown_requests() +
        shell_delegate_->num_exit_requests();
  }

  void Advance(SessionStateAnimator::AnimationSpeed speed) {
    test_animator_->Advance(test_animator_->GetDuration(speed));
  }

  void AdvancePartially(SessionStateAnimator::AnimationSpeed speed,
                        float factor) {
    base::TimeDelta duration = test_animator_->GetDuration(speed);
    base::TimeDelta partial_duration =
        base::TimeDelta::FromInternalValue(duration.ToInternalValue() * factor);
    test_animator_->Advance(partial_duration);
  }

  void ExpectPreLockAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
    EXPECT_TRUE(test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationRunning() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationRunning");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationCancel() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationCancel");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_UNDO_LIFT));
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPreLockAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectPreLockAnimationFinished");
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  }

  void ExpectPostLockAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectPostLockAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
  }

  void ExpectPostLockAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectPostLockAnimationFinished");
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectUnlockBeforeUIDestroyedAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectUnlockBeforeUIDestroyedAnimationFinished");
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockAfterUIDestroyedAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectUnlockAfterUIDestroyedAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_DROP));
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockAfterUIDestroyedAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectUnlockAfterUIDestroyedAnimationFinished");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_DROP));
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectShutdownAnimationStarted() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationStarted");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::ROOT_CONTAINER,
            SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationFinished() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationFinished");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::ROOT_CONTAINER,
            SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationCancel() {
    SCOPED_TRACE("Failure in ExpectShutdownAnimationCancel");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::ROOT_CONTAINER,
            SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectBackgroundIsShowing() {
    SCOPED_TRACE("Failure in ExpectBackgroundIsShowing");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::DESKTOP_BACKGROUND,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectBackgroundIsHiding() {
    SCOPED_TRACE("Failure in ExpectBackgroundIsHiding");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::DESKTOP_BACKGROUND,
            SessionStateAnimator::ANIMATION_FADE_OUT));
  }

  void ExpectRestoringBackgroundVisibility() {
    SCOPED_TRACE("Failure in ExpectRestoringBackgroundVisibility");
    EXPECT_LT(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(
        test_animator_->AreContainersAnimated(
            SessionStateAnimator::DESKTOP_BACKGROUND,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockedState() {
    SCOPED_TRACE("Failure in ExpectUnlockedState");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_FALSE(session_state_delegate_->IsScreenLocked());
  }

  void ExpectLockedState() {
    SCOPED_TRACE("Failure in ExpectLockedState");
    EXPECT_EQ(0u, test_animator_->GetAnimationCount());
    EXPECT_TRUE(session_state_delegate_->IsScreenLocked());
  }

  void HideBackground() {
    test_animator_->HideBackground();
  }

  void PressPowerButton() {
    power_button_controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleasePowerButton() {
    power_button_controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  }

  void PressLockButton() {
    power_button_controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleaseLockButton() {
    power_button_controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  }

  void PressVolumeDown() {
    GetEventGenerator().PressKey(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  }

  void ReleaseVolumeDown() {
    GetEventGenerator().ReleaseKey(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  }

  void SystemLocks() {
    lock_state_controller_->OnLockStateChanged(true);
    session_state_delegate_->LockScreen();
  }

  void SuccessfulAuthentication(bool* call_flag) {
    base::Closure closure = base::Bind(&CheckCalledCallback, call_flag);
    lock_state_controller_->OnLockScreenHide(closure);
  }

  void SystemUnlocks() {
    lock_state_controller_->OnLockStateChanged(false);
    session_state_delegate_->UnlockScreen();
  }

  void EnableMaximizeMode(bool enable) {
    Shell::GetInstance()->maximize_mode_controller()->
        EnableMaximizeModeWindowManager(enable);
  }

  void Initialize(bool legacy_button, user::LoginStatus status) {
    power_button_controller_->set_has_legacy_power_button_for_test(
        legacy_button);
    lock_state_controller_->OnLoginStateChanged(status);
    SetUserLoggedIn(status != user::LOGGED_IN_NONE);
    if (status == user::LOGGED_IN_GUEST)
      SetCanLockScreen(false);
    lock_state_controller_->OnLockStateChanged(false);
  }

  PowerButtonController* power_button_controller_;  // not owned
  LockStateController* lock_state_controller_;  // not owned
  TestLockStateControllerDelegate*
      lock_state_controller_delegate_;  // not owned
  TestSessionStateAnimator* test_animator_;  // not owned
  SessionStateDelegate* session_state_delegate_;  // not owned
  scoped_ptr<LockStateController::TestApi> test_api_;
  TestShellDelegate* shell_delegate_;  // not owned

 private:
  DISALLOW_COPY_AND_ASSIGN(LockStateControllerTest);
};

// Test the lock-to-shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should lock immediately the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
TEST_F(LockStateControllerTest, LegacyLockAndShutDown) {
  Initialize(true, user::LOGGED_IN_USER);

  ExpectUnlockedState();

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  PressPowerButton();

  EXPECT_FALSE(test_api_->is_lock_cancellable());

  ExpectPreLockAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectPreLockAnimationFinished();

  EXPECT_EQ(1, lock_state_controller_delegate_->num_lock_requests());

  // Notify that we locked successfully.
  lock_state_controller_->OnStartingLock();
  EXPECT_EQ(0u, test_animator_->GetAnimationCount());

  SystemLocks();

  ExpectPostLockAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectPostLockAnimationFinished();

  // We shouldn't progress towards the shutdown state, however.
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());

  ReleasePowerButton();

  // Hold the button again and check that we start shutting down.
  PressPowerButton();

  ExpectShutdownAnimationStarted();

  EXPECT_EQ(0, NumShutdownRequests());
  // Make sure a mouse move event won't show the cursor.
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're not logged in on an unofficial system.
TEST_F(LockStateControllerTest, LegacyNotLoggedIn) {
  Initialize(true, user::LOGGED_IN_NONE);

  PressPowerButton();
  ExpectShutdownAnimationStarted();

  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're logged in as a guest on an unofficial system.
TEST_F(LockStateControllerTest, LegacyGuest) {
  Initialize(true, user::LOGGED_IN_GUEST);

  PressPowerButton();
  ExpectShutdownAnimationStarted();

  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// When we hold the power button while the user isn't logged in, we should shut
// down the machine directly.
TEST_F(LockStateControllerTest, ShutdownWhenNotLoggedIn) {
  Initialize(false, user::LOGGED_IN_NONE);

  // Press the power button and check that we start the shutdown timer.
  PressPowerButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  ExpectShutdownAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN, 0.5f);

  // Release the power button before the shutdown timer fires.
  ReleasePowerButton();

  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  ExpectShutdownAnimationCancel();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_REVERT, 0.5f);

  // Press the button again and make the shutdown timeout fire this time.
  // Check that we start the timer for actually requesting the shutdown.
  PressPowerButton();

  EXPECT_TRUE(test_api_->shutdown_timer_is_running());

  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectShutdownAnimationFinished();
  test_api_->trigger_shutdown_timeout();

  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());

  // When the timout fires, we should request a shutdown.
  test_api_->trigger_real_shutdown_timeout();

  EXPECT_EQ(1, NumShutdownRequests());
}

// Test that we lock the screen and deal with unlocking correctly.
TEST_F(LockStateControllerTest, LockAndUnlock) {
  Initialize(false, user::LOGGED_IN_USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_lock_cancellable());
  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());

  test_animator_->CompleteAllAnimations(true);
  ExpectPreLockAnimationFinished();

  EXPECT_EQ(1, lock_state_controller_delegate_->num_lock_requests());

  // Notify that we locked successfully.
  lock_state_controller_->OnStartingLock();
  // We had that animation already.
  EXPECT_EQ(0u, test_animator_->GetAnimationCount());

  SystemLocks();

  ExpectPostLockAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectPostLockAnimationFinished();

  // When we release the power button, the lock-to-shutdown timer should be
  // stopped.
  ExpectLockedState();
  EXPECT_TRUE(test_api_->lock_to_shutdown_timer_is_running());
  ReleasePowerButton();
  ExpectLockedState();
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Notify that the screen has been unlocked.  We should show the
  // non-screen-locker windows.
  bool called = false;
  SuccessfulAuthentication(&called);

  ExpectUnlockBeforeUIDestroyedAnimationStarted();
  EXPECT_FALSE(called);
  test_animator_->CompleteAllAnimations(true);
  ExpectUnlockBeforeUIDestroyedAnimationFinished();

  EXPECT_TRUE(called);

  SystemUnlocks();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  test_animator_->CompleteAllAnimations(true);
  ExpectUnlockAfterUIDestroyedAnimationFinished();

  ExpectUnlockedState();
}

// Test that we deal with cancelling lock correctly.
TEST_F(LockStateControllerTest, LockAndCancel) {
  Initialize(false, user::LOGGED_IN_USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_lock_cancellable());

  // forward only half way through
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleasePowerButton();

  ExpectPreLockAnimationCancel();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockedState();
  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());
}

// Test that we deal with cancelling lock correctly.
TEST_F(LockStateControllerTest, LockAndCancelAndLockAgain) {
  Initialize(false, user::LOGGED_IN_USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_lock_cancellable());

  // forward only half way through
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS,
                   0.5f);

  PressPowerButton();
  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_lock_cancellable());

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.6f);

  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.6f);
  ExpectPreLockAnimationFinished();
  EXPECT_EQ(1, lock_state_controller_delegate_->num_lock_requests());
}

// Hold the power button down from the unlocked state to eventual shutdown.
TEST_F(LockStateControllerTest, LockToShutdown) {
  Initialize(false, user::LOGGED_IN_USER);

  // Hold the power button and lock the screen.
  PressPowerButton();
  EXPECT_TRUE(test_api_->is_animating_lock());

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  SystemLocks();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  // When the lock-to-shutdown timeout fires, we should start the shutdown
  // timer.
  EXPECT_TRUE(test_api_->lock_to_shutdown_timer_is_running());

  test_api_->trigger_lock_to_shutdown_timeout();

  ExpectShutdownAnimationStarted();
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());

  // Fire the shutdown timeout and check that we request shutdown.
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectShutdownAnimationFinished();
  test_api_->trigger_shutdown_timeout();

  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, NumShutdownRequests());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// Hold the power button down from the unlocked state to eventual shutdown,
// then release the button while system does locking.
TEST_F(LockStateControllerTest, CancelLockToShutdown) {
  Initialize(false, user::LOGGED_IN_USER);

  PressPowerButton();

  // Hold the power button and lock the screen.
  EXPECT_TRUE(test_api_->is_animating_lock());

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  SystemLocks();
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, 0.5f);

  // Power button is released while system attempts to lock.
  ReleasePowerButton();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  EXPECT_FALSE(lock_state_controller_->ShutdownRequested());
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
}

// TODO(bruthig): Investigate why this hangs on Windows 8 and whether it can be
// safely enabled on OS_WIN.
#ifndef OS_WIN
// Test that we handle the case where lock requests are ignored.
TEST_F(LockStateControllerTest, Lock) {
  Initialize(false, user::LOGGED_IN_USER);

  // Hold the power button and lock the screen.
  PressPowerButton();
  ExpectPreLockAnimationStarted();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  EXPECT_EQ(1, lock_state_controller_delegate_->num_lock_requests());
  EXPECT_TRUE(test_api_->lock_fail_timer_is_running());
  // We shouldn't start the lock-to-shutdown timer until the screen has actually
  // been locked and this was animated.
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Act as if the request timed out.
  EXPECT_DEATH(test_api_->trigger_lock_fail_timeout(), "");
}
#endif

// Test the basic operation of the lock button (not logged in).
TEST_F(LockStateControllerTest, LockButtonBasicNotLoggedIn) {
  // The lock button shouldn't do anything if we aren't logged in.
  Initialize(false, user::LOGGED_IN_NONE);

  PressLockButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());
}

// Test the basic operation of the lock button (guest).
TEST_F(LockStateControllerTest, LockButtonBasicGuest) {
  // The lock button shouldn't do anything when we're logged in as a guest.
  Initialize(false, user::LOGGED_IN_GUEST);

  PressLockButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());
}

// Test the basic operation of the lock button.
TEST_F(LockStateControllerTest, LockButtonBasic) {
  // If we're logged in as a regular user, we should start the lock timer and
  // the pre-lock animation.
  Initialize(false, user::LOGGED_IN_USER);

  PressLockButton();
  ExpectPreLockAnimationStarted();
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // If the button is released immediately, we shouldn't lock the screen.
  ReleaseLockButton();
  ExpectPreLockAnimationCancel();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  ExpectUnlockedState();
  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());

  // Press the button again and let the lock timeout fire.  We should request
  // that the screen be locked.
  PressLockButton();
  ExpectPreLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  EXPECT_EQ(1, lock_state_controller_delegate_->num_lock_requests());

  // Pressing the lock button while we have a pending lock request shouldn't do
  // anything.
  ReleaseLockButton();
  PressLockButton();
  ExpectPreLockAnimationFinished();
  ReleaseLockButton();

  // Pressing the button also shouldn't do anything after the screen is locked.
  SystemLocks();
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
  Initialize(false, user::LOGGED_IN_USER);

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
  Initialize(false, user::LOGGED_IN_USER);
  lock_state_controller_->OnStartingLock();

  ExpectPreLockAnimationStarted();
  EXPECT_FALSE(test_api_->is_lock_cancellable());
  EXPECT_LT(0u, test_animator_->GetAnimationCount());

  test_animator_->CompleteAllAnimations(true);
  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());
}

// When we hear that the process is exiting but we haven't had a chance to
// display an animation, we should just blank the screen.
TEST_F(LockStateControllerTest, ShutdownWithoutButton) {
  Initialize(false, user::LOGGED_IN_USER);
  lock_state_controller_->OnAppTerminating();

  EXPECT_TRUE(
      test_animator_->AreContainersAnimated(
          SessionStateAnimator::kAllNonRootContainersMask,
          SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());
}

// Test that we display the fast-close animation and shut down when we get an
// outside request to shut down (e.g. from the login or lock screen).
TEST_F(LockStateControllerTest, RequestShutdownFromLoginScreen) {
  Initialize(false, user::LOGGED_IN_NONE);

  lock_state_controller_->RequestShutdown();

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(LockStateControllerTest, RequestShutdownFromLockScreen) {
  Initialize(false, user::LOGGED_IN_USER);

  SystemLocks();

  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectPostLockAnimationFinished();

  lock_state_controller_->RequestShutdown();

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(LockStateControllerTest, RequestAndCancelShutdownFromLockScreen) {
  Initialize(false, user::LOGGED_IN_USER);

  SystemLocks();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectLockedState();

  // Press the power button and check that we start the shutdown timer.
  PressPowerButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());

  ExpectShutdownAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN, 0.5f);

  // Release the power button before the shutdown timer fires.
  ReleasePowerButton();

  EXPECT_FALSE(test_api_->shutdown_timer_is_running());

  ExpectShutdownAnimationCancel();

  Advance(SessionStateAnimator::ANIMATION_SPEED_REVERT_SHUTDOWN);
  ExpectLockedState();
}

// Test that we ignore power button presses when the screen is turned off.
TEST_F(LockStateControllerTest, IgnorePowerButtonIfScreenIsOff) {
  Initialize(false, user::LOGGED_IN_USER);

  // When the screen brightness is at 0%, we shouldn't do anything in response
  // to power button presses.
  power_button_controller_->OnScreenBrightnessChanged(0.0);
  PressPowerButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleasePowerButton();

  // After increasing the brightness to 10%, we should start the timer like
  // usual.
  power_button_controller_->OnScreenBrightnessChanged(10.0);
  PressPowerButton();
  EXPECT_TRUE(test_api_->is_animating_lock());
  ReleasePowerButton();
}

#if defined(OS_CHROMEOS)
TEST_F(LockStateControllerTest, HonorPowerButtonInDockedMode) {
  ScopedVector<const ui::DisplayMode> modes;
  modes.push_back(new ui::DisplayMode(gfx::Size(1, 1), false, 60.0f));

  // Create two outputs, the first internal and the second external.
  ui::DisplayConfigurator::DisplayStateList outputs;
  ui::DisplayConfigurator::DisplayState internal_output;
  ui::TestDisplaySnapshot internal_display;
  internal_display.set_type(ui::DISPLAY_CONNECTION_TYPE_INTERNAL);
  internal_display.set_modes(modes.get());
  internal_output.display = &internal_display;
  outputs.push_back(internal_output);

  ui::DisplayConfigurator::DisplayState external_output;
  ui::TestDisplaySnapshot external_display;
  external_display.set_type(ui::DISPLAY_CONNECTION_TYPE_HDMI);
  external_display.set_modes(modes.get());
  external_output.display = &external_display;
  outputs.push_back(external_output);

  // When all of the displays are turned off (e.g. due to user inactivity), the
  // power button should be ignored.
  power_button_controller_->OnScreenBrightnessChanged(0.0);
  static_cast<ui::TestDisplaySnapshot*>(outputs[0].display)
      ->set_current_mode(NULL);
  static_cast<ui::TestDisplaySnapshot*>(outputs[1].display)
      ->set_current_mode(NULL);
  power_button_controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleasePowerButton();

  // When the screen brightness is 0% but the external display is still turned
  // on (indicating either docked mode or the user having manually decreased the
  // brightness to 0%), the power button should still be handled.
  static_cast<ui::TestDisplaySnapshot*>(outputs[1].display)
      ->set_current_mode(modes[0]);
  power_button_controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_TRUE(test_api_->is_animating_lock());
  ReleasePowerButton();
}
#endif

// Test that hidden background appears and revers correctly on lock/cancel.
TEST_F(LockStateControllerTest, TestHiddenBackgroundLockCancel) {
  Initialize(false, user::LOGGED_IN_USER);
  HideBackground();

  ExpectUnlockedState();
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  ExpectBackgroundIsShowing();

  // Forward only half way through.
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();
  ExpectBackgroundIsHiding();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDO_MOVE_WINDOWS);

  // When the CancelPrelockAnimation sequence finishes it queues up a
  // restore background visibilty sequence when the background is hidden.
  ExpectRestoringBackgroundVisibility();

  Advance(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockedState();
}

// Test that hidden background appears and revers correctly on lock/unlock.
TEST_F(LockStateControllerTest, TestHiddenBackgroundLockUnlock) {
  Initialize(false, user::LOGGED_IN_USER);
  HideBackground();

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  ExpectBackgroundIsShowing();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  ExpectPreLockAnimationFinished();

  SystemLocks();

  ReleasePowerButton();

  ExpectPostLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPostLockAnimationFinished();

  ExpectLockedState();

  SuccessfulAuthentication(NULL);

  ExpectUnlockBeforeUIDestroyedAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished();

  SystemUnlocks();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  ExpectBackgroundIsHiding();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  // When the StartUnlockAnimationAfterUIDestroyed sequence finishes it queues
  // up a restore background visibilty sequence when the background is hidden.
  ExpectRestoringBackgroundVisibility();

  Advance(SessionStateAnimator::ANIMATION_SPEED_IMMEDIATE);

  ExpectUnlockAfterUIDestroyedAnimationFinished();

  ExpectUnlockedState();
}

TEST_F(LockStateControllerTest, Screenshot) {
  test::TestScreenshotDelegate* delegate = GetScreenshotDelegate();
  delegate->set_can_take_screenshot(true);

  EnableMaximizeMode(false);

  // Screenshot handling should not be active when not in maximize mode.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressVolumeDown();
  PressPowerButton();
  ReleasePowerButton();
  ReleaseVolumeDown();
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  EnableMaximizeMode(true);

  // Pressing power alone does not take a screenshot.
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Press & release volume then pressing power does not take a screenshot.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressVolumeDown();
  ReleaseVolumeDown();
  PressPowerButton();
  ReleasePowerButton();
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Pressing power and then volume does not take a screenshot.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressPowerButton();
  ReleasePowerButton();
  PressVolumeDown();
  ReleaseVolumeDown();
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Holding volume down and pressing power takes a screenshot.
  ASSERT_EQ(0, delegate->handle_take_screenshot_count());
  PressVolumeDown();
  PressPowerButton();
  ReleasePowerButton();
  ReleaseVolumeDown();
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
}

// Tests that a lock action is cancellable when quick lock is turned on and
// maximize mode is not active.
TEST_F(LockStateControllerTest, QuickLockWhileNotInMaximizeMode) {
  Initialize(false, user::LOGGED_IN_USER);
  power_button_controller_->set_enable_quick_lock_for_test(true);
  EnableMaximizeMode(false);

  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_animating_lock());
  EXPECT_TRUE(lock_state_controller_->CanCancelLockAnimation());

  ReleasePowerButton();

  EXPECT_EQ(0, lock_state_controller_delegate_->num_lock_requests());
}

// Tests that a lock action is not cancellable when quick lock is turned on and
// maximize mode is active.
TEST_F(LockStateControllerTest, QuickLockWhileInMaximizeMode) {
  Initialize(false, user::LOGGED_IN_USER);
  power_button_controller_->set_enable_quick_lock_for_test(true);
  EnableMaximizeMode(true);

  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_animating_lock());
  EXPECT_FALSE(lock_state_controller_->CanCancelLockAnimation());

  ReleasePowerButton();

  ExpectPreLockAnimationStarted();

  test_animator_->CompleteAllAnimations(true);
  EXPECT_EQ(1, lock_state_controller_delegate_->num_lock_requests());
}

}  // namespace test
}  // namespace ash
