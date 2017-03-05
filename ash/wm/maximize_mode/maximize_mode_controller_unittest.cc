// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"

#include <math.h>
#include <utility>
#include <vector>

#include "ash/common/ash_switches.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/user_action_tester.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
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

const char kTouchViewInitiallyDisabled[] = "Touchview_Initially_Disabled";
const char kTouchViewEnabled[] = "Touchview_Enabled";
const char kTouchViewDisabled[] = "Touchview_Disabled";

}  // namespace

// Test accelerometer data taken with the lid at less than 180 degrees while
// shaking the device around. The data is to be interpreted in groups of 6 where
// each 6 values corresponds to the base accelerometer (-y / g, -x / g, -z / g)
// followed by the lid accelerometer (-y / g , x / g, z / g).
extern const float kAccelerometerLaptopModeTestData[];
extern const size_t kAccelerometerLaptopModeTestDataLength;

// Test accelerometer data taken with the lid open 360 degrees while
// shaking the device around. The data is to be interpreted in groups of 6 where
// each 6 values corresponds to the base accelerometer (-y / g, -x / g, -z / g)
// followed by the lid accelerometer (-y / g , x / g, z / g).
extern const float kAccelerometerFullyOpenTestData[];
extern const size_t kAccelerometerFullyOpenTestDataLength;

// Test accelerometer data taken with the lid open 360 degrees while the device
// hinge was nearly vertical, while shaking the device around. The data is to be
// interpreted in groups of 6 where each 6 values corresponds to the X, Y, and Z
// readings from the base and lid accelerometers in this order.
extern const float kAccelerometerVerticalHingeTestData[];
extern const size_t kAccelerometerVerticalHingeTestDataLength;
extern const float kAccelerometerVerticalHingeUnstableAnglesTestData[];
extern const size_t kAccelerometerVerticalHingeUnstableAnglesTestDataLength;

class MaximizeModeControllerTest : public test::AshTestBase {
 public:
  MaximizeModeControllerTest() {}
  ~MaximizeModeControllerTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTouchView);
    test::AshTestBase::SetUp();
    chromeos::AccelerometerReader::GetInstance()->RemoveObserver(
        maximize_mode_controller());

    // Set the first display to be the internal display for the accelerometer
    // screen rotation tests.
    display::test::DisplayManagerTestApi(
        Shell::GetInstance()->display_manager())
        .SetFirstDisplayAsInternalDisplay();
  }

  void TearDown() override {
    chromeos::AccelerometerReader::GetInstance()->AddObserver(
        maximize_mode_controller());
    test::AshTestBase::TearDown();
  }

  MaximizeModeController* maximize_mode_controller() {
    return WmShell::Get()->maximize_mode_controller();
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

  // Attaches a SimpleTestTickClock to the MaximizeModeController with a non
  // null value initial value.
  void AttachTickClockForTest() {
    std::unique_ptr<base::TickClock> tick_clock(
        test_tick_clock_ = new base::SimpleTestTickClock());
    test_tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
    maximize_mode_controller()->SetTickClockForTest(std::move(tick_clock));
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
    gfx::Vector3dF lid_vector(0.0f, kMeanGravity * cos(radians),
                              kMeanGravity * sin(radians));
    TriggerBaseAndLidUpdate(base_vector, lid_vector);
  }

  void HoldDeviceVertical() {
    gfx::Vector3dF base_vector(9.8f, 0.0f, 0.0f);
    gfx::Vector3dF lid_vector(9.8f, 0.0f, 0.0f);
    TriggerBaseAndLidUpdate(base_vector, lid_vector);
  }

  void OpenLid() {
    maximize_mode_controller()->LidEventReceived(
        true /* open */, maximize_mode_controller()->tick_clock_->NowTicks());
  }

  void CloseLid() {
    maximize_mode_controller()->LidEventReceived(
        false /* open */, maximize_mode_controller()->tick_clock_->NowTicks());
  }

  bool WasLidOpenedRecently() {
    return maximize_mode_controller()->WasLidOpenedRecently();
  }

  void SetTabletMode(bool on) {
    maximize_mode_controller()->TabletModeEventReceived(
        on, maximize_mode_controller()->tick_clock_->NowTicks());
  }

  bool AreEventsBlocked() {
    return !!maximize_mode_controller()->event_blocker_.get();
  }

  base::UserActionTester* user_action_tester() { return &user_action_tester_; }

 private:
  base::SimpleTestTickClock* test_tick_clock_;

  // Tracks user action counts.
  base::UserActionTester user_action_tester_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeControllerTest);
};

// Verify TouchView enabled/disabled user action metrics are recorded.
TEST_F(MaximizeModeControllerTest, VerifyTouchViewEnabledDisabledCounts) {
  ASSERT_EQ(1,
            user_action_tester()->GetActionCount(kTouchViewInitiallyDisabled));
  ASSERT_EQ(0, user_action_tester()->GetActionCount(kTouchViewEnabled));
  ASSERT_EQ(0, user_action_tester()->GetActionCount(kTouchViewDisabled));

  user_action_tester()->ResetCounts();
  maximize_mode_controller()->EnableMaximizeModeWindowManager(true);
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTouchViewEnabled));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTouchViewDisabled));
  maximize_mode_controller()->EnableMaximizeModeWindowManager(true);
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTouchViewEnabled));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTouchViewDisabled));

  user_action_tester()->ResetCounts();
  maximize_mode_controller()->EnableMaximizeModeWindowManager(false);
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTouchViewEnabled));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTouchViewDisabled));
  maximize_mode_controller()->EnableMaximizeModeWindowManager(false);
  EXPECT_EQ(0, user_action_tester()->GetActionCount(kTouchViewEnabled));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(kTouchViewDisabled));
}

// Verify that closing the lid will exit maximize mode.
TEST_F(MaximizeModeControllerTest, CloseLidWhileInMaximizeMode) {
  OpenLidToAngle(315.0f);
  ASSERT_TRUE(IsMaximizeModeStarted());

  CloseLid();
  EXPECT_FALSE(IsMaximizeModeStarted());
}

// Verify that maximize mode will not be entered when the lid is closed.
TEST_F(MaximizeModeControllerTest, HingeAnglesWithLidClosed) {
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
TEST_F(MaximizeModeControllerTest, UnstableHingeAnglesWhenLidRecentlyOpened) {
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

TEST_F(MaximizeModeControllerTest, TabletModeTransition) {
  OpenLidToAngle(90.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  // Unstable reading. This should not trigger maximize mode.
  HoldDeviceVertical();
  EXPECT_FALSE(IsMaximizeModeStarted());

  // When tablet mode switch is on it should force maximize mode even if the
  // reading is not stable.
  SetTabletMode(true);
  EXPECT_TRUE(IsMaximizeModeStarted());

  // After tablet mode switch is off it should stay in maximize mode if the
  // reading is not stable.
  SetTabletMode(false);
  EXPECT_TRUE(IsMaximizeModeStarted());

  // Should leave maximize mode when the lid angle is small enough.
  OpenLidToAngle(90.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(300.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());
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

TEST_F(MaximizeModeControllerTest, VerticalHingeTest) {
  // Feeds in sample accelerometer data and verifies that there are no
  // transitions out of touchview / maximize mode while shaking the device
  // around, while the hinge is nearly vertical. The data was captured from
  // maxmimize_mode_controller.cc and does not require conversion.
  ASSERT_EQ(0u, kAccelerometerVerticalHingeTestDataLength % 6);
  for (size_t i = 0; i < kAccelerometerVerticalHingeTestDataLength / 6; ++i) {
    gfx::Vector3dF base(kAccelerometerVerticalHingeTestData[i * 6],
                        kAccelerometerVerticalHingeTestData[i * 6 + 1],
                        kAccelerometerVerticalHingeTestData[i * 6 + 2]);
    gfx::Vector3dF lid(kAccelerometerVerticalHingeTestData[i * 6 + 3],
                       kAccelerometerVerticalHingeTestData[i * 6 + 4],
                       kAccelerometerVerticalHingeTestData[i * 6 + 5]);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsMaximizeModeStarted());
  }
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
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(800, 0, 100, 100)));
  ASSERT_NE(w1->GetRootWindow(), w2->GetRootWindow());

  maximize_mode_controller()->EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(WmShell::Get()->window_selector_controller()->ToggleOverview());

  UpdateDisplay("800x600");
  EXPECT_FALSE(WmShell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(w1->GetRootWindow(), w2->GetRootWindow());
}

// Test that the disabling of the internal display exits maximize mode, and that
// while disabled we do not re-enter maximize mode.
TEST_F(MaximizeModeControllerTest, NoMaximizeModeWithDisabledInternalDisplay) {
  UpdateDisplay("200x200, 200x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  ASSERT_FALSE(IsMaximizeModeStarted());

  OpenLidToAngle(270.0f);
  EXPECT_TRUE(IsMaximizeModeStarted());
  EXPECT_TRUE(AreEventsBlocked());

  // Deactivate internal display to simulate Docked Mode.
  std::vector<display::ManagedDisplayInfo> secondary_only;
  secondary_only.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetDisplayAt(1).id()));
  display_manager()->OnNativeDisplaysChanged(secondary_only);
  ASSERT_FALSE(display_manager()->IsActiveDisplayId(internal_display_id));
  EXPECT_FALSE(IsMaximizeModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  OpenLidToAngle(270.0f);
  EXPECT_FALSE(IsMaximizeModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Tablet mode signal should also be ignored.
  SetTabletMode(true);
  EXPECT_FALSE(IsMaximizeModeStarted());
  EXPECT_FALSE(AreEventsBlocked());
}

// Tests that is a tablet mode signal is received while docked, that maximize
// mode is enabled upon exiting docked mode.
TEST_F(MaximizeModeControllerTest, MaximizeModeAfterExitingDockedMode) {
  UpdateDisplay("200x200, 200x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  ASSERT_FALSE(IsMaximizeModeStarted());

  // Deactivate internal display to simulate Docked Mode.
  std::vector<display::ManagedDisplayInfo> all_displays;
  all_displays.push_back(display_manager()->GetDisplayInfo(
      display_manager()->GetDisplayAt(0).id()));
  std::vector<display::ManagedDisplayInfo> secondary_only;
  display::ManagedDisplayInfo secondary_display =
      display_manager()->GetDisplayInfo(
          display_manager()->GetDisplayAt(1).id());
  all_displays.push_back(secondary_display);
  secondary_only.push_back(secondary_display);
  display_manager()->OnNativeDisplaysChanged(secondary_only);
  ASSERT_FALSE(display_manager()->IsActiveDisplayId(internal_display_id));

  // Tablet mode signal should also be ignored.
  SetTabletMode(true);
  EXPECT_FALSE(IsMaximizeModeStarted());
  EXPECT_FALSE(AreEventsBlocked());

  // Exiting docked state
  display_manager()->OnNativeDisplaysChanged(all_displays);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  EXPECT_TRUE(IsMaximizeModeStarted());
}

// Verify that the device won't exit touchview / maximize mode for unstable
// angles when hinge is nearly vertical
TEST_F(MaximizeModeControllerTest, VerticalHingeUnstableAnglesTest) {
  // Trigger maximize mode by opening to 270 to begin the test in maximize mode.
  TriggerBaseAndLidUpdate(gfx::Vector3dF(0.0f, 0.0f, kMeanGravity),
                          gfx::Vector3dF(0.0f, -kMeanGravity, 0.0f));
  ASSERT_TRUE(IsMaximizeModeStarted());

  // Feeds in sample accelerometer data and verifies that there are no
  // transitions out of touchview / maximize mode while shaking the device
  // around, while the hinge is nearly vertical. The data was captured
  // from maxmimize_mode_controller.cc and does not require conversion.
  ASSERT_EQ(0u, kAccelerometerVerticalHingeUnstableAnglesTestDataLength % 6);
  for (size_t i = 0;
       i < kAccelerometerVerticalHingeUnstableAnglesTestDataLength / 6; ++i) {
    gfx::Vector3dF base(
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 1],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 2]);
    gfx::Vector3dF lid(
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 3],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 4],
        kAccelerometerVerticalHingeUnstableAnglesTestData[i * 6 + 5]);
    TriggerBaseAndLidUpdate(base, lid);
    // There are a lot of samples, so ASSERT rather than EXPECT to only generate
    // one failure rather than potentially hundreds.
    ASSERT_TRUE(IsMaximizeModeStarted());
  }
}

}  // namespace ash
