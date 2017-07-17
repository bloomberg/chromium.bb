// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/ui_scene_manager_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

namespace vr {

class MockUiInputManagerDelegate : public UiInputManagerDelegate {
 public:
  MOCK_METHOD1(OnContentEnter, void(const gfx::PointF& normalized_hit_point));
  MOCK_METHOD0(OnContentLeave, void());
  MOCK_METHOD1(OnContentMove, void(const gfx::PointF& normalized_hit_point));
  MOCK_METHOD1(OnContentDown, void(const gfx::PointF& normalized_hit_point));
  MOCK_METHOD1(OnContentUp, void(const gfx::PointF& normalized_hit_point));

  // As move-only parameters aren't supported by mock methods, we will override
  // the functions explicitly and fwd the calls to the mocked functions.
  MOCK_METHOD2(FwdContentFlingBegin,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentFlingCancel,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentScrollBegin,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentScrollUpdate,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));
  MOCK_METHOD2(FwdContentScrollEnd,
               void(std::unique_ptr<blink::WebGestureEvent>& gesture,
                    const gfx::PointF& normalized_hit_point));

  void OnContentFlingBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                           const gfx::PointF& normalized_hit_point) override {
    FwdContentFlingBegin(gesture, normalized_hit_point);
  }
  void OnContentFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override {
    FwdContentFlingCancel(gesture, normalized_hit_point);
  }
  void OnContentScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& normalized_hit_point) override {
    FwdContentScrollBegin(gesture, normalized_hit_point);
  }
  void OnContentScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& normalized_hit_point) override {
    FwdContentScrollUpdate(gesture, normalized_hit_point);
  }
  void OnContentScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                          const gfx::PointF& normalized_hit_point) override {
    FwdContentScrollEnd(gesture, normalized_hit_point);
  }
};

class UiInputManagerTest : public UiSceneManagerTest {
 public:
  void SetUp() override {
    UiSceneManagerTest::SetUp();
    MakeManager(kNotInCct, kNotInWebVr);
    input_manager_ = base::MakeUnique<UiInputManager>(scene_.get(), &delegate_);
    scene_->OnBeginFrame(UsToTicks(1));
  }

 protected:
  std::unique_ptr<UiInputManager> input_manager_;
  MockUiInputManagerDelegate delegate_;
};

TEST_F(UiInputManagerTest, NoMouseMovesDuringClick) {
  // It would be nice if the controller weren't platform specific and we could
  // mock out the underlying sensor data. For now, we will hallucinate
  // parameters to HandleInput.
  UiElement* content_quad =
      scene_->GetUiElementByDebugId(UiElementDebugId::kContentQuad);
  gfx::Point3F content_quad_center;
  content_quad->screen_space_transform().TransformPoint(&content_quad_center);
  gfx::Point3F origin;
  GestureList gesture_list;
  gfx::Point3F out_target_point;
  UiElement* out_reticle_render_target;
  input_manager_->HandleInput(content_quad_center - origin, origin,
                              UiInputManager::ButtonState::DOWN, &gesture_list,
                              &out_target_point, &out_reticle_render_target);

  // We should have hit the content quad if our math was correct.
  EXPECT_EQ(UiElementDebugId::kContentQuad,
            out_reticle_render_target->debug_id());

  // Unless we suppress content move events during clicks, this will cause us to
  // call OnContentMove on the delegate. We should do this suppression, so we
  // set the expected number of calls to zero.
  EXPECT_CALL(delegate_, OnContentMove(testing::_)).Times(0);

  input_manager_->HandleInput(content_quad_center - origin, origin,
                              UiInputManager::ButtonState::DOWN, &gesture_list,
                              &out_target_point, &out_reticle_render_target);
}

}  // namespace vr
