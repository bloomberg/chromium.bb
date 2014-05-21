// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_controller.h"

#include "ash/accelerometer/accelerometer_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_lock_state_controller_delegate.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/test/test_volume_control_delegate.h"
#include "ui/aura/test/event_generator.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/vector3d_f.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace {

const float kDegreesToRadians = 3.14159265f / 180.0f;

// Filter to count the number of events seen.
class EventCounter : public ui::EventHandler {
 public:
  EventCounter();
  virtual ~EventCounter();

  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE;

  void reset() {
    event_count_ = 0;
  }

  size_t event_count() const { return event_count_; }

 private:
  size_t event_count_;

  DISALLOW_COPY_AND_ASSIGN(EventCounter);
};

EventCounter::EventCounter() : event_count_(0) {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

EventCounter::~EventCounter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void EventCounter::OnEvent(ui::Event* event) {
  event_count_++;
}

}  // namespace

// Test accelerometer data taken with the lid at less than 180 degrees while
// shaking the device around. The data is to be interpreted in groups of 6 where
// each 6 values corresponds to the X, Y, and Z readings from the base and lid
// accelerometers in this order.
extern const float kAccelerometerLaptopModeTestData[];
extern const size_t kAccelerometerLaptopModeTestDataLength;

// Test accelerometer data taken with the lid open 360 degrees while
// shaking the device around. The data is to be interpreted in groups of 6 where
// each 6 values corresponds to the X, Y, and Z readings from the base and lid
// accelerometers in this order.
extern const float kAccelerometerFullyOpenTestData[];
extern const size_t kAccelerometerFullyOpenTestDataLength;

class MaximizeModeControllerTest : public test::AshTestBase {
 public:
  MaximizeModeControllerTest() {}
  virtual ~MaximizeModeControllerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    Shell::GetInstance()->accelerometer_controller()->RemoveObserver(
        maximize_mode_controller());

    // Set the first display to be the internal display for the accelerometer
    // screen rotation tests.
    test::DisplayManagerTestApi(Shell::GetInstance()->display_manager()).
        SetFirstDisplayAsInternalDisplay();
  }

  virtual void TearDown() OVERRIDE {
    Shell::GetInstance()->accelerometer_controller()->AddObserver(
        maximize_mode_controller());
    test::AshTestBase::TearDown();
  }

  MaximizeModeController* maximize_mode_controller() {
    return Shell::GetInstance()->maximize_mode_controller();
  }

  void TriggerAccelerometerUpdate(const gfx::Vector3dF& base,
                                  const gfx::Vector3dF& lid) {
    maximize_mode_controller()->OnAccelerometerUpdated(base, lid);
  }

  bool IsMaximizeModeStarted() const {
    return Shell::GetInstance()->IsMaximizeModeWindowManagerEnabled();
  }

  gfx::Display::Rotation GetInternalDisplayRotation() const {
    return Shell::GetInstance()->display_manager()->GetDisplayInfo(
        gfx::Display::InternalDisplayId()).rotation();
  }

  void SetInternalDisplayRotation(gfx::Display::Rotation rotation) const {
    Shell::GetInstance()->display_manager()->
        SetDisplayRotation(gfx::Display::InternalDisplayId(), rotation);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MaximizeModeControllerTest);
};

// Tests that opening the lid beyond 180 will enter touchview, and that it will
// exit when the lid comes back from 180. Also tests the thresholds, i.e. it
// will stick to the current mode.
TEST_F(MaximizeModeControllerTest, EnterExitThresholds) {
  // For the simple test the base remains steady.
  gfx::Vector3dF base(0.0f, 0.0f, 1.0f);

  // Lid open 90 degrees.
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Open just past 180.
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(0.05f, 0.0f, -1.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Open up 270 degrees.
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(1.0f, 0.0f, 0.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());

  // Open up 360 degrees and appearing to be slightly past it (i.e. as if almost
  // closed).
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(-0.05f, 0.0f, 1.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());

  // Open just before 180.
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(-0.05f, 0.0f, -1.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());

  // Open 90 degrees.
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());
}

// Tests that when the hinge is nearly vertically aligned, the current state
// persists as the computed angle is highly inaccurate in this orientation.
TEST_F(MaximizeModeControllerTest, HingeAligned) {
  // Laptop in normal orientation lid open 90 degrees.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, 1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Completely vertical.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, -1.0f, 0.0f),
                             gfx::Vector3dF(0.0f, -1.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Close to vertical but with hinge appearing to be open 270 degrees.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, -1.0f, 0.01f),
                             gfx::Vector3dF(0.01f, -1.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Flat and open 270 degrees should start maximize mode.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, 1.0f),
                             gfx::Vector3dF(1.0f, 0.0f, 0.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());

  // Normal 90 degree orientation but near vertical should stay in maximize
  // mode.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, -1.0f, 0.01f),
                             gfx::Vector3dF(-0.01f, -1.0f, 0.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());
}

// Tests that accelerometer readings in each of the screen angles will trigger
// a rotation of the internal display.
TEST_F(MaximizeModeControllerTest, DisplayRotation) {
  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  // Now test rotating in all directions.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 1.0f, 0.0f),
                             gfx::Vector3dF(0.0f, 1.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(1.0f, 0.0f, 0.0f),
                             gfx::Vector3dF(1.0f, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, -1.0f, 0.0f),
                             gfx::Vector3dF(0.0f, -1.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_270, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(-1.0f, 0.0f, 0.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
}

// Tests that low angles are ignored by the accelerometer (i.e. when the device
// is almost laying flat).
TEST_F(MaximizeModeControllerTest, RotationIgnoresLowAngles) {
  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  TriggerAccelerometerUpdate(gfx::Vector3dF(-1.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, -1.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.2f, -1.0f),
                             gfx::Vector3dF(0.0f, 0.2f, -1.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.2f, 0.0f, -1.0f),
                             gfx::Vector3dF(0.2f, 0.0f, -1.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, -0.2f, -1.0f),
                             gfx::Vector3dF(0.0f, -0.2f, -1.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(-0.2f, 0.0f, -1.0f),
                             gfx::Vector3dF(-0.2f, 0.0f, -1.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
}

// Tests that the display will stick to the current orientation beyond the
// halfway point, preventing frequent updates back and forth.
TEST_F(MaximizeModeControllerTest, RotationSticky) {
  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  gfx::Vector3dF gravity(-1.0f, 0.0f, 0.0f);
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  // Turn past half-way point to next direction and rotation should remain
  // the same.
  float degrees = 50.0;
  gravity.set_x(-cos(degrees * kDegreesToRadians));
  gravity.set_y(sin(degrees * kDegreesToRadians));
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  // Turn more and the screen should rotate.
  degrees = 70.0;
  gravity.set_x(-cos(degrees * kDegreesToRadians));
  gravity.set_y(sin(degrees * kDegreesToRadians));
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());

  // Turn back just beyond the half-way point and the new rotation should
  // still be in effect.
  degrees = 40.0;
  gravity.set_x(-cos(degrees * kDegreesToRadians));
  gravity.set_y(sin(degrees * kDegreesToRadians));
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
}

// Tests that the screen only rotates when maximize mode is engaged, and will
// return to the standard orientation on exiting maximize mode.
TEST_F(MaximizeModeControllerTest, RotationOnlyInMaximizeMode) {
  // Rotate on side with lid only open 90 degrees.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.95f, 0.35f),
                             gfx::Vector3dF(-0.35f, 0.95f, 0.0f));
  ASSERT_FALSE(IsMaximizeModeStarted());
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  // Open lid, screen should now rotate to match orientation.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.95f, -0.35f),
                             gfx::Vector3dF(-0.35f, 0.95f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());

  // Close lid back to 90, screen should rotate back.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.95f, 0.35f),
                             gfx::Vector3dF(-0.35f, 0.95f, 0.0f));
  ASSERT_FALSE(IsMaximizeModeStarted());
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
}

// Tests that maximize mode blocks keyboard and mouse events but not touch
// events.
TEST_F(MaximizeModeControllerTest, BlocksKeyboardAndMouse) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator event_generator(root, root);
  EventCounter counter;

  event_generator.PressKey(ui::VKEY_ESCAPE, 0);
  event_generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_GT(counter.event_count(), 0u);
  counter.reset();

  event_generator.ClickLeftButton();
  EXPECT_GT(counter.event_count(), 0u);
  counter.reset();

  event_generator.ScrollSequence(
      gfx::Point(), base::TimeDelta::FromMilliseconds(5), 0, 100, 5, 2);
  EXPECT_GT(counter.event_count(), 0u);
  counter.reset();

  event_generator.MoveMouseWheel(0, 10);
  EXPECT_GT(counter.event_count(), 0u);
  counter.reset();

  event_generator.PressTouch();
  event_generator.ReleaseTouch();
  EXPECT_GT(counter.event_count(), 0u);
  counter.reset();

  // Open up 270 degrees.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, 1.0f),
                             gfx::Vector3dF(1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  event_generator.PressKey(ui::VKEY_ESCAPE, 0);
  event_generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_EQ(0u, counter.event_count());
  counter.reset();

  event_generator.ClickLeftButton();
  EXPECT_EQ(0u, counter.event_count());
  counter.reset();

  event_generator.ScrollSequence(
      gfx::Point(), base::TimeDelta::FromMilliseconds(5), 0, 100, 5, 2);
  EXPECT_EQ(0u, counter.event_count());
  counter.reset();

  event_generator.MoveMouseWheel(0, 10);
  EXPECT_EQ(0u, counter.event_count());
  counter.reset();

  // Touch should not be blocked.
  event_generator.PressTouch();
  event_generator.ReleaseTouch();
  EXPECT_GT(counter.event_count(), 0u);
  counter.reset();

  gfx::Vector3dF base;

  // Lid open 90 degrees.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, 1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));

  event_generator.PressKey(ui::VKEY_ESCAPE, 0);
  event_generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_GT(counter.event_count(), 0u);
  counter.reset();
}

#if defined(OS_CHROMEOS)
// Tests that a screenshot can be taken in maximize mode by holding volume down
// and pressing power.
TEST_F(MaximizeModeControllerTest, Screenshot) {
  Shell::GetInstance()->lock_state_controller()->SetDelegate(
      new test::TestLockStateControllerDelegate);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator event_generator(root, root);
  test::TestScreenshotDelegate* delegate = GetScreenshotDelegate();
  delegate->set_can_take_screenshot(true);

  // Open up 270 degrees.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, 1.0f),
                             gfx::Vector3dF(1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  // Pressing power alone does not take a screenshot.
  event_generator.PressKey(ui::VKEY_POWER, 0);
  event_generator.ReleaseKey(ui::VKEY_POWER, 0);
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Holding volume down and pressing power takes a screenshot.
  event_generator.PressKey(ui::VKEY_VOLUME_DOWN, 0);
  event_generator.PressKey(ui::VKEY_POWER, 0);
  event_generator.ReleaseKey(ui::VKEY_POWER, 0);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
  event_generator.ReleaseKey(ui::VKEY_VOLUME_DOWN, 0);
}
#endif  // OS_CHROMEOS

// Tests that maximize mode does not block Volume Up & Down events.
TEST_F(MaximizeModeControllerTest, AllowsVolumeControl) {
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator event_generator(root, root);

  TestVolumeControlDelegate* volume_delegate =
      new TestVolumeControlDelegate(true);
  ash::Shell::GetInstance()->system_tray_delegate()->SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate>(volume_delegate).Pass());

  // Trigger maximize mode by opening to 270 to begin the test in maximize mode.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  // Verify volume down button event is not blocked
  ASSERT_EQ(0, volume_delegate->handle_volume_down_count());
  event_generator.PressKey(ui::VKEY_VOLUME_DOWN, 0);
  event_generator.ReleaseKey(ui::VKEY_VOLUME_DOWN, 0);
  EXPECT_EQ(1, volume_delegate->handle_volume_down_count());

  // Verify volume up event is not blocked
  ASSERT_EQ(0, volume_delegate->handle_volume_up_count());
  event_generator.PressKey(ui::VKEY_VOLUME_UP, 0);
  event_generator.ReleaseKey(ui::VKEY_VOLUME_UP, 0);
  EXPECT_EQ(1, volume_delegate->handle_volume_up_count());
}

TEST_F(MaximizeModeControllerTest, LaptopTest) {
  // Feeds in sample accelerometer data and verifies that there are no
  // transitions into touchview / maximize mode while shaking the device around
  // with the hinge at less than 180 degrees.
  ASSERT_TRUE(kAccelerometerLaptopModeTestDataLength % 6 == 0);
  for (size_t i = 0; i < kAccelerometerLaptopModeTestDataLength / 6; ++i) {
    gfx::Vector3dF base(kAccelerometerLaptopModeTestData[i * 6],
                        kAccelerometerLaptopModeTestData[i * 6 + 1],
                        kAccelerometerLaptopModeTestData[i * 6 + 2]);
    gfx::Vector3dF lid(kAccelerometerLaptopModeTestData[i * 6 + 3],
                       kAccelerometerLaptopModeTestData[i * 6 + 4],
                       kAccelerometerLaptopModeTestData[i * 6 + 5]);
    TriggerAccelerometerUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_FALSE(IsMaximizeModeStarted());
  }
}

TEST_F(MaximizeModeControllerTest, MaximizeModeTest) {
  // Trigger maximize mode by opening to 270 to begin the test in maximize mode.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  // Feeds in sample accelerometer data and verifies that there are no
  // transitions out of touchview / maximize mode while shaking the device
  // around.
  ASSERT_TRUE(kAccelerometerFullyOpenTestDataLength % 6 == 0);
  for (size_t i = 0; i < kAccelerometerFullyOpenTestDataLength / 6; ++i) {
    gfx::Vector3dF base(kAccelerometerFullyOpenTestData[i * 6],
                        kAccelerometerFullyOpenTestData[i * 6 + 1],
                        kAccelerometerFullyOpenTestData[i * 6 + 2]);
    gfx::Vector3dF lid(kAccelerometerFullyOpenTestData[i * 6 + 3],
                       kAccelerometerFullyOpenTestData[i * 6 + 4],
                       kAccelerometerFullyOpenTestData[i * 6 + 5]);
    TriggerAccelerometerUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsMaximizeModeStarted());
  }
}

// Tests that the display will stick to its current orientation when the
// rotation lock has been set.
TEST_F(MaximizeModeControllerTest, RotationLockPreventsRotation) {
  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  gfx::Vector3dF gravity(-1.0f, 0.0f, 0.0f);

  maximize_mode_controller()->set_rotation_locked(true);

  // Turn past the threshold for rotation.
  float degrees = 90.0;
  gravity.set_x(-cos(degrees * kDegreesToRadians));
  gravity.set_y(sin(degrees * kDegreesToRadians));
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  maximize_mode_controller()->set_rotation_locked(false);
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
}

// Tests that when MaximizeModeController turns off MaximizeMode that on the
// next accelerometer update the rotation lock is cleared.
TEST_F(MaximizeModeControllerTest, ExitingMaximizeModeClearRotationLock) {
  // The base remains steady.
  gfx::Vector3dF base(0.0f, 0.0f, 1.0f);

  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  maximize_mode_controller()->set_rotation_locked(true);

  // Open 90 degrees.
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Send an update that would not relaunch MaximizeMode. 90 degrees.
  TriggerAccelerometerUpdate(base, gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_FALSE(maximize_mode_controller()->rotation_locked());
}

// The TrayDisplay class that is responsible for adding/updating MessageCenter
// notifications is only added to the SystemTray on ChromeOS.
#if defined(OS_CHROMEOS)
// Tests that the screen rotation notifications are suppressed when
// triggered by the accelerometer.
TEST_F(MaximizeModeControllerTest, BlockRotationNotifications) {
  test::TestSystemTrayDelegate* tray_delegate =
      static_cast<test::TestSystemTrayDelegate*>(
          Shell::GetInstance()->system_tray_delegate());
  tray_delegate->set_should_show_display_notification(true);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when not in maximize mode
  ASSERT_FALSE(IsMaximizeModeStarted());
  ASSERT_NE(gfx::Display::ROTATE_180, GetInternalDisplayRotation());
  ASSERT_EQ(0u, message_center->NotificationCount());
  ASSERT_FALSE(message_center->HasPopupNotifications());
  SetInternalDisplayRotation(gfx::Display::ROTATE_180);
  EXPECT_EQ(gfx::Display::ROTATE_180, GetInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());

  // Reset the screen rotation.
  SetInternalDisplayRotation(gfx::Display::ROTATE_0);
  // Clear all notifications
  message_center->RemoveAllNotifications(false);
  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are still displayed when
  // adjusting the screen rotation directly when in maximize mode
  ASSERT_NE(gfx::Display::ROTATE_270, GetInternalDisplayRotation());
  SetInternalDisplayRotation(gfx::Display::ROTATE_270);
  EXPECT_EQ(gfx::Display::ROTATE_270, GetInternalDisplayRotation());
  EXPECT_EQ(1u, message_center->NotificationCount());
  EXPECT_TRUE(message_center->HasPopupNotifications());

  // Clear all notifications
  message_center->RemoveAllNotifications(false);
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());

  // Make sure notifications are blocked when adjusting the screen rotation
  // via the accelerometer while in maximize mode
  // Rotate the screen 90 degrees
  ASSERT_NE(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 1.0f, 0.0f),
                             gfx::Vector3dF(0.0f, 1.0f, 0.0f));
  ASSERT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
  EXPECT_EQ(0u, message_center->NotificationCount());
  EXPECT_FALSE(message_center->HasPopupNotifications());
}
#endif

}  // namespace ash
