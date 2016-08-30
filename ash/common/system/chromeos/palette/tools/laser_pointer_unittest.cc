// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/laser_pointer_mode.h"
#include "ash/common/system/chromeos/palette/tools/laser_pointer_mode_test_api.h"
#include "ash/common/system/chromeos/palette/tools/laser_pointer_points_test_api.h"
#include "ash/common/system/chromeos/palette/tools/laser_pointer_view.h"
#include "ash/common/test/test_palette_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

const int kTestPointsLifetimeSeconds = 5;

class LaserPointerPointsTest : public test::AshTestBase {
 public:
  LaserPointerPointsTest()
      : points_(base::TimeDelta::FromSeconds(kTestPointsLifetimeSeconds)),
        points_test_api_(base::MakeUnique<LaserPointerPoints>(
            base::TimeDelta::FromSeconds(kTestPointsLifetimeSeconds))) {}

  ~LaserPointerPointsTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    // Add a test delegate so that laser pointer mode does not complain when
    // being destroyed.
    WmShell::Get()->SetPaletteDelegateForTesting(
        base::MakeUnique<TestPaletteDelegate>());
  }

 protected:
  LaserPointerPoints points_;
  LaserPointerPointsTestApi points_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LaserPointerPointsTest);
};

class LaserPointerModeTest : public test::AshTestBase {
 public:
  LaserPointerModeTest() {}
  ~LaserPointerModeTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    WmShell::Get()->SetPaletteDelegateForTesting(
        base::MakeUnique<TestPaletteDelegate>());
    mode_test_api_.reset(new LaserPointerModeTestApi(
        base::WrapUnique<LaserPointerMode>(new LaserPointerMode(nullptr))));
  }

  void TearDown() override {
    // This needs to be called first to remove the pointer watcher otherwise
    // tear down will complain about there being more than zero pointer watcher
    // alive.
    mode_test_api_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<LaserPointerModeTestApi> mode_test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LaserPointerModeTest);
};

}  // namespace

// Tests that the laser pointers internal collection handles receiving points
// and that the functions are returning the expected output.
TEST_F(LaserPointerPointsTest, LaserPointerInternalCollection) {
  EXPECT_TRUE(points_.IsEmpty());
  EXPECT_EQ(gfx::Rect(), points_.GetBoundingBox());
  const gfx::Point left(1, 1);
  const gfx::Point bottom(1, 9);
  const gfx::Point top_right(30, 0);
  const gfx::Point last(2, 2);
  points_.AddPoint(left);
  EXPECT_EQ(gfx::Rect(1, 1, 0, 0), points_.GetBoundingBox());

  // Should be the new bottom of the bounding box.
  points_.AddPoint(bottom);
  EXPECT_EQ(gfx::Rect(1, 1, 0, bottom.y() - 1), points_.GetBoundingBox());

  // Should be the new top and right of the bounding box.
  points_.AddPoint(top_right);
  EXPECT_EQ(3, points_.GetNumberOfPoints());
  EXPECT_FALSE(points_.IsEmpty());
  EXPECT_EQ(gfx::Rect(left.x(), top_right.y(), top_right.x() - left.x(),
                      bottom.y() - top_right.y()),
            points_.GetBoundingBox());

  // Should not expand bounding box.
  points_.AddPoint(last);
  EXPECT_EQ(gfx::Rect(left.x(), top_right.y(), top_right.x() - left.x(),
                      bottom.y() - top_right.y()),
            points_.GetBoundingBox());

  // Points should be sorted in the order they are added.
  EXPECT_EQ(left, points_.GetOldest().location);
  EXPECT_EQ(last, points_.GetNewest().location);

  // Add a new point which will expand the bounding box.
  gfx::Point new_left_bottom(0, 40);
  points_.AddPoint(new_left_bottom);
  EXPECT_EQ(5, points_.GetNumberOfPoints());
  EXPECT_EQ(gfx::Rect(new_left_bottom.x(), top_right.y(),
                      top_right.x() - new_left_bottom.x(),
                      new_left_bottom.y() - top_right.y()),
            points_.GetBoundingBox());

  // Verify clearing works.
  points_.Clear();
  EXPECT_TRUE(points_.IsEmpty());
}

// Test the laser pointer points collection to verify that old points are
// removed.
TEST_F(LaserPointerPointsTest, LaserPointerInternalCollectionDeletion) {
  // When a point older than kTestPointsLifetime (5 seconds) is added, it
  // should get removed.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, points_test_api_.GetNumberOfPoints());
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(2, points_test_api_.GetNumberOfPoints());

  // Verify adding a point 10 seconds later will clear all other points, since
  // they are older than 5 seconds.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(1, points_test_api_.GetNumberOfPoints());

  // Verify adding 3 points one second apart each will add 3 points to the
  // collection, since all 4 poitns are younger than 5 seconds.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(4, points_test_api_.GetNumberOfPoints());

  // Verify adding 1 point three seconds later will remove 2 points which are
  // older than 5 seconds.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(3));
  EXPECT_EQ(3, points_test_api_.GetNumberOfPoints());
}

// Test to ensure the class responsible for drawing the laser pointer receives
// points from mouse movements as expected.
TEST_F(LaserPointerModeTest, LaserPointerRenderer) {
  // The laser pointer mode only works with stylus.
  GetEventGenerator().EnterPenPointerMode();
  GetEventGenerator().MoveMouseToInHost(gfx::Point(10, 40));
  EXPECT_EQ(0, mode_test_api_->laser_points().GetNumberOfPoints());

  // Verify enabling the mode will start with a single point at the current
  // location.
  mode_test_api_->OnEnable();
  EXPECT_EQ(1, mode_test_api_->laser_points().GetNumberOfPoints());

  // Verify moving the mouse 4 times will add 4 more points.
  GetEventGenerator().MoveMouseToInHost(gfx::Point(25, 66));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(91, 38));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(34, 58));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(19, 71));
  EXPECT_EQ(5, mode_test_api_->laser_points().GetNumberOfPoints());

  // Verify disabling the mode will clear any active points.
  mode_test_api_->OnDisable();
  EXPECT_EQ(0, mode_test_api_->laser_points().GetNumberOfPoints());

  // Verify that the laser pointer does not add points while disabled.
  GetEventGenerator().MoveMouseToInHost(gfx::Point(34, 58));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(19, 71));
  EXPECT_EQ(0, mode_test_api_->laser_points().GetNumberOfPoints());

  // Verify that the laser pointer adds the last seen stylus point when enabled
  // even when stylus mode is disabled.
  GetEventGenerator().ExitPenPointerMode();
  mode_test_api_->OnEnable();
  EXPECT_EQ(1, mode_test_api_->laser_points().GetNumberOfPoints());
  EXPECT_EQ(GetEventGenerator().current_location(),
            mode_test_api_->laser_points().GetNewest().location);
  // Verify that the laser pointer does not add additional points when move
  // events are not from stylus.
  GetEventGenerator().MoveMouseToInHost(gfx::Point(34, 58));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(19, 71));
  EXPECT_EQ(1, mode_test_api_->laser_points().GetNumberOfPoints());
}
}  // namespace ash
