// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/viewport_aware_root.h"

#include <cmath>

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

const float kThreshold = ViewportAwareRoot::kViewportRotationTriggerDegrees;

bool FloatNearlyEqual(float a, float b) {
  return std::abs(a - b) < kEpsilon;
}

bool Point3FAreNearlyEqual(const gfx::Point3F& lhs, const gfx::Point3F& rhs) {
  return FloatNearlyEqual(lhs.x(), rhs.x()) &&
         FloatNearlyEqual(lhs.y(), rhs.y()) &&
         FloatNearlyEqual(lhs.z(), rhs.z());
}

bool MatricesAreNearlyEqual(const gfx::Transform& lhs,
                            const gfx::Transform& rhs) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (!FloatNearlyEqual(lhs.matrix().get(row, col),
                            rhs.matrix().get(row, col))) {
        return false;
      }
    }
  }
  return true;
}

void RotateAboutYAxis(float degrees, gfx::Vector3dF* out) {
  gfx::Transform transform;
  transform.RotateAboutYAxis(degrees);
  transform.TransformVector(out);
}

void CheckRotateClockwiseAndReverse(const gfx::Vector3dF& initial_look_at) {
  ViewportAwareRoot root;
  gfx::Vector3dF look_at(initial_look_at);
  gfx::Transform expected;

  root.AdjustRotationForHeadPose(look_at);
  EXPECT_TRUE(MatricesAreNearlyEqual(expected, root.LocalTransform()));

  float total_rotation = 0.f;
  RotateAboutYAxis(kThreshold - 1.f, &look_at);
  total_rotation += (kThreshold - 1.f);
  root.AdjustRotationForHeadPose(look_at);
  // Rotating less than kThreshold should yield identity local transform.
  EXPECT_TRUE(MatricesAreNearlyEqual(expected, root.LocalTransform()));

  // Rotate look_at clockwise again kThreshold degrees.
  RotateAboutYAxis(kThreshold, &look_at);
  total_rotation += kThreshold;
  root.AdjustRotationForHeadPose(look_at);

  // Rotating more than kThreshold should reposition to element to center.
  // We have rotated kThreshold - 1 + kThreshold degrees in total.
  expected.RotateAboutYAxis(total_rotation);
  EXPECT_TRUE(MatricesAreNearlyEqual(expected, root.LocalTransform()));

  // Since we rotated, reset total rotation.
  total_rotation = 0.f;

  // Rotate look_at counter clockwise kThreshold-2 degrees.
  RotateAboutYAxis(-(kThreshold - 2.f), &look_at);
  total_rotation -= (kThreshold - 2.f);
  root.AdjustRotationForHeadPose(look_at);
  // Rotating opposite direction within the kThreshold should not reposition.
  EXPECT_TRUE(MatricesAreNearlyEqual(expected, root.LocalTransform()));

  // Rotate look_at counter clockwise again kThreshold degrees.
  RotateAboutYAxis(-kThreshold, &look_at);
  total_rotation -= kThreshold;
  root.AdjustRotationForHeadPose(look_at);
  expected.RotateAboutYAxis(total_rotation);
  // Rotating opposite direction passing the kThreshold should reposition.
  EXPECT_TRUE(MatricesAreNearlyEqual(expected, root.LocalTransform()));
}

}  // namespace

TEST(ViewportAwareRoot, TestAdjustRotationForHeadPose) {
  CheckRotateClockwiseAndReverse(gfx::Vector3dF{0.f, 0.f, -1.f});

  CheckRotateClockwiseAndReverse(
      gfx::Vector3dF{0.f, std::sin(1), -std::cos(1)});

  CheckRotateClockwiseAndReverse(
      gfx::Vector3dF{0.f, -std::sin(1.5), -std::cos(1.5)});
}

TEST(ViewportAwareRoot, ChildElementsRepositioned) {
  UiScene scene;

  auto root = base::MakeUnique<ViewportAwareRoot>();
  root->set_id(0);
  root->set_draw_phase(0);
  scene.AddUiElement(std::move(root));

  auto element = base::MakeUnique<UiElement>();
  element->set_id(1);
  element->set_viewport_aware(true);
  element->set_draw_phase(0);
  element->SetTranslate(0.f, 0.f, -1.f);
  scene.GetUiElementById(0)->AddChild(element.get());
  scene.AddUiElement(std::move(element));

  gfx::Vector3dF look_at{0.f, 0.f, -1.f};
  scene.OnBeginFrame(MicrosecondsToTicks(0), look_at);
  EXPECT_TRUE(Point3FAreNearlyEqual(gfx::Point3F(0.f, 0.f, -1.f),
                                    scene.GetUiElementById(1)->GetCenter()));

  // This should trigger reposition of viewport aware elements.
  RotateAboutYAxis(90.f, &look_at);
  scene.OnBeginFrame(MicrosecondsToTicks(10), look_at);
  EXPECT_TRUE(Point3FAreNearlyEqual(gfx::Point3F(-1.f, 0.f, 0.f),
                                    scene.GetUiElementById(1)->GetCenter()));
}

}  // namespace vr
