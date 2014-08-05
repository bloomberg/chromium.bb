// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

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
#include "base/test/simple_test_tick_clock.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/vector3d_f.h"
#include "ui/message_center/message_center.h"

#if defined(USE_X11)
#include "ui/events/test/events_test_utils_x11.h"
#endif

namespace ash {

namespace {

const float kDegreesToRadians = 3.14159265f / 180.0f;

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

  bool IsMaximizeModeStarted() {
    return maximize_mode_controller()->IsMaximizeModeWindowManagerEnabled();
  }

  gfx::Display::Rotation GetInternalDisplayRotation() const {
    return Shell::GetInstance()->display_manager()->GetDisplayInfo(
        gfx::Display::InternalDisplayId()).rotation();
  }

  void SetInternalDisplayRotation(gfx::Display::Rotation rotation) const {
    Shell::GetInstance()->display_manager()->
        SetDisplayRotation(gfx::Display::InternalDisplayId(), rotation);
  }

  // Attaches a SimpleTestTickClock to the MaximizeModeController with a non
  // null value initial value.
  void AttachTickClockForTest() {
    scoped_ptr<base::TickClock> tick_clock(
        test_tick_clock_ = new base::SimpleTestTickClock());
    test_tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
    maximize_mode_controller()->SetTickClockForTest(tick_clock.Pass());
  }

  void AdvanceTickClock(const base::TimeDelta& delta) {
    DCHECK(test_tick_clock_);
    test_tick_clock_->Advance(delta);
  }

  void OpenLidToAngle(float degrees) {
    DCHECK(degrees >= 0.0f);
    DCHECK(degrees <= 360.0f);

    float radians = degrees * kDegreesToRadians;
    gfx::Vector3dF base_vector(1.0f, 0.0f, 0.0f);
    gfx::Vector3dF lid_vector(cos(radians), 0.0f, sin(radians));
    TriggerAccelerometerUpdate(base_vector, lid_vector);
  }

#if defined(OS_CHROMEOS)
  void OpenLid() {
    maximize_mode_controller()->LidEventReceived(true /* open */,
        maximize_mode_controller()->tick_clock_->NowTicks());
  }

  void CloseLid() {
    maximize_mode_controller()->LidEventReceived(false /* open */,
        maximize_mode_controller()->tick_clock_->NowTicks());
  }
#endif  // OS_CHROMEOS

  bool WasLidOpenedRecently() {
    return maximize_mode_controller()->WasLidOpenedRecently();
  }

 private:
  base::SimpleTestTickClock* test_tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeControllerTest);
};

#if defined(OS_CHROMEOS)

// Verify that closing the lid will exit maximize mode.
TEST_F(MaximizeModeControllerTest, CloseLidWhileInMaximizeMode) {
  OpenLidToAngle(315.0f);
  ASSERT_TRUE(IsMaximizeModeStarted());

  CloseLid();
  EXPECT_FALSE(IsMaximizeModeStarted());
}

// Verify that maximize mode will not be entered when the lid is closed.
TEST_F(MaximizeModeControllerTest,
    HingeAnglesWithLidClosed) {
  AttachTickClockForTest();

  CloseLid();

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(315.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(355.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());
}

// Verify the maximize mode state for unstable hinge angles when the lid was
// recently open.
TEST_F(MaximizeModeControllerTest,
    UnstableHingeAnglesWhenLidRecentlyOpened) {
  AttachTickClockForTest();

  OpenLid();
  ASSERT_TRUE(WasLidOpenedRecently());

  OpenLidToAngle(5.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(355.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  // This is a stable reading and should clear the last lid opened time.
  OpenLidToAngle(45.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());
  EXPECT_FALSE(WasLidOpenedRecently());

  OpenLidToAngle(355.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());
}

#endif  // OS_CHROMEOS

// Verify the WasLidOpenedRecently signal with respect to time.
TEST_F(MaximizeModeControllerTest, WasLidOpenedRecentlyOverTime) {
#if defined(OS_CHROMEOS)

  AttachTickClockForTest();

  // No lid open time initially.
  ASSERT_FALSE(WasLidOpenedRecently());

  CloseLid();
  EXPECT_FALSE(WasLidOpenedRecently());

  OpenLid();
  EXPECT_TRUE(WasLidOpenedRecently());

  // 1 second after lid open.
  AdvanceTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_TRUE(WasLidOpenedRecently());

  // 3 seconds after lid open.
  AdvanceTickClock(base::TimeDelta::FromSeconds(2));
  EXPECT_FALSE(WasLidOpenedRecently());

#else

  EXPECT_FALSE(WasLidOpenedRecently());

#endif  // OS_CHROMEOS
}

// Verify the maximize mode enter/exit thresholds for stable angles.
TEST_F(MaximizeModeControllerTest, StableHingeAnglesWithLidOpened) {
  ASSERT_FALSE(IsMaximizeModeStarted());
  ASSERT_FALSE(WasLidOpenedRecently());

  OpenLidToAngle(180.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(315.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());

  OpenLidToAngle(180.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());

  OpenLidToAngle(45.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(270.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());

  OpenLidToAngle(90.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());
}

// Verify the maximize mode state for unstable hinge angles when the lid is open
// but not recently.
TEST_F(MaximizeModeControllerTest, UnstableHingeAnglesWithLidOpened) {
  AttachTickClockForTest();

  ASSERT_FALSE(WasLidOpenedRecently());
  ASSERT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(5.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(355.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());

  OpenLidToAngle(5.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());
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

// Tests that accelerometer readings in each of the screen angles will trigger a
// rotation of the internal display.
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

#if defined(OS_CHROMEOS)
// Tests that a screenshot can be taken in maximize mode by holding volume down
// and pressing power.
TEST_F(MaximizeModeControllerTest, Screenshot) {
  Shell::GetInstance()->lock_state_controller()->SetDelegate(
      new test::TestLockStateControllerDelegate);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator event_generator(root, root);
  test::TestScreenshotDelegate* delegate = GetScreenshotDelegate();
  delegate->set_can_take_screenshot(true);

  // Open up 270 degrees.
  OpenLidToAngle(270.0f);
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

TEST_F(MaximizeModeControllerTest, LaptopTest) {
  // Feeds in sample accelerometer data and verifies that there are no
  // transitions into touchview / maximize mode while shaking the device around
  // with the hinge at less than 180 degrees.
  ASSERT_EQ(0u, kAccelerometerLaptopModeTestDataLength % 6);
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
  ASSERT_EQ(0u, kAccelerometerFullyOpenTestDataLength % 6);
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

  maximize_mode_controller()->SetRotationLocked(true);

  // Turn past the threshold for rotation.
  float degrees = 90.0;
  gravity.set_x(-cos(degrees * kDegreesToRadians));
  gravity.set_y(sin(degrees * kDegreesToRadians));
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());

  maximize_mode_controller()->SetRotationLocked(false);
  TriggerAccelerometerUpdate(gravity, gravity);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
}

// Tests that when MaximizeModeController turns off MaximizeMode that on the
// next accelerometer update the rotation lock is cleared.
TEST_F(MaximizeModeControllerTest, ExitingMaximizeModeClearRotationLock) {
  // Trigger maximize mode by opening to 270.
  OpenLidToAngle(270.0f);
  ASSERT_TRUE(IsMaximizeModeStarted());

  maximize_mode_controller()->SetRotationLocked(true);

  OpenLidToAngle(90.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Send an update that would not relaunch MaximizeMode.
  OpenLidToAngle(90.0f);
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
  maximize_mode_controller()->SetRotationLocked(false);
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

// Tests that if a user has set a display rotation that it is restored upon
// exiting maximize mode.
TEST_F(MaximizeModeControllerTest, ResetUserRotationUponExit) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                      gfx::Display::ROTATE_90);

  // Trigger maximize mode
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  TriggerAccelerometerUpdate(gfx::Vector3dF(1.0f, 0.0f, 0.0f),
                             gfx::Vector3dF(1.0f, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_180, GetInternalDisplayRotation());

  // Exit maximize mode
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, 1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());
  EXPECT_EQ(gfx::Display::ROTATE_90, GetInternalDisplayRotation());
}

// Tests that if a user sets a display rotation that accelerometer rotation
// becomes locked.
TEST_F(MaximizeModeControllerTest,
       NonAccelerometerRotationChangesLockRotation) {
  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  ASSERT_FALSE(maximize_mode_controller()->rotation_locked());
  SetInternalDisplayRotation(gfx::Display::ROTATE_270);
  EXPECT_TRUE(maximize_mode_controller()->rotation_locked());
}

// Tests that if a user changes the display rotation, while rotation is locked,
// that the updates are recorded. Upon exiting maximize mode the latest user
// rotation should be applied.
TEST_F(MaximizeModeControllerTest, UpdateUserRotationWhileRotationLocked) {
  // Trigger maximize mode by opening to 270.
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  SetInternalDisplayRotation(gfx::Display::ROTATE_270);
  // User sets rotation to the same rotation that the display was at when
  // maximize mode was activated.
  SetInternalDisplayRotation(gfx::Display::ROTATE_0);
  // Exit maximize mode
  TriggerAccelerometerUpdate(gfx::Vector3dF(0.0f, 0.0f, 1.0f),
                             gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_0, GetInternalDisplayRotation());
}

}  // namespace ash
