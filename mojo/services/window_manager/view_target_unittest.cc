// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/view_target.h"

#include <set>

#include "mojo/services/window_manager/window_manager_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"
#include "ui/gfx/geometry/rect.h"

namespace window_manager {

using ViewTargetTest = testing::Test;

// V1
//  +-- V2
//       +-- V3
TEST_F(ViewTargetTest, GetRoot) {
  TestView v1(1, gfx::Rect(20, 20, 400, 400));
  TestView v2(2, gfx::Rect(10, 10, 350, 350));
  TestView v3(3, gfx::Rect(10, 10, 100, 100));
  v1.AddChild(&v2);
  v2.AddChild(&v3);

  EXPECT_EQ(ViewTarget::TargetFromView(&v1),
            ViewTarget::TargetFromView(&v1)->GetRoot());
  EXPECT_EQ(ViewTarget::TargetFromView(&v1),
            ViewTarget::TargetFromView(&v2)->GetRoot());
  EXPECT_EQ(ViewTarget::TargetFromView(&v1),
            ViewTarget::TargetFromView(&v3)->GetRoot());
}

// V1
//  +-- V2
TEST_F(ViewTargetTest, ConvertPointToTarget_Simple) {
  TestView v1(1, gfx::Rect(20, 20, 400, 400));
  TestView v2(2, gfx::Rect(10, 10, 350, 350));
  v1.AddChild(&v2);

  ViewTarget* t1 = v1.target();
  ViewTarget* t2 = v2.target();

  gfx::Point point1_in_t2_coords(5, 5);
  ViewTarget::ConvertPointToTarget(t2, t1, &point1_in_t2_coords);
  gfx::Point point1_in_t1_coords(15, 15);
  EXPECT_EQ(point1_in_t1_coords, point1_in_t2_coords);

  gfx::Point point2_in_t1_coords(5, 5);
  ViewTarget::ConvertPointToTarget(t1, t2, &point2_in_t1_coords);
  gfx::Point point2_in_t2_coords(-5, -5);
  EXPECT_EQ(point2_in_t2_coords, point2_in_t1_coords);
}

// V1
//  +-- V2
//       +-- V3
TEST_F(ViewTargetTest, ConvertPointToTarget_Medium) {
  TestView v1(1, gfx::Rect(20, 20, 400, 400));
  TestView v2(2, gfx::Rect(10, 10, 350, 350));
  TestView v3(3, gfx::Rect(10, 10, 100, 100));
  v1.AddChild(&v2);
  v2.AddChild(&v3);

  ViewTarget* t1 = v1.target();
  ViewTarget* t3 = v3.target();

  gfx::Point point1_in_t3_coords(5, 5);
  ViewTarget::ConvertPointToTarget(t3, t1, &point1_in_t3_coords);
  gfx::Point point1_in_t1_coords(25, 25);
  EXPECT_EQ(point1_in_t1_coords, point1_in_t3_coords);

  gfx::Point point2_in_t1_coords(5, 5);
  ViewTarget::ConvertPointToTarget(t1, t3, &point2_in_t1_coords);
  gfx::Point point2_in_t3_coords(-15, -15);
  EXPECT_EQ(point2_in_t3_coords, point2_in_t1_coords);
}

}  // namespace window_manager
