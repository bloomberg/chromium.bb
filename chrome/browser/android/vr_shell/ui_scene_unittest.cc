// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene.h"

#define _USE_MATH_DEFINES  // For M_PI in MSVC.
#include <cmath>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "testing/gtest/include/gtest/gtest.h"

#define TOLERANCE 0.0001

#define EXPECT_VEC3F_NEAR(a, b)         \
  EXPECT_NEAR(a.x(), b.x(), TOLERANCE); \
  EXPECT_NEAR(a.y(), b.y(), TOLERANCE); \
  EXPECT_NEAR(a.z(), b.z(), TOLERANCE);

namespace vr_shell {

namespace {

base::TimeTicks usToTicks(uint64_t us) {
  return base::TimeTicks::FromInternalValue(us);
}

base::TimeDelta usToDelta(uint64_t us) {
  return base::TimeDelta::FromInternalValue(us);
}

void addElement(UiScene* scene, int id) {
  auto element = base::MakeUnique<UiElement>();
  element->set_id(id);
  scene->AddUiElement(std::move(element));
}

void addAnimation(UiScene* scene,
                  int element_id,
                  int animation_id,
                  Animation::Property property) {
  std::unique_ptr<easing::Easing> easing = base::MakeUnique<easing::Linear>();
  std::vector<float> from;
  std::vector<float> to = {1, 1, 1, 1};
  auto animation =
      base::MakeUnique<Animation>(animation_id, property, std::move(easing),
                                  from, to, usToTicks(0), usToDelta(1));
  scene->AddAnimation(element_id, std::move(animation));
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

TEST(UiScene, AddRemoveAnimations) {
  UiScene scene;
  addElement(&scene, 0);
  auto* element = scene.GetUiElementById(0);

  EXPECT_EQ(element->animations().size(), 0u);
  addAnimation(&scene, 0, 0, Animation::Property::SIZE);
  EXPECT_EQ(element->animations().size(), 1u);
  EXPECT_EQ(element->animations()[0]->property, Animation::Property::SIZE);
  addAnimation(&scene, 0, 1, Animation::Property::SCALE);
  EXPECT_EQ(element->animations().size(), 2u);
  EXPECT_EQ(element->animations()[1]->property, Animation::Property::SCALE);

  scene.RemoveAnimation(0, 0);
  EXPECT_EQ(element->animations().size(), 1u);
  EXPECT_EQ(element->animations()[0]->property, Animation::Property::SCALE);
  scene.RemoveAnimation(0, 1);
  EXPECT_EQ(element->animations().size(), 0u);

  scene.RemoveAnimation(0, 0);
  EXPECT_EQ(element->animations().size(), 0u);
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
  element->set_size({1000, 1000, 1});
  element->set_scale({3, 3, 1});
  element->set_rotation(gfx::Quaternion(gfx::Vector3dF(0, 0, 1), M_PI / 2));
  element->set_translation({6, 1, 0});
  scene.AddUiElement(std::move(element));

  // Add a child to the parent, with different transformations.
  element = base::MakeUnique<UiElement>();
  element->set_id(1);
  element->set_parent_id(0);
  element->set_size({1, 1, 1});
  element->set_scale({2, 2, 1});
  element->set_rotation(gfx::Quaternion(gfx::Vector3dF(0, 0, 1), M_PI / 2));
  element->set_translation({3, 0, 0});
  scene.AddUiElement(std::move(element));
  const UiElement* child = scene.GetUiElementById(1);

  gfx::Point3F origin(0, 0, 0);
  gfx::Point3F point(1, 0, 0);

  scene.OnBeginFrame(usToTicks(0));
  child->TransformMatrix().TransformPoint(&origin);
  child->TransformMatrix().TransformPoint(&point);
  EXPECT_VEC3F_NEAR(gfx::Point3F(6, 10, 0), origin);
  EXPECT_VEC3F_NEAR(gfx::Point3F(0, 10, 0), point);
}

TEST(UiScene, Opacity) {
  UiScene scene;

  auto element = base::MakeUnique<UiElement>();
  element->set_id(0);
  element->set_opacity(0.5);
  scene.AddUiElement(std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_id(1);
  element->set_parent_id(0);
  element->set_opacity(0.5);
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
  element->set_size({2, 2, 1});
  element->set_scale({2, 2, 1});
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

}  // namespace vr_shell
