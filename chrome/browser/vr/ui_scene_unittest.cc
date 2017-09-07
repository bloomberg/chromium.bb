// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#define _USE_MATH_DEFINES  // For M_PI in MSVC.
#include <cmath>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_transform_operations.h"
#include "chrome/browser/vr/elements/viewport_aware_root.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform_util.h"

#define TOLERANCE 0.0001

#define EXPECT_VEC3F_NEAR(a, b)         \
  EXPECT_NEAR(a.x(), b.x(), TOLERANCE); \
  EXPECT_NEAR(a.y(), b.y(), TOLERANCE); \
  EXPECT_NEAR(a.z(), b.z(), TOLERANCE);

namespace vr {

namespace {

size_t NumElementsInSubtree(UiElement* element) {
  size_t count = 1;
  for (auto& child : element->children()) {
    count += NumElementsInSubtree(child.get());
  }
  return count;
}

}  // namespace

TEST(UiScene, AddRemoveElements) {
  UiScene scene;

  // Always start with the root element.
  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 1u);

  auto element = base::MakeUnique<UiElement>();
  element->set_draw_phase(kPhaseForeground);
  UiElement* parent = element.get();
  int parent_id = parent->id();
  scene.AddUiElement(kRoot, std::move(element));

  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 2u);

  element = base::MakeUnique<UiElement>();
  element->set_draw_phase(kPhaseForeground);
  UiElement* child = element.get();
  int child_id = child->id();

  parent->AddChild(std::move(element));

  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 3u);

  EXPECT_NE(scene.GetUiElementById(parent_id), nullptr);
  EXPECT_NE(scene.GetUiElementById(child_id), nullptr);
  EXPECT_EQ(scene.GetUiElementById(-1), nullptr);

  scene.RemoveUiElement(child_id);
  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 2u);
  EXPECT_EQ(scene.GetUiElementById(child_id), nullptr);

  scene.RemoveUiElement(parent_id);
  EXPECT_EQ(NumElementsInSubtree(&scene.root_element()), 1u);
  EXPECT_EQ(scene.GetUiElementById(parent_id), nullptr);

  // It is an error to remove an already-deleted element.
  EXPECT_DCHECK_DEATH(scene.RemoveUiElement(child_id));
}

// This test creates a parent and child UI element, each with their own
// transformations, and ensures that the child's computed total transform
// incorporates the parent's transform as well as its own.
TEST(UiScene, ParentTransformAppliesToChild) {
  UiScene scene;

  // Add a parent element, with distinct transformations.
  // Size of the parent should be ignored by the child.
  auto element = base::MakeUnique<UiElement>();
  UiElement* parent = element.get();
  element->SetSize(1000, 1000);

  UiElementTransformOperations operations;
  operations.SetTranslate(6, 1, 0);
  operations.SetRotate(0, 0, 1, 90);
  operations.SetScale(3, 3, 1);
  element->SetTransformOperations(operations);
  element->set_draw_phase(0);
  scene.AddUiElement(kRoot, std::move(element));

  // Add a child to the parent, with different transformations.
  element = base::MakeUnique<UiElement>();
  UiElementTransformOperations child_operations;
  child_operations.SetTranslate(3, 0, 0);
  child_operations.SetRotate(0, 0, 1, 90);
  child_operations.SetScale(2, 2, 1);
  element->SetTransformOperations(child_operations);
  element->set_draw_phase(0);
  UiElement* child = element.get();
  parent->AddChild(std::move(element));

  gfx::Point3F origin(0, 0, 0);
  gfx::Point3F point(1, 0, 0);

  scene.OnBeginFrame(MicrosecondsToTicks(1), gfx::Vector3dF(0.f, 0.f, -1.0f));
  child->world_space_transform().TransformPoint(&origin);
  child->world_space_transform().TransformPoint(&point);
  EXPECT_VEC3F_NEAR(gfx::Point3F(6, 10, 0), origin);
  EXPECT_VEC3F_NEAR(gfx::Point3F(0, 10, 0), point);
}

TEST(UiScene, Opacity) {
  UiScene scene;

  auto element = base::MakeUnique<UiElement>();
  UiElement* parent = element.get();
  element->SetOpacity(0.5);
  element->set_draw_phase(0);
  scene.AddUiElement(kRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  UiElement* child = element.get();
  element->SetOpacity(0.5);
  element->set_draw_phase(0);
  parent->AddChild(std::move(element));

  scene.OnBeginFrame(MicrosecondsToTicks(0), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_EQ(0.5f, parent->computed_opacity());
  EXPECT_EQ(0.25f, child->computed_opacity());
}

TEST(UiScene, ViewportAware) {
  UiScene scene;

  auto root = base::MakeUnique<ViewportAwareRoot>();
  UiElement* viewport_aware_root = root.get();
  root->set_draw_phase(0);
  scene.AddUiElement(kRoot, std::move(root));

  auto element = base::MakeUnique<UiElement>();
  UiElement* parent = element.get();
  element->set_viewport_aware(true);
  element->set_draw_phase(0);
  viewport_aware_root->AddChild(std::move(element));

  element = base::MakeUnique<UiElement>();
  UiElement* child = element.get();
  element->set_viewport_aware(false);
  element->set_draw_phase(0);
  parent->AddChild(std::move(element));

  scene.OnBeginFrame(MicrosecondsToTicks(0), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_TRUE(parent->computed_viewport_aware());
  EXPECT_TRUE(child->computed_viewport_aware());
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
  UiElement* parent = element.get();
  element->SetSize(2, 2);
  element->SetScale(2, 2, 1);
  element->set_draw_phase(0);
  scene.AddUiElement(kRoot, std::move(element));

  // Add a child to the parent, with anchoring.
  element = base::MakeUnique<UiElement>();
  UiElement* child = element.get();
  element->set_x_anchoring(GetParam().x_anchoring);
  element->set_y_anchoring(GetParam().y_anchoring);
  element->set_draw_phase(0);
  parent->AddChild(std::move(element));

  scene.OnBeginFrame(MicrosecondsToTicks(0), gfx::Vector3dF(0.f, 0.f, -1.0f));
  EXPECT_NEAR(GetParam().expected_x, child->GetCenter().x(), TOLERANCE);
  EXPECT_NEAR(GetParam().expected_y, child->GetCenter().y(), TOLERANCE);
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
