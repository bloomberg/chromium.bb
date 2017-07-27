// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/linear_layout.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(LinearLayout, HorizontalLayout) {
  LinearLayout layout(LinearLayout::kHorizontal);
  layout.set_margin(10);
  UiElement rect_a;
  rect_a.SetSize(10, 10);
  rect_a.SetVisible(true);
  layout.AddChild(&rect_a);

  // One element should require no position adjustment at all.
  layout.LayOutChildren();
  EXPECT_TRUE(rect_a.LocalTransform().IsIdentity());

  // Two elements should be centered and separated by the margin.
  UiElement rect_b;
  rect_b.SetSize(20, 20);
  rect_b.SetVisible(true);
  layout.AddChild(&rect_b);
  layout.LayOutChildren();

  gfx::Point3F position_a;
  rect_a.LocalTransform().TransformPoint(&position_a);

  gfx::Point3F position_b;
  rect_b.LocalTransform().TransformPoint(&position_b);

  EXPECT_FLOAT_EQ(-15.0f, position_a.x());
  EXPECT_FLOAT_EQ(0.0f, position_a.y());
  EXPECT_FLOAT_EQ(0.0f, position_a.z());

  EXPECT_FLOAT_EQ(10.0f, position_b.x());
  EXPECT_FLOAT_EQ(0.0f, position_b.y());
  EXPECT_FLOAT_EQ(0.0f, position_b.z());

  rect_a.SetVisible(false);
  layout.LayOutChildren();
  // The invisible child should not be accounted for in the layout.
  EXPECT_TRUE(rect_b.LocalTransform().IsIdentity());
}

TEST(LinearLayout, VerticalLayout) {
  LinearLayout layout(LinearLayout::kVertical);
  layout.set_margin(10);
  UiElement rect_a;
  rect_a.SetSize(10, 10);
  rect_a.SetVisible(true);
  layout.AddChild(&rect_a);

  // One element should require no position adjustment at all.
  layout.LayOutChildren();
  EXPECT_TRUE(rect_a.LocalTransform().IsIdentity());

  // Two elements should be centered and separated by the margin.
  UiElement rect_b;
  rect_b.SetSize(20, 20);
  rect_b.SetVisible(true);
  layout.AddChild(&rect_b);
  layout.LayOutChildren();

  gfx::Point3F position_a;
  rect_a.LocalTransform().TransformPoint(&position_a);

  gfx::Point3F position_b;
  rect_b.LocalTransform().TransformPoint(&position_b);

  EXPECT_FLOAT_EQ(0.0f, position_a.x());
  EXPECT_FLOAT_EQ(-15.0f, position_a.y());
  EXPECT_FLOAT_EQ(0.0f, position_a.z());

  EXPECT_FLOAT_EQ(0.0f, position_b.x());
  EXPECT_FLOAT_EQ(10.0f, position_b.y());
  EXPECT_FLOAT_EQ(0.0f, position_b.z());

  rect_a.SetVisible(false);
  layout.LayOutChildren();
  // The invisible child should not be accounted for in the layout.
  EXPECT_TRUE(rect_b.LocalTransform().IsIdentity());
}

}  // namespace vr
