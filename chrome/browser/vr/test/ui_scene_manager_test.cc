// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_scene_manager_test.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace vr {

UiSceneManagerTest::UiSceneManagerTest() {}
UiSceneManagerTest::~UiSceneManagerTest() {}

void UiSceneManagerTest::SetUp() {
  browser_ = base::MakeUnique<MockBrowserInterface>();
}

void UiSceneManagerTest::MakeManager(InCct in_cct, InWebVr in_web_vr) {
  scene_ = base::MakeUnique<UiScene>();
  manager_ = base::MakeUnique<UiSceneManager>(browser_.get(), scene_.get(),
                                              &content_input_delegate_, in_cct,
                                              in_web_vr, kNotAutopresented);
}

void UiSceneManagerTest::MakeAutoPresentedManager() {
  scene_ = base::MakeUnique<UiScene>();
  manager_ = base::MakeUnique<UiSceneManager>(
      browser_.get(), scene_.get(), &content_input_delegate_, kNotInCct,
      kNotInWebVr, kAutopresented);
}

bool UiSceneManagerTest::IsVisible(UiElementName name) const {
  scene_->root_element().UpdateInheritedProperties();
  UiElement* element = scene_->GetUiElementByName(name);
  return element ? element->IsVisible() : false;
}

void UiSceneManagerTest::VerifyElementsVisible(
    const std::string& trace_context,
    const std::set<UiElementName>& names) const {
  scene_->root_element().UpdateInheritedProperties();
  SCOPED_TRACE(trace_context);
  for (auto name : names) {
    SCOPED_TRACE(name);
    auto* element = scene_->GetUiElementByName(name);
    EXPECT_NE(nullptr, element);
    EXPECT_TRUE(element->IsVisible());
  }
}

bool UiSceneManagerTest::VerifyVisibility(const std::set<UiElementName>& names,
                                          bool visible) const {
  scene_->root_element().UpdateInheritedProperties();
  for (auto name : names) {
    SCOPED_TRACE(name);
    auto* element = scene_->GetUiElementByName(name);
    if (!element && visible) {
      return false;
    }
    if (element && element->IsVisible() != visible) {
      return false;
    }
  }
  return true;
}

void UiSceneManagerTest::AnimateBy(base::TimeDelta delta) {
  base::TimeTicks target_time = current_time_ + delta;
  base::TimeDelta frame_time = base::TimeDelta::FromSecondsD(1.0 / 60.0);
  for (; current_time_ < target_time; current_time_ += frame_time) {
    scene_->OnBeginFrame(current_time_, gfx::Vector3dF());
  }
  current_time_ = target_time;
  scene_->OnBeginFrame(current_time_, gfx::Vector3dF());
}

bool UiSceneManagerTest::IsAnimating(
    UiElement* element,
    const std::vector<TargetProperty>& properties) const {
  for (auto property : properties) {
    if (!element->animation_player().IsAnimatingProperty(property))
      return false;
  }
  return true;
}

SkColor UiSceneManagerTest::GetBackgroundColor() const {
  Rect* front =
      static_cast<Rect*>(scene_->GetUiElementByName(kBackgroundFront));
  EXPECT_NE(nullptr, front);
  if (!front)
    return SK_ColorBLACK;

  SkColor color = front->edge_color();

  // While returning background color, ensure that all background panel elements
  // share the same color.
  for (auto name : {kBackgroundFront, kBackgroundLeft, kBackgroundBack,
                    kBackgroundRight, kBackgroundTop, kBackgroundBottom}) {
    const Rect* panel = static_cast<Rect*>(scene_->GetUiElementByName(name));
    EXPECT_NE(nullptr, panel);
    if (!panel)
      return SK_ColorBLACK;
    EXPECT_EQ(panel->center_color(), color);
    EXPECT_EQ(panel->edge_color(), color);
  }

  return color;
}

}  // namespace vr
