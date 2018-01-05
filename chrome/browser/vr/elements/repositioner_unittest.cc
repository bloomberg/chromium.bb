// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/repositioner.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "cc/test/geometry_test_utils.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

constexpr float kTestRepositionDistance = 2.f;

void CheckRepositionedCorrectly(size_t test_case_index,
                                UiElement* element,
                                const gfx::Point3F& expected_center) {
  SCOPED_TRACE(base::StringPrintf("Test case index = %zd", test_case_index));
  gfx::Point3F center = element->GetCenter();
  EXPECT_NEAR(center.x(), expected_center.x(), kEpsilon);
  EXPECT_NEAR(center.y(), expected_center.y(), kEpsilon);
  EXPECT_NEAR(center.z(), expected_center.z(), kEpsilon);
  gfx::Vector3dF right_vector = {1, 0, 0};
  element->world_space_transform().TransformVector(&right_vector);
  gfx::Vector3dF expected_right =
      gfx::CrossProduct(expected_center - kOrigin, {0, 1, 0});
  gfx::Vector3dF normalized_expected_right_vector;
  expected_right.GetNormalized(&normalized_expected_right_vector);
  EXPECT_VECTOR3DF_NEAR(right_vector, normalized_expected_right_vector,
                        kEpsilon);
}

}  // namespace

TEST(Repositioner, RepositionNegativeZWithReticle) {
  UiScene scene;
  auto child = base::MakeUnique<UiElement>();
  child->SetTranslate(0, 0, -kTestRepositionDistance);
  auto* element = child.get();
  auto parent = base::MakeUnique<Repositioner>(kTestRepositionDistance);
  auto* repositioner = parent.get();
  parent->AddChild(std::move(child));
  scene.AddUiElement(kRoot, std::move(parent));

  repositioner->set_laser_origin(gfx::Point3F(1, -1, 0));

  struct TestCase {
    TestCase(gfx::Vector3dF v, gfx::Point3F p)
        : laser_direction(v), expected_element_center(p) {}
    gfx::Vector3dF laser_direction;
    gfx::Point3F expected_element_center;
  };
  std::vector<TestCase> test_cases = {
      {{-2, 2.41421f, -1}, {-1, 1.41421f, -1}},
      {{-2, 2.41421f, 1}, {-1, 1.41421f, 1}},
      {{0, 2.41421f, 1}, {1, 1.41421f, 1}},
      {{0, 2.41421f, -1}, {1, 1.41421f, -1}},
      {{-2, -0.41421f, -1}, {-1, -1.41421f, -1}},
      {{-2, -0.41421f, 1}, {-1, -1.41421f, 1}},
      {{0, -0.41421f, 1}, {1, -1.41421f, 1}},
      {{0, -0.41421f, -1}, {1, -1.41421f, -1}},
  };

  for (size_t i = 0; i < test_cases.size(); i++) {
    repositioner->set_laser_direction(test_cases[i].laser_direction);
    scene.OnBeginFrame(MsToTicks(0), kStartHeadPose);
    // Before enable repositioner, child element should NOT have rotation.
    CheckRepositionedCorrectly(i, element, {0, 0, -kTestRepositionDistance});
  }

  repositioner->set_enable(true);

  for (size_t i = 0; i < test_cases.size(); i++) {
    repositioner->set_laser_direction(test_cases[i].laser_direction);
    scene.OnBeginFrame(MsToTicks(0), kStartHeadPose);
    CheckRepositionedCorrectly(i, element,
                               test_cases[i].expected_element_center);
  }
}

}  // namespace vr
