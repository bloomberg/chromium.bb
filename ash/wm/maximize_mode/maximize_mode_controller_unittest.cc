// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "ash/wm/maximize_mode/maximize_mode_controller.h"

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/test/test_volume_control_delegate.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "base/command_line.h"
#include "base/test/simple_test_tick_clock.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/message_center.h"

#if defined(USE_X11)
#include "ui/events/test/events_test_utils_x11.h"
#endif

namespace ash {

namespace {

const float kDegreesToRadians = 3.1415926f / 180.0f;
const float kMeanGravity = 9.8066f;

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
  ~MaximizeModeControllerTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    chromeos::AccelerometerReader::GetInstance()->RemoveObserver(
        maximize_mode_controller());

    // Set the first display to be the internal display for the accelerometer
    // screen rotation tests.
    test::DisplayManagerTestApi(Shell::GetInstance()->display_manager()).
        SetFirstDisplayAsInternalDisplay();
  }

  void TearDown() override {
    chromeos::AccelerometerReader::GetInstance()->AddObserver(
        maximize_mode_controller());
    test::AshTestBase::TearDown();
  }

  MaximizeModeController* maximize_mode_controller() {
    return Shell::GetInstance()->maximize_mode_controller();
  }

  void TriggerLidUpdate(const gfx::Vector3dF& lid) {
    scoped_refptr<chromeos::AccelerometerUpdate> update(
        new chromeos::AccelerometerUpdate());
    update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, lid.x(), lid.y(),
                lid.z());
    maximize_mode_controller()->OnAccelerometerUpdated(update);
  }

  void TriggerBaseAndLidUpdate(const gfx::Vector3dF& base,
                               const gfx::Vector3dF& lid) {
    scoped_refptr<chromeos::AccelerometerUpdate> update(
        new chromeos::AccelerometerUpdate());
    update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, base.x(),
                base.y(), base.z());
    update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, lid.x(), lid.y(),
                lid.z());
    maximize_mode_controller()->OnAccelerometerUpdated(update);
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
    gfx::Vector3dF base_vector(0.0f, -kMeanGravity, 0.0f);
    gfx::Vector3dF lid_vector(0.0f,
                              kMeanGravity * cos(radians),
                              kMeanGravity * sin(radians));
    TriggerBaseAndLidUpdate(base_vector, lid_vector);
  }

  void OpenLid() {
    maximize_mode_controller()->LidEventReceived(true /* open */,
        maximize_mode_controller()->tick_clock_->NowTicks());
  }

  void CloseLid() {
    maximize_mode_controller()->LidEventReceived(false /* open */,
        maximize_mode_controller()->tick_clock_->NowTicks());
  }

  bool WasLidOpenedRecently() {
    return maximize_mode_controller()->WasLidOpenedRecently();
  }

 private:
  base::SimpleTestTickClock* test_tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeControllerTest);
};

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

// Verify the WasLidOpenedRecently signal with respect to time.
TEST_F(MaximizeModeControllerTest, WasLidOpenedRecentlyOverTime) {
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
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravity),
                          gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Completely vertical.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f),
                          gfx::Vector3dF(kMeanGravity, 0.0f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Close to vertical but with hinge appearing to be open 270 degrees.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravity, 0.1f, 0.0f));
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Flat and open 270 degrees should start maximize mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, -kMeanGravity),
                          gfx::Vector3dF(0.0f, kMeanGravity, 0.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());

  // Normal 90 degree orientation but near vertical should stay in maximize
  // mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(kMeanGravity, 0.0f, -0.1f),
                          gfx::Vector3dF(kMeanGravity, -0.1f, 0.0f));
  EXPECT_TRUE(IsMaximizeModeStarted());
}

TEST_F(MaximizeModeControllerTest, LaptopTest) {
  // Feeds in sample accelerometer data and verifies that there are no
  // transitions into touchview / maximize mode while shaking the device around
  // with the hinge at less than 180 degrees. Note the conversion from device
  // data to accelerometer updates consistent with accelerometer_reader.cc.
  ASSERT_EQ(0u, kAccelerometerLaptopModeTestDataLength % 6);
  for (size_t i = 0; i < kAccelerometerLaptopModeTestDataLength / 6; ++i) {
    gfx::Vector3dF base(-kAccelerometerLaptopModeTestData[i * 6 + 1],
                        -kAccelerometerLaptopModeTestData[i * 6],
                        -kAccelerometerLaptopModeTestData[i * 6 + 2]);
    base.Scale(kMeanGravity);
    gfx::Vector3dF lid(-kAccelerometerLaptopModeTestData[i * 6 + 4],
                       kAccelerometerLaptopModeTestData[i * 6 + 3],
                       kAccelerometerLaptopModeTestData[i * 6 + 5]);
    lid.Scale(kMeanGravity);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_FALSE(IsMaximizeModeStarted());
  }
}

TEST_F(MaximizeModeControllerTest, MaximizeModeTest) {
  // Trigger maximize mode by opening to 270 to begin the test in maximize mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity),
                          gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  // Feeds in sample accelerometer data and verifies that there are no
  // transitions out of touchview / maximize mode while shaking the device
  // around. Note the conversion from device data to accelerometer updates
  // consistent with accelerometer_reader.cc.
  ASSERT_EQ(0u, kAccelerometerFullyOpenTestDataLength % 6);
  for (size_t i = 0; i < kAccelerometerFullyOpenTestDataLength / 6; ++i) {
    gfx::Vector3dF base(-kAccelerometerFullyOpenTestData[i * 6 + 1],
                        -kAccelerometerFullyOpenTestData[i * 6],
                        -kAccelerometerFullyOpenTestData[i * 6 + 2]);
    base.Scale(kMeanGravity);
    gfx::Vector3dF lid(-kAccelerometerFullyOpenTestData[i * 6 + 4],
                       kAccelerometerFullyOpenTestData[i * 6 + 3],
                       kAccelerometerFullyOpenTestData[i * 6 + 5]);
    lid.Scale(kMeanGravity);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsMaximizeModeStarted());
  }
}

// Tests that CanEnterMaximizeMode returns false until a valid accelerometer
// event has been received, and that it returns true afterwards.
TEST_F(MaximizeModeControllerTest,
       CanEnterMaximizeModeRequiresValidAccelerometerUpdate) {
  // Should be false until an accelerometer event is sent.
  ASSERT_FALSE(maximize_mode_controller()->CanEnterMaximizeMode());
  OpenLidToAngle(90.0f);
  EXPECT_TRUE(maximize_mode_controller()->CanEnterMaximizeMode());
}

// Tests that when an accelerometer event is received which has no keyboard that
// we enter maximize mode.
TEST_F(MaximizeModeControllerTest,
       NoKeyboardAccelerometerTriggersMaximizeMode) {
  ASSERT_FALSE(IsMaximizeModeStarted());
  TriggerLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity));
  ASSERT_TRUE(IsMaximizeModeStarted());
}

// Test if this case does not crash. See http://crbug.com/462806
TEST_F(MaximizeModeControllerTest, DisplayDisconnectionDuringOverview) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  scoped_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(800, 0, 100, 100)));
  ASSERT_NE(w1->GetRootWindow(), w2->GetRootWindow());

  maximize_mode_controller()->EnableMaximizeModeWindowManager(true);
  Shell::GetInstance()->window_selector_controller()->ToggleOverview();

  UpdateDisplay("800x600");
  EXPECT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(w1->GetRootWindow(), w2->GetRootWindow());
}

class MaximizeModeControllerSwitchesTest : public MaximizeModeControllerTest {
 public:
  MaximizeModeControllerSwitchesTest() {}
  ~MaximizeModeControllerSwitchesTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTouchViewTesting);
    MaximizeModeControllerTest::SetUp();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MaximizeModeControllerSwitchesTest);
};

// Tests that when the command line switch for testing maximize mode is on, that
// accelerometer updates which would normally cause it to exit do not.
TEST_F(MaximizeModeControllerSwitchesTest, IgnoreHingeAngles) {
  maximize_mode_controller()->EnableMaximizeModeWindowManager(true);

  // Would normally trigger an exit from maximize mode.
  OpenLidToAngle(90.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());
}

}  // namespace ash
