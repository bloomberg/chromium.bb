// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_controller.h"
#include "ash/laser/laser_pointer_controller_test_api.h"
#include "ash/laser/laser_pointer_points_test_api.h"
#include "ash/laser/laser_pointer_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

const int kTestPointsLifetimeSeconds = 5;

// TODO(sammiequon): Move this test into a different file. See
// http://crbug.com/646953.
class LaserPointerPointsTest : public test::AshTestBase {
 public:
  LaserPointerPointsTest()
      : points_(base::TimeDelta::FromSeconds(kTestPointsLifetimeSeconds)) {}

  ~LaserPointerPointsTest() override {}

 protected:
  LaserPointerPoints points_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LaserPointerPointsTest);
};

class LaserPointerControllerTest : public test::AshTestBase {
 public:
  LaserPointerControllerTest() {}
  ~LaserPointerControllerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    controller_.reset(new LaserPointerController());
  }

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<LaserPointerController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LaserPointerControllerTest);
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
  LaserPointerPointsTestApi points_test_api_(&points_);

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
// points from stylus movements as expected.
TEST_F(LaserPointerControllerTest, LaserPointerRenderer) {
  LaserPointerControllerTestApi controller_test_api_(controller_.get());

  // The laser pointer mode only works with stylus.
  GetEventGenerator().EnterPenPointerMode();

  // When disabled the laser pointer should not be showing.
  GetEventGenerator().MoveMouseToInHost(gfx::Point(1, 1));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that by enabling the mode, the laser pointer should still not be
  // showing.
  controller_test_api_.SetEnabled(true);
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify moving the stylus 4 times will not display the laser pointer.
  GetEventGenerator().MoveMouseToInHost(gfx::Point(2, 2));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(3, 3));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(4, 4));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify pressing the stylus will show the laser pointer and add a point.
  GetEventGenerator().PressLeftButton();
  EXPECT_TRUE(controller_test_api_.IsShowingLaserPointer());
  EXPECT_EQ(1, controller_test_api_.laser_points().GetNumberOfPoints());

  // Verify dragging the stylus 2 times will add 2 more points.
  GetEventGenerator().MoveMouseToInHost(gfx::Point(6, 6));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(7, 7));
  EXPECT_EQ(3, controller_test_api_.laser_points().GetNumberOfPoints());

  // Verify releasing the stylus hides the laser pointer.
  GetEventGenerator().ReleaseLeftButton();
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that disabling the mode does not display the laser pointer.
  controller_test_api_.SetEnabled(false);
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that disabling the mode while laser pointer is displayed does not
  // display the laser pointer.
  controller_test_api_.SetEnabled(true);
  GetEventGenerator().PressLeftButton();
  GetEventGenerator().MoveMouseToInHost(gfx::Point(6, 6));
  EXPECT_TRUE(controller_test_api_.IsShowingLaserPointer());
  controller_test_api_.SetEnabled(false);
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that the laser pointer does not add points while disabled.
  GetEventGenerator().PressLeftButton();
  GetEventGenerator().MoveMouseToInHost(gfx::Point(8, 8));
  GetEventGenerator().ReleaseLeftButton();
  GetEventGenerator().MoveMouseToInHost(gfx::Point(9, 9));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that the laser pointer does not get shown if points are not coming
  // from the stylus, even when enabled.
  GetEventGenerator().ExitPenPointerMode();
  controller_test_api_.SetEnabled(true);
  GetEventGenerator().PressLeftButton();
  GetEventGenerator().MoveMouseToInHost(gfx::Point(10, 10));
  GetEventGenerator().MoveMouseToInHost(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());
  GetEventGenerator().ReleaseLeftButton();
}
}  // namespace ash
