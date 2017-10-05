// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_scene_manager_test.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/test/fake_ui_element_renderer.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

namespace {

bool IsElementFacingCamera(const UiElement* element) {
  // Element might become invisible due to incorrect rotation, i.e when rotation
  // cause the visible side of the element flip.
  // Here we calculate the dot product of (origin - center) and normal. If the
  // result is greater than 0, it means the visible side of this element is
  // facing camera.
  gfx::Point3F center = element->GetCenter();
  return gfx::DotProduct(-gfx::Vector3dF(center.x(), center.y(), center.z()),
                         element->GetNormal()) > 0.f;
}

}  // namespace

UiSceneManagerTest::UiSceneManagerTest() {}
UiSceneManagerTest::~UiSceneManagerTest() {}

void UiSceneManagerTest::SetUp() {
  browser_ = base::MakeUnique<testing::NiceMock<MockBrowserInterface>>();
  content_input_delegate_ =
      base::MakeUnique<testing::NiceMock<MockContentInputDelegate>>();
}

void UiSceneManagerTest::MakeManager(InCct in_cct, InWebVr in_web_vr) {
  scene_ = base::MakeUnique<UiScene>();
  model_ = base::MakeUnique<Model>();

  UiInitialState ui_initial_state;
  ui_initial_state.in_cct = in_cct;
  ui_initial_state.in_web_vr = in_web_vr;
  ui_initial_state.web_vr_autopresentation_expected = false;
  manager_ = base::MakeUnique<UiSceneManager>(browser_.get(), scene_.get(),
                                              content_input_delegate_.get(),
                                              model_.get(), ui_initial_state);
}

void UiSceneManagerTest::MakeAutoPresentedManager() {
  scene_ = base::MakeUnique<UiScene>();
  model_ = base::MakeUnique<Model>();

  UiInitialState ui_initial_state;
  ui_initial_state.in_cct = false;
  ui_initial_state.in_web_vr = false;
  ui_initial_state.web_vr_autopresentation_expected = true;
  manager_ = base::MakeUnique<UiSceneManager>(browser_.get(), scene_.get(),
                                              content_input_delegate_.get(),
                                              model_.get(), ui_initial_state);
}

bool UiSceneManagerTest::IsVisible(UiElementName name) const {
  scene_->root_element().UpdateComputedOpacityRecursive();
  scene_->root_element().UpdateWorldSpaceTransformRecursive();
  UiElement* element = scene_->GetUiElementByName(name);
  if (!element)
    return false;

  return element->IsVisible() && IsElementFacingCamera(element);
}

void UiSceneManagerTest::VerifyElementsVisible(
    const std::string& trace_context,
    const std::set<UiElementName>& names) const {
  scene_->root_element().UpdateComputedOpacityRecursive();
  scene_->root_element().UpdateWorldSpaceTransformRecursive();
  SCOPED_TRACE(trace_context);
  for (auto name : names) {
    SCOPED_TRACE(name);
    auto* element = scene_->GetUiElementByName(name);
    ASSERT_NE(nullptr, element);
    EXPECT_TRUE(element->IsVisible());
    EXPECT_TRUE(IsElementFacingCamera(element));
    EXPECT_NE(kPhaseNone, element->draw_phase());
  }
}

bool UiSceneManagerTest::VerifyVisibility(const std::set<UiElementName>& names,
                                          bool visible) const {
  scene_->root_element().UpdateComputedOpacityRecursive();
  scene_->root_element().UpdateWorldSpaceTransformRecursive();
  for (auto name : names) {
    SCOPED_TRACE(name);
    auto* element = scene_->GetUiElementByName(name);
    EXPECT_NE(nullptr, element);
    if (!element || element->IsVisible() != visible) {
      return false;
    }
    if (!element || (visible && element->draw_phase() == kPhaseNone)) {
      return false;
    }
  }
  return true;
}

bool UiSceneManagerTest::VerifyRequiresLayout(
    const std::set<UiElementName>& names,
    bool requires_layout) const {
  scene_->root_element().UpdateComputedOpacityRecursive();
  scene_->root_element().UpdateWorldSpaceTransformRecursive();
  for (auto name : names) {
    SCOPED_TRACE(name);
    auto* element = scene_->GetUiElementByName(name);
    EXPECT_NE(nullptr, element);
    if (!element || element->requires_layout() != requires_layout) {
      return false;
    }
  }
  return true;
}

void UiSceneManagerTest::CheckRendererOpacityRecursive(
    UiElement* element) {
  // Disable all opacity animation for testing.
  element->SetTransitionedProperties({});
  // Set element's opacity to a value smaller than 1. This could make sure it's
  // children's opacity is not the same as computed_opacity. Otherwise, our test
  // might be confused which opacity is used by renderer.
  element->SetOpacity(0.9f);

  OnBeginFrame();

  FakeUiElementRenderer renderer;
  if (element->draw_phase() != kPhaseNone) {
    element->Render(&renderer, kProjMatrix);
  }

  // It is expected that some elements doesn't render anything (such as root
  // elements). So skipping verify these elements should be fine.
  if (renderer.called()) {
    EXPECT_FLOAT_EQ(renderer.opacity(), element->computed_opacity())
        << "element name: " << element->name();
  }

  for (auto& child : element->children()) {
    CheckRendererOpacityRecursive(child.get());
  }
}

void UiSceneManagerTest::AnimateBy(base::TimeDelta delta) {
  base::TimeTicks target_time = current_time_ + delta;
  base::TimeDelta frame_time = base::TimeDelta::FromSecondsD(1.0 / 60.0);
  for (; current_time_ < target_time; current_time_ += frame_time) {
    scene_->OnBeginFrame(current_time_, gfx::Vector3dF(0.f, 0.f, -1.0f));
  }
  current_time_ = target_time;
  scene_->OnBeginFrame(current_time_, gfx::Vector3dF(0.f, 0.f, -1.0f));
}

void UiSceneManagerTest::OnBeginFrame() {
  scene_->OnBeginFrame(base::TimeTicks(), gfx::Vector3dF(0.f, 0.f, -1.0f));
}

void UiSceneManagerTest::GetBackgroundColor(SkColor* background_color) const {
  Rect* front =
      static_cast<Rect*>(scene_->GetUiElementByName(kBackgroundFront));
  ASSERT_NE(nullptr, front);
  SkColor color = front->edge_color();

  // While returning background color, ensure that all background panel elements
  // share the same color.
  for (auto name : {kBackgroundFront, kBackgroundLeft, kBackgroundBack,
                    kBackgroundRight, kBackgroundTop, kBackgroundBottom}) {
    const Rect* panel = static_cast<Rect*>(scene_->GetUiElementByName(name));
    ASSERT_NE(nullptr, panel);
    EXPECT_EQ(panel->center_color(), color);
    EXPECT_EQ(panel->edge_color(), color);
  }

  *background_color = color;
}

}  // namespace vr
