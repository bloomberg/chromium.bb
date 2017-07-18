// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#define _USE_MATH_DEFINES  // For M_PI in MSVC.
#include <cmath>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform_util.h"

#define TOLERANCE 0.0001

#define EXPECT_VEC3F_NEAR(a, b)         \
  EXPECT_NEAR(a.x(), b.x(), TOLERANCE); \
  EXPECT_NEAR(a.y(), b.y(), TOLERANCE); \
  EXPECT_NEAR(a.z(), b.z(), TOLERANCE);

namespace vr {

namespace {

base::TimeTicks usToTicks(uint64_t us) {
  return base::TimeTicks::FromInternalValue(us);
}

void addElement(UiScene* scene, int id) {
  auto element = base::MakeUnique<UiElement>();
  element->set_id(id);
  scene->AddUiElement(std::move(element));
}

}  // namespace

TEST(UiScene, AddRemoveElements) {
  UiScene scene;

  EXPECT_EQ(scene.GetUiElements().size(), 0u);
  addElement(&scene, 0);
  EXPECT_EQ(scene.GetUiElements().size(), 1u);
  addElement(&scene, 99);
  EXPECT_EQ(scene.GetUiElements().size(), 2u);

  EXPECT_NE(scene.GetUiElementById(0), nullptr);
  EXPECT_NE(scene.GetUiElementById(99), nullptr);
  EXPECT_EQ(scene.GetUiElementById(1), nullptr);

  scene.RemoveUiElement(0);
  EXPECT_EQ(scene.GetUiElements().size(), 1u);
  EXPECT_EQ(scene.GetUiElementById(0), nullptr);
  scene.RemoveUiElement(99);
  EXPECT_EQ(scene.GetUiElements().size(), 0u);
  EXPECT_EQ(scene.GetUiElementById(99), nullptr);

  scene.RemoveUiElement(0);
  scene.RemoveUiElement(99);
  EXPECT_EQ(scene.GetUiElements().size(), 0u);
}

// This test creates a parent and child UI element, each with their own
// transformations, and ensures that the child's computed total transform
// incorporates the parent's transform as well as its own.
TEST(UiScene, ParentTransformAppliesToChild) {
  UiScene scene;

  // Add a parent element, with distinct transformations.
  // Size of the parent should be ignored by the child.
  auto element = base::MakeUnique<UiElement>();
  element->set_id(0);
  element->SetSize(1000, 1000);

  cc::TransformOperations operations;
  operations.AppendTranslate(6, 1, 0);
  operations.AppendRotate(0, 0, 1, 180 / 2);
  operations.AppendScale(3, 3, 1);
  element->SetTransformOperations(operations);
  scene.AddUiElement(std::move(element));

  // Add a child to the parent, with different transformations.
  element = base::MakeUnique<UiElement>();
  element->set_id(1);
  element->set_parent_id(0);
  cc::TransformOperations child_operations;
  child_operations.AppendTranslate(3, 0, 0);
  child_operations.AppendRotate(0, 0, 1, 180 / 2);
  child_operations.AppendScale(2, 2, 1);
  element->SetTransformOperations(child_operations);
  scene.AddUiElement(std::move(element));
  const UiElement* child = scene.GetUiElementById(1);

  gfx::Point3F origin(0, 0, 0);
  gfx::Point3F point(1, 0, 0);

  scene.OnBeginFrame(usToTicks(1));
  child->screen_space_transform().TransformPoint(&origin);
  child->screen_space_transform().TransformPoint(&point);
  EXPECT_VEC3F_NEAR(gfx::Point3F(6, 10, 0), origin);
  EXPECT_VEC3F_NEAR(gfx::Point3F(0, 10, 0), point);
}

TEST(UiScene, Opacity) {
  UiScene scene;

  auto element = base::MakeUnique<UiElement>();
  element->set_id(0);
  element->SetOpacity(0.5);
  scene.AddUiElement(std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_id(1);
  element->set_parent_id(0);
  element->SetOpacity(0.5);
  scene.AddUiElement(std::move(element));

  scene.OnBeginFrame(usToTicks(0));
  EXPECT_EQ(0.5f, scene.GetUiElementById(0)->computed_opacity());
  EXPECT_EQ(0.25f, scene.GetUiElementById(1)->computed_opacity());
}

TEST(UiScene, LockToFov) {
  UiScene scene;

  auto element = base::MakeUnique<UiElement>();
  element->set_id(0);
  element->set_lock_to_fov(true);
  scene.AddUiElement(std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_id(1);
  element->set_parent_id(0);
  element->set_lock_to_fov(false);
  scene.AddUiElement(std::move(element));

  scene.OnBeginFrame(usToTicks(0));
  EXPECT_TRUE(scene.GetUiElementById(0)->computed_lock_to_fov());
  EXPECT_TRUE(scene.GetUiElementById(1)->computed_lock_to_fov());
}

typedef struct {
  XAnchoring x_anchoring;
  YAnchoring y_anchoring;
  float expected_x;
  float expected_y;
} AnchoringTestCase;

class AnchoringTest : public ::testing::TestWithParam<AnchoringTestCase> {};

TEST_P(AnchoringTest, VerifyCorrectPosition) {
  UiScene scene;

  // Create a parent element with non-unity size and scale.
  auto element = base::MakeUnique<UiElement>();
  element->set_id(0);
  element->SetSize(2, 2);
  element->SetScale(2, 2, 1);
  scene.AddUiElement(std::move(element));

  // Add a child to the parent, with anchoring.
  element = base::MakeUnique<UiElement>();
  element->set_id(1);
  element->set_parent_id(0);
  element->set_x_anchoring(GetParam().x_anchoring);
  element->set_y_anchoring(GetParam().y_anchoring);
  scene.AddUiElement(std::move(element));

  scene.OnBeginFrame(usToTicks(0));
  const UiElement* child = scene.GetUiElementById(1);
  EXPECT_NEAR(GetParam().expected_x, child->GetCenter().x(), TOLERANCE);
  EXPECT_NEAR(GetParam().expected_y, child->GetCenter().y(), TOLERANCE);
  scene.RemoveUiElement(1);
}

const std::vector<AnchoringTestCase> anchoring_test_cases = {
    {XAnchoring::XNONE, YAnchoring::YNONE, 0, 0},
    {XAnchoring::XLEFT, YAnchoring::YNONE, -2, 0},
    {XAnchoring::XRIGHT, YAnchoring::YNONE, 2, 0},
    {XAnchoring::XNONE, YAnchoring::YTOP, 0, 2},
    {XAnchoring::XNONE, YAnchoring::YBOTTOM, 0, -2},
    {XAnchoring::XLEFT, YAnchoring::YTOP, -2, 2},
};

INSTANTIATE_TEST_CASE_P(AnchoringTestCases,
                        AnchoringTest,
                        ::testing::ValuesIn(anchoring_test_cases));

}  // namespace vr
