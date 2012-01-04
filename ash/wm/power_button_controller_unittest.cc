// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/power_button_controller.h"

#include "ash/shell.h"
#include "ash/test/aura_shell_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/aura/root_window.h"

namespace ash {
namespace test {

// Fake implementation of PowerButtonControllerDelegate that just logs requests
// to lock the screen and shut down the device.
class TestPowerButtonControllerDelegate : public PowerButtonControllerDelegate {
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

class PowerButtonControllerTest : public AuraShellTestBase {
 public:
  PowerButtonControllerTest() : controller_(NULL), delegate_(NULL) {}
  virtual ~PowerButtonControllerTest() {}

  void SetUp() OVERRIDE {
    AuraShellTestBase::SetUp();
    delegate_ = new TestPowerButtonControllerDelegate;
    controller_ = Shell::GetInstance()->power_button_controller();
    controller_->set_delegate(delegate_);  // transfers ownership
    test_api_.reset(new PowerButtonController::TestApi(controller_));
  }

 protected:
  PowerButtonController* controller_;  // not owned
  TestPowerButtonControllerDelegate* delegate_;  // not owned
  scoped_ptr<PowerButtonController::TestApi> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PowerButtonControllerTest);
};

// When we hold the power button while the user isn't logged in, we should shut
// down the machine directly.
TEST_F(PowerButtonControllerTest, ShutdownWhenNotLoggedIn) {
  controller_->OnLoginStateChange(false /*logged_in*/, false /*is_guest*/);
  controller_->OnLockStateChange(false);
  EXPECT_FALSE(test_api_->BackgroundLayerIsVisible());

  // Press the power button and check that we start the shutdown timer.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_CONTAINERS,
          PowerButtonController::ANIMATION_SLOW_CLOSE));
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());

  // Release the power button before the shutdown timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_CONTAINERS,
          PowerButtonController::ANIMATION_UNDO_SLOW_CLOSE));
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());

  // We should re-hide the black background layer after waiting long enough for
  // the animation to finish.
  EXPECT_TRUE(test_api_->hide_background_layer_timer_is_running());
  test_api_->trigger_hide_background_layer_timeout();
  EXPECT_FALSE(test_api_->BackgroundLayerIsVisible());

  // Press the button again and make the shutdown timeout fire this time.
  // Check that we start the timer for actually requesting the shutdown.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  test_api_->trigger_shutdown_timeout();
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, delegate_->num_shutdown_requests());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_CONTAINERS,
          PowerButtonController::ANIMATION_FAST_CLOSE));

  // When the timout fires, we should request a shutdown.
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, delegate_->num_shutdown_requests());
}

// Test that we lock the screen and deal with unlocking correctly.
TEST_F(PowerButtonControllerTest, LockAndUnlock) {
  controller_->OnLoginStateChange(true /*logged_in*/, false /*is_guest*/);
  controller_->OnLockStateChange(false);
  EXPECT_FALSE(test_api_->BackgroundLayerIsVisible());

  // We should initially be showing the screen locker containers, since they
  // also contain login-related windows that we want to show during the
  // logging-in animation.
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::SCREEN_LOCKER_CONTAINERS,
          PowerButtonController::ANIMATION_RESTORE));

  // Press the power button and check that the lock timer is started and that we
  // start scaling the non-screen-locker containers.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_FALSE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_SLOW_CLOSE));
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());

  // Release the button before the lock timer fires.
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_UNDO_SLOW_CLOSE));
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());
  EXPECT_TRUE(test_api_->hide_background_layer_timer_is_running());
  test_api_->trigger_hide_background_layer_timeout();
  EXPECT_FALSE(test_api_->BackgroundLayerIsVisible());

  // Press the button and fire the lock timer.  We should request that the
  // screen be locked, but we should still be in the slow-close animation.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_EQ(0, delegate_->num_lock_requests());
  test_api_->trigger_lock_timeout();
  EXPECT_EQ(1, delegate_->num_lock_requests());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_SLOW_CLOSE));
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());

  // Notify that we locked successfully.
  controller_->OnStartingLock();
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_FAST_CLOSE));
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::SCREEN_LOCKER_CONTAINERS,
          PowerButtonController::ANIMATION_HIDE));

  // Notify that the lock window is visible.  We should make it fade in.
  controller_->OnLockStateChange(true);
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_FADE_IN));

  // When we release the power button, the lock-to-shutdown timer should be
  // stopped.
  EXPECT_TRUE(test_api_->lock_to_shutdown_timer_is_running());
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Notify that the screen has been unlocked.  We should show the
  // non-screen-locker windows and hide the background layer.
  controller_->OnLockStateChange(false);
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_RESTORE));
  EXPECT_FALSE(test_api_->BackgroundLayerIsVisible());
}

// Hold the power button down from the unlocked state to eventual shutdown.
TEST_F(PowerButtonControllerTest, LockToShutdown) {
  controller_->OnLoginStateChange(true /*logged_in*/, false /*is_guest*/);
  controller_->OnLockStateChange(false);

  // Hold the power button and lock the screen.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  test_api_->trigger_lock_timeout();
  controller_->OnStartingLock();
  controller_->OnLockStateChange(true);
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());

  // When the lock-to-shutdown timeout fires, we should start the shutdown
  // timer.
  EXPECT_TRUE(test_api_->lock_to_shutdown_timer_is_running());
  test_api_->trigger_lock_to_shutdown_timeout();
  EXPECT_TRUE(test_api_->shutdown_timer_is_running());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_CONTAINERS,
          PowerButtonController::ANIMATION_SLOW_CLOSE));

  // Fire the shutdown timeout and check that we request shutdown.
  test_api_->trigger_shutdown_timeout();
  EXPECT_TRUE(test_api_->real_shutdown_timer_is_running());
  EXPECT_EQ(0, delegate_->num_shutdown_requests());
  test_api_->trigger_real_shutdown_timeout();
  EXPECT_EQ(1, delegate_->num_shutdown_requests());
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());
}

// Test that we handle the case where lock requests are ignored.
TEST_F(PowerButtonControllerTest, LockFail) {
  controller_->OnLoginStateChange(true /*logged_in*/, false /*is_guest*/);
  controller_->OnLockStateChange(false);

  // Hold the power button and lock the screen.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->lock_timer_is_running());
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_RESTORE));
  EXPECT_TRUE(test_api_->BackgroundLayerIsVisible());
  test_api_->trigger_lock_timeout();
  EXPECT_EQ(1, delegate_->num_lock_requests());
  EXPECT_TRUE(test_api_->lock_fail_timer_is_running());

  // We shouldn't start the lock-to-shutdown timer until the screen has actually
  // been locked.
  EXPECT_FALSE(test_api_->lock_to_shutdown_timer_is_running());

  // Act as if the request timed out.  We should restore the windows.
  test_api_->trigger_lock_fail_timeout();
  EXPECT_TRUE(
      test_api_->ContainerGroupIsAnimated(
          PowerButtonController::ALL_BUT_SCREEN_LOCKER_AND_RELATED_CONTAINERS,
          PowerButtonController::ANIMATION_RESTORE));
  EXPECT_FALSE(test_api_->BackgroundLayerIsVisible());
}

// Test that we start the timer to hide the background layer when the power
// button is released, but that we cancel the timer if the button is pressed
// again before the timer has fired.
TEST_F(PowerButtonControllerTest, CancelHideBackground) {
  controller_->OnLoginStateChange(false /*logged_in*/, false /*is_guest*/);
  controller_->OnLockStateChange(false);

  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  controller_->OnPowerButtonEvent(false, base::TimeTicks::Now());
  EXPECT_TRUE(test_api_->hide_background_layer_timer_is_running());

  // We should cancel the timer if we get another button-down event.
  controller_->OnPowerButtonEvent(true, base::TimeTicks::Now());
  EXPECT_FALSE(test_api_->hide_background_layer_timer_is_running());
}

}  // namespace test
}  // namespace ash
