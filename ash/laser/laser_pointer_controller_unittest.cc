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
  // should get removed. The age of the point is a number between 0.0 and 1.0,
  // with 0.0 specifying a newly added point and 1.0 specifying the age of a
  // point added |kTestPointsLifetime| ago.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, points_test_api_.GetNumberOfPoints());
  EXPECT_FLOAT_EQ(0.0, points_test_api_.GetPointAtIndex(0).age);

  // Verify when we move forward in time by one second, the age of the last
  // point, added one second ago is 1 / |kTestPointsLifetime|.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(2, points_test_api_.GetNumberOfPoints());
  EXPECT_FLOAT_EQ(0.2, points_test_api_.GetPointAtIndex(0).age);
  EXPECT_FLOAT_EQ(0.0, points_test_api_.GetPointAtIndex(1).age);
  // Verify adding a point 10 seconds later will clear all other points, since
  // they are older than 5 seconds.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(1, points_test_api_.GetNumberOfPoints());

  // Verify adding 3 points one second apart each will add 3 points to the
  // collection, since all 4 points are younger than 5 seconds. All 4 points are
  // added 1 second apart so their age should be 0.2 apart.
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  points_test_api_.MoveForwardInTime(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(4, points_test_api_.GetNumberOfPoints());
  EXPECT_FLOAT_EQ(0.6, points_test_api_.GetPointAtIndex(0).age);
  EXPECT_FLOAT_EQ(0.4, points_test_api_.GetPointAtIndex(1).age);
  EXPECT_FLOAT_EQ(0.2, points_test_api_.GetPointAtIndex(2).age);
  EXPECT_FLOAT_EQ(0.0, points_test_api_.GetPointAtIndex(3).age);

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
  GetEventGenerator().MoveTouch(gfx::Point(1, 1));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that by enabling the mode, the laser pointer should still not be
  // showing.
  controller_test_api_.SetEnabled(true);
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify moving the stylus 4 times will not display the laser pointer.
  GetEventGenerator().MoveTouch(gfx::Point(2, 2));
  GetEventGenerator().MoveTouch(gfx::Point(3, 3));
  GetEventGenerator().MoveTouch(gfx::Point(4, 4));
  GetEventGenerator().MoveTouch(gfx::Point(5, 5));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify pressing the stylus will show the laser pointer and add a point but
  // will not activate fading out.
  GetEventGenerator().PressTouch();
  EXPECT_TRUE(controller_test_api_.IsShowingLaserPointer());
  EXPECT_FALSE(controller_test_api_.IsFadingAway());
  EXPECT_EQ(1, controller_test_api_.laser_points().GetNumberOfPoints());

  // Verify dragging the stylus 2 times will add 2 more points.
  GetEventGenerator().MoveTouch(gfx::Point(6, 6));
  GetEventGenerator().MoveTouch(gfx::Point(7, 7));
  EXPECT_EQ(3, controller_test_api_.laser_points().GetNumberOfPoints());

  // Verify releasing the stylus still shows the laser pointer, which is fading
  // away.
  GetEventGenerator().ReleaseTouch();
  EXPECT_TRUE(controller_test_api_.IsShowingLaserPointer());
  EXPECT_TRUE(controller_test_api_.IsFadingAway());

  // Verify that disabling the mode does not display the laser pointer.
  controller_test_api_.SetEnabled(false);
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that disabling the mode while laser pointer is displayed does not
  // display the laser pointer.
  controller_test_api_.SetIsFadingAway(false);
  controller_test_api_.SetEnabled(true);
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(6, 6));
  EXPECT_TRUE(controller_test_api_.IsShowingLaserPointer());
  controller_test_api_.SetEnabled(false);
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that the laser pointer does not add points while disabled.
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(8, 8));
  GetEventGenerator().ReleaseTouch();
  GetEventGenerator().MoveTouch(gfx::Point(9, 9));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());

  // Verify that the laser pointer does not get shown if points are not coming
  // from the stylus, even when enabled.
  GetEventGenerator().ExitPenPointerMode();
  controller_test_api_.SetEnabled(true);
  GetEventGenerator().PressTouch();
  GetEventGenerator().MoveTouch(gfx::Point(10, 10));
  GetEventGenerator().MoveTouch(gfx::Point(11, 11));
  EXPECT_FALSE(controller_test_api_.IsShowingLaserPointer());
  GetEventGenerator().ReleaseTouch();
}
}  // namespace ash
