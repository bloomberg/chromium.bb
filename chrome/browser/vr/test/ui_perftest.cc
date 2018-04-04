// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "cc/base/lap_timer.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/perf_test_utils.h"
#include "chrome/browser/vr/test/ui_test.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "ui/gfx/geometry/quaternion.h"

namespace vr {

namespace {

constexpr size_t kNumCycles = 500;
constexpr size_t kNumSteps = 20;
constexpr size_t kTimeLimitMs = 3000;
constexpr size_t kNumWarmupLaps = 20;
constexpr char kSuiteName[] = "UiPerfTest";

}  // namespace

class UiPerfTest : public UiTest {
 public:
  UiPerfTest()
      : timer_(kNumWarmupLaps,
               base::TimeDelta::FromMilliseconds(kTimeLimitMs),
               kNumSteps) {}
  ~UiPerfTest() override {}

 protected:
  cc::LapTimer timer_;
};

TEST_F(UiPerfTest, MoveController) {
  CreateScene(kNotInCct, kNotInWebVr);
  std::vector<UiElementName> names = {kContentQuad, kUrlBarBackButton,
                                      kUrlBarUrlText};
  gfx::Vector3dF direction = kForwardVector;
  timer_.Reset();
  for (size_t i = 0; i < kNumCycles; ++i) {
    for (auto name : names) {
      // Start interpolation toward next target.
      auto* element = scene_->GetUiElementByName(name);
      gfx::Vector3dF target = element->GetCenter() - gfx::Point3F();
      gfx::Quaternion q(direction, target);

      for (size_t k = 0; k < kNumSteps; ++k) {
        ControllerModel controller_model;
        controller_model.laser_direction = direction;
        ReticleModel reticle_model;
        GestureList gesture_list;

        double t = k / (static_cast<double>(kNumSteps) - 1);
        gfx::Transform(gfx::Quaternion().Slerp(q, t))
            .TransformVector(&controller_model.laser_direction);
        ui_->input_manager()->HandleInput(current_time_, RenderInfo(),
                                          controller_model, &reticle_model,
                                          &gesture_list);

        ui_->OnControllerUpdated(controller_model, reticle_model);
        OnDelayedFrame(base::TimeDelta::FromMilliseconds(16));
        scene_->UpdateTextures();
        timer_.NextLap();
      }

      direction = target;
    }
  }
  PrintResults(kSuiteName, "move_controller", &timer_);
}

}  // namespace vr
