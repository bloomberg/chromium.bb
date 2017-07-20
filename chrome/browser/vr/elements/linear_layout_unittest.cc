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
  EXPECT_TRUE(rect_a.transform_operations().Apply().IsIdentity());

  // Two elements should be centered and separated by the margin.
  UiElement rect_b;
  rect_b.SetSize(20, 20);
  rect_b.SetVisible(true);
  layout.AddChild(&rect_b);
  layout.LayOutChildren();

  EXPECT_FLOAT_EQ(-15.0f, rect_a.transform_operations().at(0).translate.x);
  EXPECT_FLOAT_EQ(0.0f, rect_a.transform_operations().at(0).translate.y);
  EXPECT_FLOAT_EQ(0.0f, rect_a.transform_operations().at(0).translate.z);

  EXPECT_FLOAT_EQ(10.0f, rect_b.transform_operations().at(0).translate.x);
  EXPECT_FLOAT_EQ(0.0f, rect_b.transform_operations().at(0).translate.y);
  EXPECT_FLOAT_EQ(0.0f, rect_b.transform_operations().at(0).translate.z);

  rect_a.SetVisible(false);
  layout.LayOutChildren();
  // The invisible child should not be accounted for in the layout.
  EXPECT_TRUE(rect_b.transform_operations().Apply().IsIdentity());
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
  EXPECT_TRUE(rect_a.transform_operations().Apply().IsIdentity());

  // Two elements should be centered and separated by the margin.
  UiElement rect_b;
  rect_b.SetSize(20, 20);
  rect_b.SetVisible(true);
  layout.AddChild(&rect_b);
  layout.LayOutChildren();

  const cc::TransformOperation& op_a =
      rect_a.transform_operations().at(UiElement::kLayoutOffsetIndex);

  EXPECT_FLOAT_EQ(0.0f, op_a.translate.x);
  EXPECT_FLOAT_EQ(-15.0f, op_a.translate.y);
  EXPECT_FLOAT_EQ(0.0f, op_a.translate.z);

  const cc::TransformOperation& op_b =
      rect_b.transform_operations().at(UiElement::kLayoutOffsetIndex);

  EXPECT_FLOAT_EQ(0.0f, op_b.translate.x);
  EXPECT_FLOAT_EQ(10.0f, op_b.translate.y);
  EXPECT_FLOAT_EQ(0.0f, op_b.translate.z);

  rect_a.SetVisible(false);
  layout.LayOutChildren();
  // The invisible child should not be accounted for in the layout.
  EXPECT_TRUE(rect_b.transform_operations().Apply().IsIdentity());
}

}  // namespace vr
