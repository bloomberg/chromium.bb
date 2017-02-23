// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene.h"

#define _USE_MATH_DEFINES  // For M_PI in MSVC.
#include <cmath>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"
#include "chrome/browser/android/vr_shell/ui_elements.h"
#include "chrome/browser/android/vr_shell/vr_math.h"
#include "testing/gtest/include/gtest/gtest.h"

#define TOLERANCE 0.0001

#define EXPECT_VEC3F_NEAR(a, b)     \
  EXPECT_NEAR(a.x, b.x, TOLERANCE); \
  EXPECT_NEAR(a.y, b.y, TOLERANCE); \
  EXPECT_NEAR(a.z, b.z, TOLERANCE);

namespace vr_shell {

namespace {

void addElement(UiScene* scene, int id) {
  std::unique_ptr<ContentRectangle> element(new ContentRectangle);
  element->id = id;
  scene->AddUiElement(element);
}

void addAnimation(UiScene* scene,
                  int element_id,
                  int animation_id,
                  Animation::Property property) {
  std::unique_ptr<Animation> animation(
      new Animation(animation_id, property,
                    std::unique_ptr<easing::Easing>(new easing::Linear()), {},
                    {1, 1, 1, 1}, 0, 1));
  scene->AddAnimation(element_id, animation);
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

  EXPECT_EQ(element->animations.size(), 0u);
  addAnimation(&scene, 0, 0, Animation::Property::SIZE);
  EXPECT_EQ(element->animations.size(), 1u);
  EXPECT_EQ(element->animations[0]->property, Animation::Property::SIZE);
  addAnimation(&scene, 0, 1, Animation::Property::SCALE);
  EXPECT_EQ(element->animations.size(), 2u);
  EXPECT_EQ(element->animations[1]->property, Animation::Property::SCALE);

  scene.RemoveAnimation(0, 0);
  EXPECT_EQ(element->animations.size(), 1u);
  EXPECT_EQ(element->animations[0]->property, Animation::Property::SCALE);
  scene.RemoveAnimation(0, 1);
  EXPECT_EQ(element->animations.size(), 0u);

  scene.RemoveAnimation(0, 0);
  EXPECT_EQ(element->animations.size(), 0u);
}

// This test creates a parent and child UI element, each with their own
// transformations, and ensures that the child's computed total transform
// incorporates the parent's transform as well as its own.
TEST(UiScene, ParentTransformAppliesToChild) {
  UiScene scene;

  // Add a parent element, with distinct transformations.
  // Size of the parent should be ignored by the child.
  std::unique_ptr<ContentRectangle> element(new ContentRectangle);
  element->id = 0;
  element->size = {1000, 1000, 1};
  element->scale = {3, 3, 1};
  element->rotation = {0, 0, 1, M_PI / 2};
  element->translation = {6, 1, 0};
  scene.AddUiElement(element);

  // Add a child to the parent, with different transformations.
  element.reset(new ContentRectangle);
  element->id = 1;
  element->parent_id = 0;
  element->size = {1, 1, 1};
  element->scale = {2, 2, 1};
  element->rotation = {0, 0, 1, M_PI / 2};
  element->translation = {3, 0, 0};
  scene.AddUiElement(element);
  const ContentRectangle* child = scene.GetUiElementById(1);

  const gvr::Vec3f origin({0, 0, 0});
  const gvr::Vec3f point({1, 0, 0});

  // Check resulting transform with no screen tilt.
  scene.UpdateTransforms(0, 0);
  auto new_origin = MatrixVectorMul(child->transform.to_world, origin);
  auto new_point = MatrixVectorMul(child->transform.to_world, point);
  EXPECT_VEC3F_NEAR(gvr::Vec3f({6, 10, 0}), new_origin);
  EXPECT_VEC3F_NEAR(gvr::Vec3f({0, 10, 0}), new_point);

  // Check with screen tilt (use 90 degrees for simplicity).
  scene.UpdateTransforms(M_PI / 2, 0);
  new_origin = MatrixVectorMul(child->transform.to_world, origin);
  new_point = MatrixVectorMul(child->transform.to_world, point);
  EXPECT_VEC3F_NEAR(gvr::Vec3f({6, 0, 10}), new_origin);
  EXPECT_VEC3F_NEAR(gvr::Vec3f({0, 0, 10}), new_point);
}

TEST(UiScene, Opacity) {
  UiScene scene;
  std::unique_ptr<ContentRectangle> element;

  element.reset(new ContentRectangle);
  element->id = 0;
  element->opacity = 0.5;
  scene.AddUiElement(element);

  element.reset(new ContentRectangle);
  element->id = 1;
  element->parent_id = 0;
  element->opacity = 0.5;
  scene.AddUiElement(element);

  scene.UpdateTransforms(0, 0);
  EXPECT_EQ(scene.GetUiElementById(0)->computed_opacity, 0.5f);
  EXPECT_EQ(scene.GetUiElementById(1)->computed_opacity, 0.25f);
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
  std::unique_ptr<ContentRectangle> element(new ContentRectangle);
  element->id = 0;
  element->size = {2, 2, 1};
  element->scale = {2, 2, 1};
  scene.AddUiElement(element);

  // Add a child to the parent, with anchoring.
  element.reset(new ContentRectangle);
  element->id = 1;
  element->parent_id = 0;
  element->x_anchoring = GetParam().x_anchoring;
  element->y_anchoring = GetParam().y_anchoring;
  scene.AddUiElement(element);

  scene.UpdateTransforms(0, 0);
  const ContentRectangle* child = scene.GetUiElementById(1);
  EXPECT_NEAR(child->GetCenter().x, GetParam().expected_x, TOLERANCE);
  EXPECT_NEAR(child->GetCenter().y, GetParam().expected_y, TOLERANCE);
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

TEST(UiScene, AddUiElementFromDictionary) {
  UiScene scene;
  addElement(&scene, 11);

  base::DictionaryValue dict;

  dict.SetInteger("id", 10);
  dict.SetInteger("parentId", 11);
  dict.SetBoolean("visible", false);
  dict.SetBoolean("hitTestable", false);
  dict.SetBoolean("lockToFov", true);
  dict.SetInteger("fillType", Fill::SPRITE);
  dict.SetInteger("xAnchoring", XAnchoring::XLEFT);
  dict.SetInteger("yAnchoring", YAnchoring::YTOP);
  dict.SetDouble("opacity", 0.357);

  dict.SetInteger("copyRectX", 100);
  dict.SetInteger("copyRectY", 101);
  dict.SetInteger("copyRectWidth", 102);
  dict.SetInteger("copyRectHeight", 103);

  dict.SetDouble("sizeX", 200);
  dict.SetDouble("sizeY", 201);

  dict.SetDouble("scaleX", 300);
  dict.SetDouble("scaleY", 301);
  dict.SetDouble("scaleZ", 302);

  dict.SetDouble("rotationX", 400);
  dict.SetDouble("rotationY", 401);
  dict.SetDouble("rotationZ", 402);
  dict.SetDouble("rotationAngle", 403);

  dict.SetDouble("translationX", 500);
  dict.SetDouble("translationY", 501);
  dict.SetDouble("translationZ", 502);

  scene.AddUiElementFromDict(dict);
  const auto* element = scene.GetUiElementById(10);
  EXPECT_NE(element, nullptr);

  EXPECT_EQ(element->id, 10);
  EXPECT_EQ(element->parent_id, 11);
  EXPECT_EQ(element->visible, false);
  EXPECT_EQ(element->hit_testable, false);
  EXPECT_EQ(element->lock_to_fov, true);
  EXPECT_EQ(element->fill, Fill::SPRITE);
  EXPECT_EQ(element->x_anchoring, XAnchoring::XLEFT);
  EXPECT_EQ(element->y_anchoring, YAnchoring::YTOP);
  EXPECT_FLOAT_EQ(element->opacity, 0.357);

  EXPECT_EQ(element->copy_rect.x, 100);
  EXPECT_EQ(element->copy_rect.y, 101);
  EXPECT_EQ(element->copy_rect.width, 102);
  EXPECT_EQ(element->copy_rect.height, 103);

  EXPECT_FLOAT_EQ(element->size.x, 200);
  EXPECT_FLOAT_EQ(element->size.y, 201);
  EXPECT_FLOAT_EQ(element->size.z, 1);

  EXPECT_FLOAT_EQ(element->scale.x, 300);
  EXPECT_FLOAT_EQ(element->scale.y, 301);
  EXPECT_FLOAT_EQ(element->scale.z, 302);

  EXPECT_FLOAT_EQ(element->rotation.x, 400);
  EXPECT_FLOAT_EQ(element->rotation.y, 401);
  EXPECT_FLOAT_EQ(element->rotation.z, 402);
  EXPECT_FLOAT_EQ(element->rotation.angle, 403);

  EXPECT_FLOAT_EQ(element->translation.x, 500);
  EXPECT_FLOAT_EQ(element->translation.y, 501);
  EXPECT_FLOAT_EQ(element->translation.z, 502);
}

TEST(UiScene, AddUiElementFromDictionary_Fill) {
  UiScene scene;
  base::DictionaryValue dict;

  dict.SetInteger("copyRectX", 1);
  dict.SetInteger("copyRectY", 2);
  dict.SetInteger("copyRectWidth", 3);
  dict.SetInteger("copyRectHeight", 4);

  base::DictionaryValue edge_color;
  edge_color.SetDouble("r", 0.1);
  edge_color.SetDouble("g", 0.2);
  edge_color.SetDouble("b", 0.3);
  edge_color.SetDouble("a", 0.4);

  base::DictionaryValue center_color;
  center_color.SetDouble("r", 0.5);
  center_color.SetDouble("g", 0.6);
  center_color.SetDouble("b", 0.7);
  center_color.SetDouble("a", 0.8);

  // Test SPRITE filling.
  dict.SetInteger("id", 9);
  dict.SetInteger("fillType", Fill::SPRITE);
  scene.AddUiElementFromDict(dict);
  const auto* element = scene.GetUiElementById(9);

  EXPECT_EQ(element->fill, Fill::SPRITE);
  EXPECT_EQ(element->copy_rect.x, 1);
  EXPECT_EQ(element->copy_rect.y, 2);
  EXPECT_EQ(element->copy_rect.width, 3);
  EXPECT_EQ(element->copy_rect.height, 4);

  // Test OPAQUE_GRADIENT filling.
  dict.Clear();
  dict.SetInteger("id", 10);
  dict.SetInteger("fillType", Fill::OPAQUE_GRADIENT);
  dict.Set("edgeColor", edge_color.DeepCopy());
  dict.Set("centerColor", center_color.DeepCopy());
  scene.AddUiElementFromDict(dict);
  element = scene.GetUiElementById(10);

  EXPECT_EQ(element->fill, Fill::OPAQUE_GRADIENT);
  EXPECT_FLOAT_EQ(element->edge_color.r, 0.1);
  EXPECT_FLOAT_EQ(element->edge_color.g, 0.2);
  EXPECT_FLOAT_EQ(element->edge_color.b, 0.3);
  EXPECT_FLOAT_EQ(element->edge_color.a, 0.4);
  EXPECT_FLOAT_EQ(element->center_color.r, 0.5);
  EXPECT_FLOAT_EQ(element->center_color.g, 0.6);
  EXPECT_FLOAT_EQ(element->center_color.b, 0.7);
  EXPECT_FLOAT_EQ(element->center_color.a, 0.8);

  // Test GRID_GRADIENT filling.
  dict.Clear();
  dict.SetInteger("id", 11);
  dict.SetInteger("fillType", Fill::GRID_GRADIENT);
  dict.Set("edgeColor", edge_color.DeepCopy());
  dict.Set("centerColor", center_color.DeepCopy());
  dict.SetInteger("gridlineCount", 10);
  scene.AddUiElementFromDict(dict);
  element = scene.GetUiElementById(11);

  EXPECT_EQ(element->fill, Fill::GRID_GRADIENT);
  EXPECT_FLOAT_EQ(element->edge_color.r, 0.1);
  EXPECT_FLOAT_EQ(element->edge_color.g, 0.2);
  EXPECT_FLOAT_EQ(element->edge_color.b, 0.3);
  EXPECT_FLOAT_EQ(element->edge_color.a, 0.4);
  EXPECT_FLOAT_EQ(element->center_color.r, 0.5);
  EXPECT_FLOAT_EQ(element->center_color.g, 0.6);
  EXPECT_FLOAT_EQ(element->center_color.b, 0.7);
  EXPECT_FLOAT_EQ(element->center_color.a, 0.8);
  EXPECT_EQ(element->gridline_count, 10);

  // Test CONTENT filling.
  dict.Clear();
  dict.SetInteger("id", 12);
  dict.SetInteger("fillType", Fill::CONTENT);
  scene.AddUiElementFromDict(dict);
  element = scene.GetUiElementById(12);

  EXPECT_EQ(element->fill, Fill::CONTENT);
}

TEST(UiScene, AddAnimationFromDictionary) {
  UiScene scene;
  addElement(&scene, 0);

  base::DictionaryValue dict;

  dict.SetInteger("id", 10);
  dict.SetInteger("meshId", 0);
  dict.SetDouble("startInMillis", 12345);
  dict.SetDouble("durationMillis", 54321);
  dict.SetInteger("property", Animation::Property::ROTATION);

  std::unique_ptr<base::DictionaryValue> easing(new base::DictionaryValue);
  easing->SetInteger("type", vr_shell::easing::EasingType::CUBICBEZIER);
  easing->SetInteger("p1x", 101);
  easing->SetInteger("p1y", 101);
  easing->SetInteger("p2x", 101);
  easing->SetInteger("p2y", 101);
  dict.Set("easing", std::move(easing));

  std::unique_ptr<base::DictionaryValue> to(new base::DictionaryValue);
  to->SetInteger("x", 200);
  to->SetInteger("y", 201);
  to->SetInteger("z", 202);
  to->SetInteger("a", 203);
  dict.Set("to", std::move(to));

  std::unique_ptr<base::DictionaryValue> from(new base::DictionaryValue);
  from->SetInteger("x", 300);
  from->SetInteger("y", 301);
  from->SetInteger("z", 302);
  from->SetInteger("a", 303);
  dict.Set("from", std::move(from));

  scene.AddAnimationFromDict(dict, 10000000);
  const auto* element = scene.GetUiElementById(0);
  const auto* animation = element->animations[0].get();
  EXPECT_NE(animation, nullptr);

  EXPECT_EQ(animation->id, 10);

  EXPECT_FLOAT_EQ(animation->to[0], 200);
  EXPECT_FLOAT_EQ(animation->to[1], 201);
  EXPECT_FLOAT_EQ(animation->to[2], 202);
  EXPECT_FLOAT_EQ(animation->to[3], 203);

  EXPECT_FLOAT_EQ(animation->from[0], 300);
  EXPECT_FLOAT_EQ(animation->from[1], 301);
  EXPECT_FLOAT_EQ(animation->from[2], 302);
  EXPECT_FLOAT_EQ(animation->from[3], 303);

  EXPECT_EQ(animation->start, 22345000);
  EXPECT_EQ(animation->duration, 54321000);
}

}  // namespace vr_shell
