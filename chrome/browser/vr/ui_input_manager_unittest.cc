// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_input_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/mock_content_input_delegate.h"
#include "chrome/browser/vr/test/ui_scene_manager_test.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::StrictMock;

namespace vr {

constexpr UiInputManager::ButtonState kUp = UiInputManager::ButtonState::UP;
constexpr UiInputManager::ButtonState kDown = UiInputManager::ButtonState::DOWN;
constexpr UiInputManager::ButtonState kClick =
    UiInputManager::ButtonState::CLICKED;

class MockRect : public Rect {
 public:
  MockRect() = default;
  ~MockRect() override = default;

  MOCK_METHOD1(OnHoverEnter, void(const gfx::PointF& position));
  MOCK_METHOD0(OnHoverLeave, void());
  MOCK_METHOD1(OnMove, void(const gfx::PointF& position));
  MOCK_METHOD1(OnButtonDown, void(const gfx::PointF& position));
  MOCK_METHOD1(OnButtonUp, void(const gfx::PointF& position));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRect);
};

class UiInputManagerTest : public testing::Test {
 public:
  void SetUp() override {
    scene_ = base::MakeUnique<UiScene>();
    input_manager_ = base::MakeUnique<UiInputManager>(scene_.get());
  }

  StrictMock<MockRect>* CreateMockElement(float z_position) {
    auto element = base::MakeUnique<StrictMock<MockRect>>();
    StrictMock<MockRect>* p_element = element.get();
    element->SetTranslate(0, 0, z_position);
    element->SetVisible(true);
    scene_->AddUiElement(kRoot, std::move(element));
    scene_->OnBeginFrame(base::TimeTicks(), kForwardVector);
    return p_element;
  }

  void HandleInput(const gfx::Vector3dF& laser_direction,
                   UiInputManager::ButtonState button_state) {
    ControllerModel controller_model;
    controller_model.laser_direction = laser_direction;
    controller_model.laser_origin = {0, 0, 0};
    controller_model.touchpad_button_state = button_state;
    ReticleModel reticle_model;
    GestureList gesture_list;
    input_manager_->HandleInput(MsToTicks(1), controller_model, &reticle_model,
                                &gesture_list);
  }

 protected:
  std::unique_ptr<UiScene> scene_;
  std::unique_ptr<UiInputManager> input_manager_;
  InSequence inSequence;
};

class UiInputManagerContentTest : public UiSceneManagerTest {
 public:
  void SetUp() override {
    UiSceneManagerTest::SetUp();
    MakeManager(kNotInCct, kNotInWebVr);
    input_manager_ = base::MakeUnique<UiInputManager>(scene_.get());
    EXPECT_TRUE(scene_->OnBeginFrame(MicrosecondsToTicks(1), kForwardVector));
  }

 protected:
  std::unique_ptr<UiInputManager> input_manager_;
};

TEST_F(UiInputManagerTest, ReticleRenderTarget) {
  auto element = base::MakeUnique<Rect>();
  UiElement* p_element = element.get();
  element->SetTranslate(0, 0, -1.f);
  element->SetVisible(true);
  scene_->AddUiElement(kRoot, std::move(element));
  scene_->OnBeginFrame(base::TimeTicks(), kForwardVector);

  ControllerModel controller_model;
  controller_model.laser_direction = kBackwardVector;
  controller_model.laser_origin = {0, 0, 0};
  controller_model.touchpad_button_state = kUp;
  ReticleModel reticle_model;
  GestureList gesture_list;

  input_manager_->HandleInput(MsToTicks(1), controller_model, &reticle_model,
                              &gesture_list);
  EXPECT_EQ(0, reticle_model.target_element_id);

  controller_model.laser_direction = kForwardVector;
  input_manager_->HandleInput(MsToTicks(1), controller_model, &reticle_model,
                              &gesture_list);
  EXPECT_EQ(p_element->id(), reticle_model.target_element_id);
  EXPECT_NEAR(-1.0, reticle_model.target_point.z(), kEpsilon);
}

// Test hover and click by toggling button state, and directing the controller
// either directly at (forward) or directly away from (backward) a test element.
// Verify mock expectations along the way to make failures easier to track.
TEST_F(UiInputManagerTest, HoverClick) {
  StrictMock<MockRect>* p_element = CreateMockElement(-5.f);

  // Move over the test element.
  EXPECT_CALL(*p_element, OnHoverEnter(_));
  HandleInput(kForwardVector, kUp);
  EXPECT_CALL(*p_element, OnMove(_));
  HandleInput(kForwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Press the button while on the element.
  EXPECT_CALL(*p_element, OnMove(_));
  EXPECT_CALL(*p_element, OnButtonDown(_));
  HandleInput(kForwardVector, kDown);
  Mock::VerifyAndClearExpectations(p_element);

  // Release the button while on the element.
  EXPECT_CALL(*p_element, OnMove(_));
  EXPECT_CALL(*p_element, OnButtonUp(_));
  HandleInput(kForwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Perform a click (both press and release) on the element.
  EXPECT_CALL(*p_element, OnMove(_));
  EXPECT_CALL(*p_element, OnButtonDown(_));
  EXPECT_CALL(*p_element, OnButtonUp(_));
  HandleInput(kForwardVector, kClick);
  Mock::VerifyAndClearExpectations(p_element);

  // Move off of the element.
  EXPECT_CALL(*p_element, OnHoverLeave());
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Press while not on the element, move over the element, move away, then
  // release. The element should receive no input.
  HandleInput(kBackwardVector, kDown);
  HandleInput(kForwardVector, kDown);
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Press on an element, move away, then release.
  EXPECT_CALL(*p_element, OnHoverEnter(_));
  EXPECT_CALL(*p_element, OnButtonDown(_));
  HandleInput(kForwardVector, kDown);
  EXPECT_CALL(*p_element, OnMove(_));
  HandleInput(kBackwardVector, kDown);
  Mock::VerifyAndClearExpectations(p_element);
  EXPECT_CALL(*p_element, OnMove(_));
  EXPECT_CALL(*p_element, OnButtonUp(_));
  EXPECT_CALL(*p_element, OnHoverLeave());
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);
}

// Test pressing the button while on an element, moving to another element, and
// releasing the button. Upon release, the previous element should see its click
// and hover states cleared, and the new element should see a hover.
TEST_F(UiInputManagerTest, ReleaseButtonOnAnotherElement) {
  StrictMock<MockRect>* p_front_element = CreateMockElement(-5.f);
  StrictMock<MockRect>* p_back_element = CreateMockElement(5.f);

  // Press on an element, move away, then release.
  EXPECT_CALL(*p_front_element, OnHoverEnter(_));
  EXPECT_CALL(*p_front_element, OnButtonDown(_));
  EXPECT_CALL(*p_front_element, OnMove(_));
  EXPECT_CALL(*p_front_element, OnMove(_));
  EXPECT_CALL(*p_front_element, OnButtonUp(_));
  EXPECT_CALL(*p_front_element, OnHoverLeave());
  EXPECT_CALL(*p_back_element, OnHoverEnter(_));
  HandleInput(kForwardVector, kDown);
  HandleInput(kBackwardVector, kDown);
  HandleInput(kBackwardVector, kUp);
}

// Test that input is tolerant of disappearing elements.
TEST_F(UiInputManagerTest, ElementDeletion) {
  StrictMock<MockRect>* p_element = CreateMockElement(-5.f);

  // Hover on an element.
  EXPECT_CALL(*p_element, OnHoverEnter(_));
  HandleInput(kForwardVector, kUp);

  // Remove and retain the element from the scene, and ensure that it receives
  // no extraneous input.
  auto deleted_element = scene_->RemoveUiElement(p_element->id());
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);

  // Re-add the element to the scene, and press on it to lock it for input.
  scene_->AddUiElement(kRoot, std::move(deleted_element));
  scene_->OnBeginFrame(base::TimeTicks(), kForwardVector);
  EXPECT_CALL(*p_element, OnHoverEnter(_));
  EXPECT_CALL(*p_element, OnButtonDown(_));
  HandleInput(kForwardVector, kDown);

  // Remove the element again, move off the element, and release to ensure that
  // input isn't delivered to an input-locked element that's been deleted from
  // the scene.
  scene_->RemoveUiElement(p_element->id());
  HandleInput(kBackwardVector, kDown);
  HandleInput(kBackwardVector, kUp);
  Mock::VerifyAndClearExpectations(p_element);
}

TEST_F(UiInputManagerContentTest, NoMouseMovesDuringClick) {
  EXPECT_TRUE(AnimateBy(MsToDelta(500)));
  // It would be nice if the controller weren't platform specific and we could
  // mock out the underlying sensor data. For now, we will hallucinate
  // parameters to HandleInput.
  UiElement* content_quad =
      scene_->GetUiElementByName(UiElementName::kContentQuad);
  gfx::Point3F content_quad_center;
  content_quad->world_space_transform().TransformPoint(&content_quad_center);
  gfx::Point3F origin;

  ControllerModel controller_model;
  controller_model.laser_direction = content_quad_center - origin;
  controller_model.laser_origin = origin;
  controller_model.touchpad_button_state = UiInputManager::ButtonState::DOWN;
  ReticleModel reticle_model;
  GestureList gesture_list;
  input_manager_->HandleInput(MsToTicks(1), controller_model, &reticle_model,
                              &gesture_list);

  // We should have hit the content quad if our math was correct.
  ASSERT_NE(0, reticle_model.target_element_id);
  EXPECT_EQ(content_quad->id(), reticle_model.target_element_id);

  // Unless we suppress content move events during clicks, this will cause us to
  // call OnContentMove on the delegate. We should do this suppression, so we
  // set the expected number of calls to zero.
  EXPECT_CALL(*content_input_delegate_, OnContentMove(testing::_)).Times(0);

  input_manager_->HandleInput(MsToTicks(1), controller_model, &reticle_model,
                              &gesture_list);
}

TEST_F(UiInputManagerContentTest, TreeVsZOrder) {
  EXPECT_TRUE(AnimateBy(MsToDelta(500)));
  // It would be nice if the controller weren't platform specific and we could
  // mock out the underlying sensor data. For now, we will hallucinate
  // parameters to HandleInput.
  UiElement* content_quad =
      scene_->GetUiElementByName(UiElementName::kContentQuad);
  gfx::Point3F content_quad_center;
  content_quad->world_space_transform().TransformPoint(&content_quad_center);
  gfx::Point3F origin;

  ControllerModel controller_model;
  controller_model.laser_direction = content_quad_center - origin;
  controller_model.laser_origin = origin;
  controller_model.touchpad_button_state = UiInputManager::ButtonState::DOWN;
  ReticleModel reticle_model;
  GestureList gesture_list;
  input_manager_->HandleInput(MsToTicks(1), controller_model, &reticle_model,
                              &gesture_list);

  // We should have hit the content quad if our math was correct.
  ASSERT_NE(0, reticle_model.target_element_id);
  EXPECT_EQ(content_quad->id(), reticle_model.target_element_id);

  // We will now move the content quad behind the backplane.
  content_quad->SetTranslate(0, 0, -2.0 * kTextureOffset);
  EXPECT_TRUE(AnimateBy(MsToDelta(500)));
  input_manager_->HandleInput(MsToTicks(1), controller_model, &reticle_model,
                              &gesture_list);

  // We should have hit the content quad even though, geometrically, it stacks
  // behind the backplane.
  ASSERT_NE(0, reticle_model.target_element_id);
  EXPECT_EQ(content_quad->id(), reticle_model.target_element_id);
}

}  // namespace vr
