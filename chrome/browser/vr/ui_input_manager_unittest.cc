// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/mock_content_input_delegate.h"
#include "chrome/browser/vr/test/ui_scene_manager_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

namespace vr {

class UiInputManagerTest : public UiSceneManagerTest {
 public:
  void SetUp() override {
    UiSceneManagerTest::SetUp();
    MakeManager(kNotInCct, kNotInWebVr);
    input_manager_ = base::MakeUnique<UiInputManager>(scene_.get());
    scene_->OnBeginFrame(MicrosecondsToTicks(1), kForwardVector);
  }

 protected:
  std::unique_ptr<UiInputManager> input_manager_;
};

TEST_F(UiInputManagerTest, NoMouseMovesDuringClick) {
  // It would be nice if the controller weren't platform specific and we could
  // mock out the underlying sensor data. For now, we will hallucinate
  // parameters to HandleInput.
  UiElement* content_quad =
      scene_->GetUiElementByName(UiElementName::kContentQuad);
  gfx::Point3F content_quad_center;
  content_quad->world_space_transform().TransformPoint(&content_quad_center);
  gfx::Point3F origin;
  GestureList gesture_list;
  gfx::Point3F out_target_point;
  UiElement* out_reticle_render_target;
  input_manager_->HandleInput(content_quad_center - origin, origin,
                              UiInputManager::ButtonState::DOWN, &gesture_list,
                              &out_target_point, &out_reticle_render_target);

  // We should have hit the content quad if our math was correct.
  ASSERT_NE(nullptr, out_reticle_render_target);
  EXPECT_EQ(UiElementName::kContentQuad, out_reticle_render_target->name());

  // Unless we suppress content move events during clicks, this will cause us to
  // call OnContentMove on the delegate. We should do this suppression, so we
  // set the expected number of calls to zero.
  EXPECT_CALL(*content_input_delegate_, OnContentMove(testing::_)).Times(0);

  input_manager_->HandleInput(content_quad_center - origin, origin,
                              UiInputManager::ButtonState::DOWN, &gesture_list,
                              &out_target_point, &out_reticle_render_target);
}

}  // namespace vr
