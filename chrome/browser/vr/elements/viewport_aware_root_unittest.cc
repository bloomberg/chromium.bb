// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/viewport_aware_root.h"

#include <cmath>

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

const float kThreshold = ViewportAwareRoot::kViewportRotationTriggerDegrees;
const int kFramesPerSecond = 60;

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
  UiScene scene;

  auto element = base::MakeUnique<ViewportAwareRoot>();
  ViewportAwareRoot& root = *element;
  scene.AddUiElement(kRoot, std::move(element));
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

class ViewportAwareRootTest : public testing::Test {
 public:
  ViewportAwareRootTest() {}
  ~ViewportAwareRootTest() override {}

  void SetUp() override {
    scene_ = base::MakeUnique<UiScene>();
    auto viewport_aware_root = base::MakeUnique<ViewportAwareRoot>();
    viewport_aware_root->set_draw_phase(kPhaseForeground);
    viewport_root = viewport_aware_root.get();
    scene_->AddUiElement(kRoot, std::move(viewport_aware_root));

    auto element = base::MakeUnique<UiElement>();
    element->set_viewport_aware(true);
    element->set_draw_phase(kPhaseForeground);
    element->SetTranslate(0.f, 0.f, -1.f);
    viewport_element = element.get();
    viewport_root->AddChild(std::move(element));
  }

 protected:
  void AnimateWithHeadPose(base::TimeDelta delta,
                           const gfx::Vector3dF& head_pose) {
    base::TimeTicks target_time = current_time_ + delta;
    base::TimeDelta frame_duration =
        base::TimeDelta::FromSecondsD(1.0 / kFramesPerSecond);
    for (; current_time_ < target_time; current_time_ += frame_duration) {
      scene_->OnBeginFrame(current_time_, head_pose);
    }
    current_time_ = target_time;
    scene_->OnBeginFrame(current_time_, head_pose);
  }

  UiElement* viewport_root;
  UiElement* viewport_element;

 private:
  std::unique_ptr<UiScene> scene_;
  base::TimeTicks current_time_;
};

TEST_F(ViewportAwareRootTest, ChildElementsRepositioned) {
  gfx::Vector3dF look_at{0.f, 0.f, -1.f};
  AnimateWithHeadPose(MsToDelta(0), look_at);
  EXPECT_TRUE(Point3FAreNearlyEqual(gfx::Point3F(0.f, 0.f, -1.f),
                                    viewport_element->GetCenter()));

  // This should trigger reposition of viewport aware elements.
  RotateAboutYAxis(90.f, &look_at);
  AnimateWithHeadPose(MsToDelta(10), look_at);
  EXPECT_TRUE(Point3FAreNearlyEqual(gfx::Point3F(-1.f, 0.f, 0.f),
                                    viewport_element->GetCenter()));
}

TEST_F(ViewportAwareRootTest, ChildElementsHasOpacityAnimation) {
  gfx::Vector3dF look_at{0.f, 0.f, -1.f};
  AnimateWithHeadPose(MsToDelta(0), look_at);
  EXPECT_TRUE(viewport_element->IsVisible());

  // Trigger a reposition.
  RotateAboutYAxis(90.f, &look_at);
  AnimateWithHeadPose(MsToDelta(5), look_at);

  // Initially the element should be invisible and then animate to its full
  // opacity.
  EXPECT_FALSE(viewport_element->IsVisible());
  AnimateWithHeadPose(MsToDelta(50), look_at);
  EXPECT_TRUE(viewport_element->IsVisible());
  EXPECT_TRUE(IsAnimating(viewport_root, {OPACITY}));
  AnimateWithHeadPose(MsToDelta(500), look_at);
  EXPECT_TRUE(viewport_element->IsVisible());
  EXPECT_FALSE(IsAnimating(viewport_root, {OPACITY}));
}

}  // namespace vr
