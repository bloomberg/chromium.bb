// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_state_controller.h"

#include "ash/ash_switches.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_lock_state_controller_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/session_state_animator.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#if defined(OS_CHROMEOS)
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/types/display_constants.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
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

aura::Window* GetContainer(int container ) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  return Shell::GetContainer(root_window, container);
}

bool IsBackgroundHidden() {
  return !GetContainer(kShellWindowId_DesktopBackgroundContainer)->IsVisible();
}

void HideBackground() {
  ui::ScopedLayerAnimationSettings settings(
      GetContainer(kShellWindowId_DesktopBackgroundContainer)
          ->layer()
          ->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta());
  GetContainer(kShellWindowId_DesktopBackgroundContainer)->Hide();
}

} // namespace

class LockStateControllerTest : public AshTestBase {
 public:
  LockStateControllerTest() : controller_(NULL), delegate_(NULL) {}
  virtual ~LockStateControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    // We would control animations in a fine way:
    animation_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION));
    // TODO(antrim) : restore
    // animator_helper_ = ui::test::CreateLayerAnimatorHelperForTest();

    // Temporary disable animations so that observer is always called, and
    // no leaks happen during tests.
    animation_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
    // TODO(antrim): once there is a way to mock time and run animations, make
    // sure that animations are finished even in simple tests.

    delegate_ = new TestLockStateControllerDelegate;
    controller_ = Shell::GetInstance()->power_button_controller();
    lock_state_controller_ = static_cast<LockStateController*>(
        Shell::GetInstance()->lock_state_controller());
    lock_state_controller_->SetDelegate(delegate_);  // transfers ownership
    test_api_.reset(new LockStateController::TestApi(lock_state_controller_));
    animator_api_.reset(
        new SessionStateAnimator::TestApi(lock_state_controller_->
            animator_.get()));
    shell_delegate_ = reinterpret_cast<TestShellDelegate*>(
        ash::Shell::GetInstance()->delegate());
    session_state_delegate_ = Shell::GetInstance()->session_state_delegate();
  }

  virtual void TearDown() {
    // TODO(antrim) : restore
    // animator_helper_->AdvanceUntilDone();
    window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void GenerateMouseMoveEvent() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
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
    EXPECT_FALSE(session_state_delegate_->IsScreenLocked());

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
    EXPECT_TRUE(session_state_delegate_->IsScreenLocked());

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
    lock_state_controller_->OnLockStateChanged(true);
    session_state_delegate_->LockScreen();
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void SuccessfulAuthentication(bool* call_flag) {
    base::Closure closure = base::Bind(&CheckCalledCallback, call_flag);
    lock_state_controller_->OnLockScreenHide(closure);
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void SystemUnlocks() {
    lock_state_controller_->OnLockStateChanged(false);
    session_state_delegate_->UnlockScreen();
    //TODO (antrim) : restore animator_helper_->Advance(base::TimeDelta());
  }

  void Initialize(bool legacy_button, user::LoginStatus status) {
    controller_->set_has_legacy_power_button_for_test(legacy_button);
    lock_state_controller_->OnLoginStateChanged(status);
    SetUserLoggedIn(status != user::LOGGED_IN_NONE);
    if (status == user::LOGGED_IN_GUEST)
      SetCanLockScreen(false);
    lock_state_controller_->OnLockStateChanged(false);
  }

  void CreateWindowForLockscreen() {
    window_.reset(new aura::Window(&window_delegate_));
    window_->SetBounds(gfx::Rect(0, 0, 100, 100));
    window_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window_->Init(aura::WINDOW_LAYER_TEXTURED);
    window_->SetName("WINDOW");
    aura::Window* container = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(), kShellWindowId_LockScreenContainer);
    ASSERT_TRUE(container);
    container->AddChild(window_.get());
    window_->Show();
  }

  PowerButtonController* controller_;  // not owned
  LockStateController* lock_state_controller_;  // not owned
  TestLockStateControllerDelegate* delegate_;  // not owned
  TestShellDelegate* shell_delegate_;  // not owned
  SessionStateDelegate* session_state_delegate_;  // not owned

  aura::test::TestWindowDelegate window_delegate_;
  scoped_ptr<aura::Window> window_;
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> animation_duration_mode_;
  scoped_ptr<LockStateController::TestApi> test_api_;
  scoped_ptr<SessionStateAnimator::TestApi> animator_api_;
  // TODO(antrim) : restore
//  scoped_ptr<ui::test::AnimationContainerTestHelper> animator_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LockStateControllerTest);
};

// Test the lock-to-shutdown flow for non-Chrome-OS hardware that doesn't
// correctly report power button releases.  We should lock immediately the first
// time the button is pressed and shut down when it's pressed from the locked
// state.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(LockStateControllerTest, DISABLED_LegacyLockAndShutDown) {
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
  lock_state_controller_->OnStartingLock();
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
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(LockStateControllerTest, DISABLED_LockAndUnlock) {
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
  lock_state_controller_->OnStartingLock();
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
TEST_F(LockStateControllerTest, DISABLED_LockAndCancel) {
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
      GetContainer(kShellWindowId_DefaultContainer)->layer()->transform();

  // Release the button before the lock timer fires.
  ReleasePowerButton();

  ExpectPreLockAnimationCancel();

  gfx::Transform transform_after_button_released =
      GetContainer(kShellWindowId_DefaultContainer)->layer()->transform();
  // Expect no flickering, animation should proceed from mid-state.
  EXPECT_EQ(transform_before_button_released, transform_after_button_released);

  Advance(SessionStateAnimator::ANIMATION_SPEED_MOVE_WINDOWS);
  ExpectUnlockedState();
  EXPECT_EQ(0, delegate_->num_lock_requests());
}

// Test that we deal with cancelling lock correctly.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(LockStateControllerTest,
       DISABLED_LockAndCancelAndLockAgain) {
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
TEST_F(LockStateControllerTest, DISABLED_LockToShutdown) {
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

// Test that we handle the case where lock requests are ignored.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(LockStateControllerTest, DISABLED_Lock) {
  // We require animations to have a duration for this test.
  ui::ScopedAnimationDurationScaleMode normal_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

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
TEST_F(LockStateControllerTest, LockButtonBasicNotLoggedIn) {
  // The lock button shouldn't do anything if we aren't logged in.
  Initialize(false, user::LOGGED_IN_NONE);

  PressLockButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, delegate_->num_lock_requests());
}

// Test the basic operation of the lock button (guest).
TEST_F(LockStateControllerTest, LockButtonBasicGuest) {
  // The lock button shouldn't do anything when we're logged in as a guest.
  Initialize(false, user::LOGGED_IN_GUEST);

  PressLockButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleaseLockButton();
  EXPECT_EQ(0, delegate_->num_lock_requests());
}

// Test the basic operation of the lock button.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(LockStateControllerTest, DISABLED_LockButtonBasic) {
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
TEST_F(LockStateControllerTest,
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
TEST_F(LockStateControllerTest, LockWithoutButton) {
  Initialize(false, user::LOGGED_IN_USER);
  lock_state_controller_->OnStartingLock();

  ExpectPreLockAnimationStarted();
  EXPECT_FALSE(test_api_->is_lock_cancellable());

  // TODO(antrim): After time-faking is fixed, let the pre-lock animation
  // complete here and check that delegate_->num_lock_requests() is 0 to
  // prevent http://crbug.com/172487 from regressing.
}

// When we hear that the process is exiting but we haven't had a chance to
// display an animation, we should just blank the screen.
TEST_F(LockStateControllerTest, ShutdownWithoutButton) {
  Initialize(false, user::LOGGED_IN_USER);
  lock_state_controller_->OnAppTerminating();

  EXPECT_TRUE(
      animator_api_->ContainersAreAnimated(
          SessionStateAnimator::kAllContainersMask,
          SessionStateAnimator::ANIMATION_HIDE_IMMEDIATELY));
  GenerateMouseMoveEvent();
  EXPECT_FALSE(cursor_visible());
}

// Test that we display the fast-close animation and shut down when we get an
// outside request to shut down (e.g. from the login or lock screen).
TEST_F(LockStateControllerTest, RequestShutdownFromLoginScreen) {
  Initialize(false, user::LOGGED_IN_NONE);
  CreateWindowForLockscreen();

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
  CreateWindowForLockscreen();
  Advance(SessionStateAnimator::ANIMATION_SPEED_SHUTDOWN);
  ExpectPastLockAnimationFinished();

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

// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(LockStateControllerTest,
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
TEST_F(LockStateControllerTest, IgnorePowerButtonIfScreenIsOff) {
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
  ReleasePowerButton();
}

#if defined(OS_CHROMEOS) && defined(USE_X11)
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
  controller_->OnScreenBrightnessChanged(0.0);
  static_cast<ui::TestDisplaySnapshot*>(outputs[0].display)
      ->set_current_mode(NULL);
  static_cast<ui::TestDisplaySnapshot*>(outputs[1].display)
      ->set_current_mode(NULL);
  controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_FALSE(test_api_->is_animating_lock());
  ReleasePowerButton();

  // When the screen brightness is 0% but the external display is still turned
  // on (indicating either docked mode or the user having manually decreased the
  // brightness to 0%), the power button should still be handled.
  static_cast<ui::TestDisplaySnapshot*>(outputs[1].display)
      ->set_current_mode(modes[0]);
  controller_->OnDisplayModeChanged(outputs);
  PressPowerButton();
  EXPECT_TRUE(test_api_->is_animating_lock());
  ReleasePowerButton();
}
#endif

// Test that hidden background appears and revers correctly on lock/cancel.
// TODO(antrim): Reenable this: http://crbug.com/167048
TEST_F(LockStateControllerTest, DISABLED_TestHiddenBackgroundLockCancel) {
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
TEST_F(LockStateControllerTest, DISABLED_TestHiddenBackgroundLockUnlock) {
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
