// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_scene_manager_test.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/test/animation_utils.h"
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
  gfx::Point3F origin;
  if (center == origin) {
    // If the center of element is at origin, as our camera facing negative z.
    // we only need to make sure the normal of the element have positive z.
    return element->GetNormal().z() > 0.f;
  }
  return gfx::DotProduct(origin - center, element->GetNormal()) > 0.f;
}

}  // namespace

UiSceneManagerTest::UiSceneManagerTest() {}
UiSceneManagerTest::~UiSceneManagerTest() {}

void UiSceneManagerTest::SetUp() {
  browser_ = base::MakeUnique<testing::NiceMock<MockBrowserInterface>>();
}

void UiSceneManagerTest::MakeManager(InCct in_cct, InWebVr in_web_vr) {
  MakeManagerInternal(in_cct, in_web_vr, kNotAutopresented);
}

void UiSceneManagerTest::MakeAutoPresentedManager() {
  MakeManagerInternal(kNotInCct, kNotInWebVr, kAutopresented);
}

bool UiSceneManagerTest::IsVisible(UiElementName name) const {
  OnBeginFrame();
  UiElement* element = scene_->GetUiElementByName(name);
  if (!element || !element->IsVisible())
    return false;

  if (!element->IsWorldPositioned())
    return true;

  return IsElementFacingCamera(element);
}

void UiSceneManagerTest::SetIncognito(bool incognito) {
  model_->incognito = incognito;
}

void UiSceneManagerTest::VerifyElementsVisible(
    const std::string& trace_context,
    const std::set<UiElementName>& names) const {
  OnBeginFrame();
  SCOPED_TRACE(trace_context);
  for (auto name : names) {
    SCOPED_TRACE(UiElementNameToString(name));
    auto* element = scene_->GetUiElementByName(name);
    ASSERT_NE(nullptr, element);
    EXPECT_TRUE(element->IsVisible());
    EXPECT_TRUE(!element->IsWorldPositioned() ||
                IsElementFacingCamera(element));
    EXPECT_NE(kPhaseNone, element->draw_phase());
  }
}

bool UiSceneManagerTest::VerifyVisibility(const std::set<UiElementName>& names,
                                          bool visible) const {
  OnBeginFrame();
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

bool UiSceneManagerTest::VerifyIsAnimating(
    const std::set<UiElementName>& names,
    const std::vector<TargetProperty>& properties,
    bool animating) const {
  OnBeginFrame();
  for (auto name : names) {
    auto* element = scene_->GetUiElementByName(name);
    EXPECT_NE(nullptr, element);
    SCOPED_TRACE(element->DebugName());
    if (IsAnimating(element, properties) != animating) {
      return false;
    }
  }
  return true;
}

int UiSceneManagerTest::NumVisibleChildren(UiElementName name) const {
  auto* root = scene_->GetUiElementByName(name);
  EXPECT_NE(root, nullptr);
  if (!root) {
    return 0;
  }
  int visible = 0;
  for (const auto& element : *root) {
    if (element.IsVisible()) {
      visible++;
    }
  }
  return visible;
}

bool UiSceneManagerTest::VerifyRequiresLayout(
    const std::set<UiElementName>& names,
    bool requires_layout) const {
  OnBeginFrame();
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
    CameraModel model;
    model.view_proj_matrix = kPixelDaydreamProjMatrix;
    element->Render(&renderer, model);
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

bool UiSceneManagerTest::AnimateBy(base::TimeDelta delta) {
  base::TimeTicks target_time = current_time_ + delta;
  base::TimeDelta frame_time = base::TimeDelta::FromSecondsD(1.0 / 60.0);
  bool changed = false;
  for (; current_time_ < target_time; current_time_ += frame_time) {
    if (scene_->OnBeginFrame(current_time_, kForwardVector))
      changed = true;
  }
  current_time_ = target_time;
  if (scene_->OnBeginFrame(current_time_, kForwardVector))
    changed = true;
  return changed;
}

bool UiSceneManagerTest::OnBeginFrame() const {
  return scene_->OnBeginFrame(current_time_, kForwardVector);
}

void UiSceneManagerTest::GetBackgroundColor(SkColor* background_color) const {
  OnBeginFrame();
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

void UiSceneManagerTest::MakeManagerInternal(
    InCct in_cct,
    InWebVr in_web_vr,
    WebVrAutopresented web_vr_autopresented) {
  auto content_input_delegate =
      base::MakeUnique<testing::NiceMock<MockContentInputDelegate>>();
  content_input_delegate_ = content_input_delegate.get();

  UiInitialState ui_initial_state;
  ui_initial_state.in_cct = in_cct;
  ui_initial_state.in_web_vr = in_web_vr;
  ui_initial_state.web_vr_autopresentation_expected = web_vr_autopresented;
  ui_ =
      base::MakeUnique<Ui>(std::move(browser_.get()),
                           std::move(content_input_delegate), ui_initial_state);
  scene_ = ui_->scene();
  model_ = ui_->model_for_test();
}

}  // namespace vr
