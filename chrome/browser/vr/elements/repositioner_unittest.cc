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
                                const gfx::Vector3dF& head_up_vector,
                                const gfx::Point3F& expected_center,
                                const gfx::Vector3dF& expected_right_vector) {
  SCOPED_TRACE(base::StringPrintf("Test case index = %zd", test_case_index));
  gfx::Point3F center = element->GetCenter();
  EXPECT_NEAR(center.x(), expected_center.x(), kEpsilon);
  EXPECT_NEAR(center.y(), expected_center.y(), kEpsilon);
  EXPECT_NEAR(center.z(), expected_center.z(), kEpsilon);
  gfx::Vector3dF right_vector = {1, 0, 0};
  element->world_space_transform().TransformVector(&right_vector);
  EXPECT_VECTOR3DF_NEAR(right_vector, expected_right_vector, kEpsilon);
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
    TestCase(gfx::Vector3dF v,
             gfx::Vector3dF u,
             gfx::Point3F p,
             gfx::Vector3dF r)
        : laser_direction(v),
          head_up_vector(u),
          expected_element_center(p),
          expected_right_vector(r) {}
    gfx::Vector3dF laser_direction;
    gfx::Vector3dF head_up_vector;
    gfx::Point3F expected_element_center;
    gfx::Vector3dF expected_right_vector;
  };

  std::vector<TestCase> test_cases = {
      {{-2, 2.41421f, -1},
       {0, 1, 0},
       {-1, 1.41421f, -1},
       {0.707107f, 0, -0.707107f}},
      {{-2, 2.41421f, 1},
       {0, 1, 0},
       {-1, 1.41421f, 1},
       {-0.707107f, 0, -0.707107f}},
      {{0, 2.41421f, 1},
       {0, 1, 0},
       {1, 1.41421f, 1},
       {-0.707107f, 0, 0.707107f}},
      {{0, 2.41421f, -1},
       {0, 1, 0},
       {1, 1.41421f, -1},
       {0.707107f, 0, 0.707107f}},
      {{-2, -0.41421f, -1},
       {0, 1, 0},
       {-1, -1.41421f, -1},
       {0.707107f, 0, -0.707107f}},
      {{-2, -0.41421f, 1},
       {0, 1, 0},
       {-1, -1.41421f, 1},
       {-0.707107f, 0, -0.707107f}},
      {{0, -0.41421f, 1},
       {0, 1, 0},
       {1, -1.41421f, 1},
       {-0.707107f, 0, 0.707107f}},
      {{0, -0.41421f, -1},
       {0, 1, 0},
       {1, -1.41421f, -1},
       {0.707107f, 0, 0.707107f}},
      {{-1, 3, 0}, {0, 0, 1}, {0, kTestRepositionDistance, 0}, {1, 0, 0}},
      {{-1, -1, 0}, {0, 0, -1}, {0, -kTestRepositionDistance, 0}, {1, 0, 0}},
      {{-1, 1, 2}, {-1, 0, 0}, {0, 0, kTestRepositionDistance}, {0, -1, 0}},
      {{-1, 1, -2}, {-1, 0, 0}, {0, 0, -kTestRepositionDistance}, {0, 1, 0}},
      {{-1, 1, -2}, {-1, 0, 0}, {0, 0, -kTestRepositionDistance}, {0, 1, 0}},
  };

  for (size_t i = 0; i < test_cases.size(); i++) {
    TestCase test_case = test_cases[i];
    repositioner->set_laser_direction(test_case.laser_direction);
    gfx::Quaternion quat{{0, 1, 0}, test_case.head_up_vector};
    gfx::Transform head_pose(quat);
    scene.OnBeginFrame(MsToTicks(0), head_pose);
    // Before enable repositioner, child element should NOT have rotation.
    CheckRepositionedCorrectly(i, element, test_case.head_up_vector,
                               {0, 0, -kTestRepositionDistance}, {1, 0, 0});
  }

  repositioner->set_enable(true);

  for (size_t i = 0; i < test_cases.size(); i++) {
    TestCase test_case = test_cases[i];
    repositioner->set_laser_direction(test_case.laser_direction);
    gfx::Quaternion quat{test_case.head_up_vector, {0, 1, 0}};
    gfx::Transform head_pose(quat);
    scene.OnBeginFrame(MsToTicks(0), head_pose);
    CheckRepositionedCorrectly(i, element, test_case.head_up_vector,
                               test_case.expected_element_center,
                               test_case.expected_right_vector);
  }
}

}  // namespace vr
