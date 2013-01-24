// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/session_state_controller_impl2.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
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
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/compositor_test_support.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {

using internal::SessionStateAnimator;

namespace test {

namespace {

bool cursor_visible() {
  return ash::Shell::GetInstance()->cursor_manager()->IsCursorVisible();
}

void CheckCalledCallback(bool* flag) {
  if (flag)
    (*flag) = true;
}

aura::Window* GetContainer(int container ) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();
  return Shell::GetContainer(root_window, container);
}

bool IsBackgroundHidden() {
  return !GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->
              IsVisible();
}

void ShowBackground() {
  ui::ScopedLayerAnimationSettings settings(
      GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->
          layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta());
  GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->Show();
}

void HideBackground() {
  ui::ScopedLayerAnimationSettings settings(
      GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->
          layer()->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta());
  GetContainer(internal::kShellWindowId_DesktopBackgroundContainer)->Hide();
}

} // namespace

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

  virtual void SetUp() OVERRIDE {
    CHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
        ash::switches::kAshDisableNewLockAnimations));

    AshTestBase::SetUp();

    // We would control animations in a fine way:
    ui::LayerAnimator::set_disable_animations_for_test(false);
    // TODO(antrim) : restore
    // animator_helper_ = ui::test::CreateLayerAnimatorHelperForTest();

    // Temporary disable animations so that observer is always called, and
    // no leaks happen during tests.
    ui::LayerAnimator::set_disable_animations_for_test(true);
    // TODO(antrim): once there is a way to mock time and run animations, make
    // sure that animations are finished even in simple tests.

    delegate_ = new TestSessionStateControllerDelegate;
    controller_ = Shell::GetInstance()->power_button_controller();
    state_controller_ = static_cast<SessionStateControllerImpl2*>(
        Shell::GetInstance()->session_state_controller());
    state_controller_->SetDelegate(delegate_);  // transfers ownership
    test_api_.reset(
        new SessionStateControllerImpl2::TestApi(state_controller_));
    animator_api_.reset(
        new SessionStateAnimator::TestApi(state_controller_->
            animator_.get()));
    shell_delegate_ = reinterpret_cast<TestShellDelegate*>(
        ash::Shell::GetInstance()->delegate());
  }

  void TearDown() {
    // TODO(antrim) : restore
    // animator_helper_->AdvanceUntilDone();
    AshTestBase::TearDown();
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

  void Advance(SessionStateAnimator::AnimationSpeed speed) {
    // TODO (antrim) : restore
    // animator_helper_->Advance(SessionStateAnimator::GetDuration(speed));
  }

  void AdvancePartially(SessionStateAnimator::AnimationSpeed speed,
                        float factor) {
// TODO (antrim) : restore
//  base::TimeDelta duration = SessionStateAnimator::GetDuration(speed);
//  base::TimeDelta partial_duration =
//      base::TimeDelta::FromInternalValue(duration.ToInternalValue() * factor);
//  animator_helper_->Advance(partial_duration);
  }

  void ExpectPreLockAnimationStarted() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
    EXPECT_TRUE(test_api_->is_animating_lock());
  }

  void ExpectPreLockAnimationCancel() {
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_DROP));
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectPreLockAnimationFinished() {
    //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_OUT));
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  }

  void ExpectPostLockAnimationStarted() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
  }

  void ExpectPastLockAnimationFinished() {
    //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_RAISE_TO_SCREEN));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationStarted() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockBeforeUIDestroyedAnimationFinished() {
    //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_LIFT));
  }

  void ExpectUnlockAfterUIDestroyedAnimationStarted() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_DROP));
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectUnlockAfterUIDestroyedAnimationFinished() {
    //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
            SessionStateAnimator::ANIMATION_DROP));
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::LAUNCHER,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectShutdownAnimationStarted() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->RootWindowIsAnimated(
            SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationFinished()  {
    //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->RootWindowIsAnimated(
            SessionStateAnimator::ANIMATION_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectShutdownAnimationCancel() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->RootWindowIsAnimated(
            SessionStateAnimator::ANIMATION_UNDO_GRAYSCALE_BRIGHTNESS));
  }

  void ExpectBackgroundIsShowing() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::DESKTOP_BACKGROUND,
            SessionStateAnimator::ANIMATION_FADE_IN));
  }

  void ExpectBackgroundIsHiding() {
    //TODO (antrim) : restore EXPECT_TRUE(animator_helper_->IsAnimating());
    EXPECT_TRUE(
        animator_api_->ContainersAreAnimated(
            SessionStateAnimator::DESKTOP_BACKGROUND,
            SessionStateAnimator::ANIMATION_FADE_OUT));
  }

  void ExpectUnlockedState() {
    //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());
    EXPECT_FALSE(shell_delegate_->IsScreenLocked());

    aura::Window::Windows containers;

    SessionStateAnimator::GetContainers(
        SessionStateAnimator::LAUNCHER |
        SessionStateAnimator::NON_LOCK_SCREEN_CONTAINERS,
        &containers);
    for (aura::Window::Windows::const_iterator it = containers.begin();
         it != containers.end(); ++it) {
      aura::Window* window = *it;
      ui::Layer* layer = window->layer();
      EXPECT_EQ(1.0f, layer->opacity());
      EXPECT_EQ(0.0f, layer->layer_brightness());
      EXPECT_EQ(0.0f, layer->layer_saturation());
      EXPECT_EQ(gfx::Transform(), layer->transform());
    }
  }

  void ExpectLockedState() {
    //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());
    EXPECT_TRUE(shell_delegate_->IsScreenLocked());

    aura::Window::Windows containers;

    SessionStateAnimator::GetContainers(
        SessionStateAnimator::LOCK_SCREEN_RELATED_CONTAINERS |
        SessionStateAnimator::LOCK_SCREEN_CONTAINERS,
        &containers);
    for (aura::Window::Windows::const_iterator it = containers.begin();
         it != containers.end(); ++it) {
      aura::Window* window = *it;
      ui::Layer* layer = window->layer();
      EXPECT_EQ(1.0f, layer->opacity());
      EXPECT_EQ(0.0f, layer->layer_brightness());
      EXPECT_EQ(0.0f, layer->layer_saturation());
      EXPECT_EQ(gfx::Transform(), layer->transform());
    }
  }

  void PressPowerButton() {
    controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void ReleasePowerButton() {
    controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void PressLockButton() {
    controller_->OnLockButtonEvent(true, base::TimeTicks::Now());
  }

  void ReleaseLockButton() {
    controller_->OnLockButtonEvent(false, base::TimeTicks::Now());
  }

  void SystemLocks() {
    state_controller_->OnLockStateChanged(true);
    shell_delegate_->LockScreen();
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void SuccessfulAuthentication(bool* call_flag) {
    base::Closure closure = base::Bind(&CheckCalledCallback, call_flag);
    state_controller_->OnLockScreenHide(closure);
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void SystemUnlocks() {
    state_controller_->OnLockStateChanged(false);
    shell_delegate_->UnlockScreen();
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void Initialize(bool legacy_button, user::LoginStatus status) {
    controller_->set_has_legacy_power_button_for_test(legacy_button);
    state_controller_->OnLoginStateChanged(status);
    SetUserLoggedIn(status != user::LOGGED_IN_NONE);
    if (status == user::LOGGED_IN_GUEST)
      SetCanLockScreen(false);
    state_controller_->OnLockStateChanged(false);
  }

  PowerButtonController* controller_;  // not owned
  SessionStateControllerImpl2* state_controller_;  // not owned
  TestSessionStateControllerDelegate* delegate_;  // not owned
  TestShellDelegate* shell_delegate_;  // not owned

  scoped_ptr<SessionStateControllerImpl2::TestApi> test_api_;
  scoped_ptr<SessionStateAnimator::TestApi> animator_api_;
  // TODO(antrim) : restore
//  scoped_ptr<ui::test::AnimationContainerTestHelper> animator_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateControllerImpl2Test);
};

// Test the lock-to-shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should lock immediately the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test, DISABLED_LegacyLockAndShutDown) {
  Initialize(true, user::LOGGED_IN_USER);

  ExpectUnlockedState();

  // We should request that the screen be locked immediately after seeing the
  // power button get pressed.
  PressPowerButton();

  ExpectPreLockAnimationStarted();

  EXPECT_FALSE(test_api_->is_lock_cancellable());

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  ExpectPreLockAnimationFinished();
  EXPECT_EQ(1, delegate_->num_lock_requests());

  // Notify that we locked successfully.
  state_controller_->OnStartingLock();
  // We had that animation already.
  //TODO (antrim) : restore
  //  EXPECT_FALSE(animator_helper_->IsAnimating());

  SystemLocks();

  ExpectPostLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPastLockAnimationFinished();

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
TEST_F(SessionStateControllerImpl2Test, LegacyNotLoggedIn) {
  Initialize(true, user::LOGGED_IN_NONE);

  PressPowerButton();
  ExpectShutdownAnimationStarted();

  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// Test that we start shutting down immediately if the power button is pressed
// while we're logged in as a guest on an unofficial system.
TEST_F(SessionStateControllerImpl2Test, LegacyGuest) {
  Initialize(true, user::LOGGED_IN_GUEST);

  PressPowerButton();
  ExpectShutdownAnimationStarted();

  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
}

// When we hold the power button while the user isn't logged in, we should shut
// down the machine directly.
TEST_F(SessionStateControllerImpl2Test, ShutdownWhenNotLoggedIn) {
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
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test, DISABLED_LockAndUnlock) {
  Initialize(false, user::LOGGED_IN_USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_lock_cancellable());
  EXPECT_EQ(0, delegate_->num_lock_requests());

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  ExpectPreLockAnimationFinished();

  EXPECT_EQ(1, delegate_->num_lock_requests());

  // Notify that we locked successfully.
  state_controller_->OnStartingLock();
  // We had that animation already.
  //TODO (antrim) : restore EXPECT_FALSE(animator_helper_->IsAnimating());

  SystemLocks();

  ExpectPostLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPastLockAnimationFinished();

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
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished();

  EXPECT_TRUE(called);

  SystemUnlocks();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockAfterUIDestroyedAnimationFinished();

  ExpectUnlockedState();
}

// Test that we deal with cancelling lock correctly.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test, DISABLED_LockAndCancel) {
  Initialize(false, user::LOGGED_IN_USER);

  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_lock_cancellable());

  // forward only half way through
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  gfx::Transform transform_before_button_released =
      GetContainer(internal::kShellWindowId_DefaultContainer)->
          layer()->transform();

  // Release the button before the lock timer fires.
  ReleasePowerButton();

  ExpectPreLockAnimationCancel();

  gfx::Transform transform_after_button_released =
      GetContainer(internal::kShellWindowId_DefaultContainer)->
          layer()->transform();
  // Expect no flickering, animation should proceed from mid-state.
  EXPECT_EQ(transform_before_button_released, transform_after_button_released);

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockedState();
  EXPECT_EQ(0, delegate_->num_lock_requests());
}

// Test that we deal with cancelling lock correctly.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test, DISABLED_LockAndCancelAndLockAgain) {
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

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS, 0.5f);

  PressPowerButton();
  ExpectPreLockAnimationStarted();
  EXPECT_TRUE(test_api_->is_lock_cancellable());

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.6f);

  EXPECT_EQ(0, delegate_->num_lock_requests());
  ExpectPreLockAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.6f);
  ExpectPreLockAnimationFinished();
  EXPECT_EQ(1, delegate_->num_lock_requests());
}

// Hold the power button down from the unlocked state to eventual shutdown.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test, DISABLED_LockToShutdown) {
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
TEST_F(SessionStateControllerImpl2Test, CancelLockToShutdown) {
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

  EXPECT_FALSE(state_controller_->ShutdownRequested());
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
}

// Test that we handle the case where lock requests are ignored.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test, DISABLED_Lock) {
  // We require animations to have a duration for this test.
  ui::LayerAnimator::set_disable_animations_for_test(false);

  Initialize(false, user::LOGGED_IN_USER);

  // Hold the power button and lock the screen.
  PressPowerButton();
  ExpectPreLockAnimationStarted();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  EXPECT_EQ(1, delegate_->num_lock_requests());
  EXPECT_TRUE(test_api_->lock_fail_timer_is_running());
  // We shouldn't start the lock-to-shutdown timer until the screen has actually
  // been locked and this was animated.
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Act as if the request timed out.  We should restore the windows.
  test_api_->trigger_lock_fail_timeout();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockAfterUIDestroyedAnimationFinished();
  ExpectUnlockedState();
}

// Test the basic operation of the lock button (not logged in).
TEST_F(SessionStateControllerImpl2Test, LockButtonBasicNotLoggedIn) {
  // The lock button shouldn't do anything if we aren't logged in.
  Initialize(false, user::LOGGED_IN_NONE);

  PressLockButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, delegate_->num_lock_requests());
}

// Test the basic operation of the lock button (guest).
TEST_F(SessionStateControllerImpl2Test, LockButtonBasicGuest) {
  // The lock button shouldn't do anything when we're logged in as a guest.
  Initialize(false, user::LOGGED_IN_GUEST);

  PressLockButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, delegate_->num_lock_requests());
}

// Test the basic operation of the lock button.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test, DISABLED_LockButtonBasic) {
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
  EXPECT_EQ(0, delegate_->num_lock_requests());

  // Press the button again and let the lock timeout fire.  We should request
  // that the screen be locked.
  PressLockButton();
  ExpectPreLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);
  EXPECT_EQ(1, delegate_->num_lock_requests());

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
  ExpectPastLockAnimationFinished();

  PressLockButton();
  ReleaseLockButton();
  ExpectPastLockAnimationFinished();
}

// Test that the power button takes priority over the lock button.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test,
    DISABLED_PowerButtonPreemptsLockButton) {
  Initialize(false, user::LOGGED_IN_USER);

  // While the lock button is down, hold the power button.
  PressLockButton();
  ExpectPreLockAnimationStarted();
  PressPowerButton();
  ExpectPreLockAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // The lock timer shouldn't be stopped when the lock button is released.
  ReleaseLockButton();
  ExpectPreLockAnimationStarted();
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockedState();

  // Now press the power button first and then the lock button.
  PressPowerButton();
  ExpectPreLockAnimationStarted();
  PressLockButton();
  ExpectPreLockAnimationStarted();

  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Releasing the power button should stop the lock timer.
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();
  ReleaseLockButton();
  ExpectPreLockAnimationCancel();
}

// When the screen is locked without going through the usual power-button
// slow-close path (e.g. via the wrench menu), test that we still show the
// fast-close animation.
TEST_F(SessionStateControllerImpl2Test, LockWithoutButton) {
  Initialize(false, user::LOGGED_IN_USER);
  state_controller_->OnStartingLock();

  ExpectPreLockAnimationStarted();
  EXPECT_FALSE(test_api_->is_lock_cancellable());
}

// When we hear that the process is exiting but we haven't had a chance to
// display an animation, we should just blank the screen.
TEST_F(SessionStateControllerImpl2Test, ShutdownWithoutButton) {
  Initialize(false, user::LOGGED_IN_USER);
  state_controller_->OnAppTerminating();

  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          SessionStateAnimator::kAllContainersMask,
          SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());
}

// Test that we display the fast-close animation and shut down when we get an
// outside request to shut down (e.g. from the login or lock screen).
TEST_F(SessionStateControllerImpl2Test, RequestShutdownFromLoginScreen) {
  Initialize(false, user::LOGGED_IN_NONE);

  state_controller_->RequestShutdown();

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

TEST_F(SessionStateControllerImpl2Test, RequestShutdownFromLockScreen) {
  Initialize(false, user::LOGGED_IN_USER);

  SystemLocks();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectPastLockAnimationFinished();

  state_controller_->RequestShutdown();

  ExpectShutdownAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);

  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());

  EXPECT_EQ(0, NumShutdownRequests());
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, NumShutdownRequests());
}

// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test,
       DISABLED_RequestAndCancelShutdownFromLockScreen) {
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

  float grayscale_before_button_release =
      Shell::GetPrimaryRootWindow()->layer()->layer_grayscale();

  // Release the power button before the shutdown timer fires.
  ReleasePowerButton();

  EXPECT_FALSE(test_api_->shutdown_timer_is_running());

  ExpectShutdownAnimationCancel();

  float grayscale_after_button_release =
      Shell::GetPrimaryRootWindow()->layer()->layer_grayscale();
  // Expect no flickering in undo animation.
  EXPECT_EQ(grayscale_before_button_release, grayscale_after_button_release);

  Advance(SessionStateAnimator::ANIMATION_SPEED_REVERT);
  ExpectLockedState();
}

// Test that we ignore power button presses when the screen is turned off.
TEST_F(SessionStateControllerImpl2Test, IgnorePowerButtonIfScreenIsOff) {
  Initialize(false, user::LOGGED_IN_USER);

  // When the screen brightness is at 0%, we shouldn't do anything in response
  // to power button presses.
  controller_->OnScreenBrightnessChanged(0.0);

  PressPowerButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleasePowerButton();

  // After increasing the brightness to 10%, we should start the timer like
  // usual.
  controller_->OnScreenBrightnessChanged(10.0);

  PressPowerButton();
  EXPECT_TRUE(test_api_->is_animating_lock());
}

// Test that hidden background appears and revers correctly on lock/cancel.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test,
    DISABLED_TestHiddenBackgroundLockCancel) {
  Initialize(false, user::LOGGED_IN_USER);
  HideBackground();

  EXPECT_TRUE(IsBackgroundHidden());
  ExpectUnlockedState();
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_FALSE(IsBackgroundHidden());
  ExpectBackgroundIsShowing();

  // Forward only half way through.
  AdvancePartially(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE, 0.5f);

  // Release the button before the lock timer fires.
  ReleasePowerButton();
  ExpectPreLockAnimationCancel();
  ExpectBackgroundIsHiding();
  EXPECT_FALSE(IsBackgroundHidden());

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);

  ExpectUnlockedState();
  EXPECT_TRUE(IsBackgroundHidden());
}

// Test that hidden background appears and revers correctly on lock/unlock.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(SessionStateControllerImpl2Test,
    DISABLED_TestHiddenBackgroundLockUnlock) {
  Initialize(false, user::LOGGED_IN_USER);
  HideBackground();

  EXPECT_TRUE(IsBackgroundHidden());
  ExpectUnlockedState();

  // Press the power button and check that the lock timer is started and that we
  // start lifting the non-screen-locker containers.
  PressPowerButton();

  ExpectPreLockAnimationStarted();
  EXPECT_FALSE(IsBackgroundHidden());
  ExpectBackgroundIsShowing();

  Advance(SessionStateAnimator::ANIMATION_SPEED_UNDOABLE);

  ExpectPreLockAnimationFinished();

  SystemLocks();

  ReleasePowerButton();

  ExpectPostLockAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectPastLockAnimationFinished();

  ExpectLockedState();

  SuccessfulAuthentication(NULL);

  ExpectUnlockBeforeUIDestroyedAnimationStarted();
  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockBeforeUIDestroyedAnimationFinished();

  SystemUnlocks();

  ExpectUnlockAfterUIDestroyedAnimationStarted();
  ExpectBackgroundIsHiding();
  EXPECT_FALSE(IsBackgroundHidden());

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockAfterUIDestroyedAnimationFinished();
  EXPECT_TRUE(IsBackgroundHidden());

  ExpectUnlockedState();
}

}  // namespace test
}  // namespace ash
