// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/laser/laser_pointer_points.h"
#include "ash/laser/laser_pointer_points_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {

const int kTestPointsLifetimeSeconds = 5;

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
}  // namespace ash
