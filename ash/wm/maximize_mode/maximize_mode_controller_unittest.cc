// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_controller.h"

#include "ash/accelerometer/accelerometer_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/gfx/vector3d_f.h"

namespace ash {

class MaximizeModeControllerTest : public test::AshTestBase {
 public:
  MaximizeModeControllerTest() {}
  virtual ~MaximizeModeControllerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    Shell::GetInstance()->accelerometer_controller()->RemoveObserver(
        maximize_mode_controller());
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

}  // namespace ash
