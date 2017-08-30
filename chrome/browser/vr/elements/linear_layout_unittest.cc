// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/linear_layout.h"

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

TEST(LinearLayout, HorizontalLayout) {
  LinearLayout layout(LinearLayout::kHorizontal);
  layout.set_margin(10);
  auto element = base::MakeUnique<UiElement>();
  UiElement* rect_a = element.get();
  rect_a->SetSize(10, 10);
  rect_a->SetVisible(true);
  layout.AddChild(std::move(element));

  // One element should require no position adjustment at all.
  layout.LayOutChildren();
  EXPECT_TRUE(rect_a->LocalTransform().IsIdentity());

  // Two elements should be centered and separated by the margin.
  element = base::MakeUnique<UiElement>();
  UiElement* rect_b = element.get();
  rect_b->SetSize(20, 20);
  rect_b->SetVisible(true);
  layout.AddChild(std::move(element));
  layout.LayOutChildren();

  gfx::Point3F position_a;
  rect_a->LocalTransform().TransformPoint(&position_a);

  gfx::Point3F position_b;
  rect_b->LocalTransform().TransformPoint(&position_b);

  EXPECT_FLOAT_EQ(-15.0f, position_a.x());
  EXPECT_FLOAT_EQ(0.0f, position_a.y());
  EXPECT_FLOAT_EQ(0.0f, position_a.z());

  EXPECT_FLOAT_EQ(10.0f, position_b.x());
  EXPECT_FLOAT_EQ(0.0f, position_b.y());
  EXPECT_FLOAT_EQ(0.0f, position_b.z());

  rect_a->set_requires_layout(false);
  layout.LayOutChildren();
  // The child that doesn't require layout should not have an impact.
  EXPECT_TRUE(rect_b->LocalTransform().IsIdentity());
}

TEST(LinearLayout, VerticalLayout) {
  LinearLayout layout(LinearLayout::kVertical);
  layout.set_margin(10);
  auto element = base::MakeUnique<UiElement>();
  UiElement* rect_a = element.get();
  rect_a->SetSize(10, 10);
  rect_a->SetVisible(true);
  layout.AddChild(std::move(element));

  // One element should require no position adjustment at all.
  layout.LayOutChildren();
  EXPECT_TRUE(rect_a->LocalTransform().IsIdentity());

  // Two elements should be centered and separated by the margin.
  element = base::MakeUnique<UiElement>();
  UiElement* rect_b = element.get();
  rect_b->SetSize(20, 20);
  rect_b->SetVisible(true);
  layout.AddChild(std::move(element));
  layout.LayOutChildren();

  gfx::Point3F position_a;
  rect_a->LocalTransform().TransformPoint(&position_a);

  gfx::Point3F position_b;
  rect_b->LocalTransform().TransformPoint(&position_b);

  EXPECT_FLOAT_EQ(0.0f, position_a.x());
  EXPECT_FLOAT_EQ(-15.0f, position_a.y());
  EXPECT_FLOAT_EQ(0.0f, position_a.z());

  EXPECT_FLOAT_EQ(0.0f, position_b.x());
  EXPECT_FLOAT_EQ(10.0f, position_b.y());
  EXPECT_FLOAT_EQ(0.0f, position_b.z());

  rect_a->set_requires_layout(false);
  layout.LayOutChildren();
  // The child that doesn't require layout should not have an impact.
  EXPECT_TRUE(rect_b->LocalTransform().IsIdentity());
}

}  // namespace vr
