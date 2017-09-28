// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/vr/databinding/binding_base.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

template <typename P>
UiScene::Elements GetVisibleElements(UiElement* root, P predicate) {
  UiScene::Elements elements;
  for (auto& element : *root) {
    if (element.IsVisible() && predicate(&element))
      elements.push_back(&element);
  }
  return elements;
}

}  // namespace

void UiScene::AddUiElement(UiElementName parent,
                           std::unique_ptr<UiElement> element) {
  CHECK_GE(element->id(), 0);
  CHECK_EQ(GetUiElementById(element->id()), nullptr);
  CHECK_GE(element->draw_phase(), 0);
  if (gl_initialized_)
    element->Initialize();
  GetUiElementByName(parent)->AddChild(std::move(element));
}

void UiScene::RemoveUiElement(int element_id) {
  UiElement* to_remove = GetUiElementById(element_id);
  CHECK_NE(nullptr, to_remove);
  CHECK_NE(nullptr, to_remove->parent());
  to_remove->parent()->RemoveChild(to_remove);
}

void UiScene::AddAnimation(int element_id,
                           std::unique_ptr<cc::Animation> animation) {
  UiElement* element = GetUiElementById(element_id);
  element->AddAnimation(std::move(animation));
}

void UiScene::RemoveAnimation(int element_id, int animation_id) {
  UiElement* element = GetUiElementById(element_id);
  element->RemoveAnimation(animation_id);
}

void UiScene::OnBeginFrame(const base::TimeTicks& current_time,
                           const gfx::Vector3dF& look_at) {
  for (auto& element : *root_element_) {
    // Process all animations before calculating object transforms.
    element.OnBeginFrame(current_time);
  }

  for (auto& element : *root_element_) {
    for (auto& binding : element.bindings()) {
      binding->Update();
    }
  }

  for (auto& element : *root_element_) {
    element.LayOutChildren();
    element.AdjustRotationForHeadPose(look_at);
  }

  root_element_->UpdateInheritedProperties();
}

void UiScene::PrepareToDraw() {
  for (auto& element : *root_element_)
    element.PrepareToDraw();
}

UiElement& UiScene::root_element() {
  return *root_element_;
}

UiElement* UiScene::GetUiElementById(int element_id) const {
  for (auto& element : *root_element_) {
    if (element.id() == element_id)
      return &element;
  }
  return nullptr;
}

UiElement* UiScene::GetUiElementByName(UiElementName name) const {
  for (auto& element : *root_element_) {
    if (element.name() == name)
      return &element;
  }
  return nullptr;
}

UiScene::Elements UiScene::GetVisible2dBrowsingElements() const {
  return GetVisibleElements(
      GetUiElementByName(k2dBrowsingRoot), [](UiElement* element) {
        return element->draw_phase() == kPhaseForeground ||
               element->draw_phase() == kPhaseFloorCeiling ||
               element->draw_phase() == kPhaseBackground;
      });
}

UiScene::Elements UiScene::GetVisible2dBrowsingOverlayElements() const {
  return GetVisibleElements(
      GetUiElementByName(k2dBrowsingRoot), [](UiElement* element) {
        return element->draw_phase() == kPhaseOverlayBackground ||
               element->draw_phase() == kPhaseOverlayForeground;
      });
}

UiScene::Elements UiScene::GetVisibleSplashScreenElements() const {
  return GetVisibleElements(
      GetUiElementByName(kSplashScreenRoot), [](UiElement* element) {
        return element->draw_phase() == kPhaseOverlayBackground ||
               element->draw_phase() == kPhaseOverlayForeground;
      });
}

UiScene::Elements UiScene::GetVisibleWebVrOverlayForegroundElements() const {
  return GetVisibleElements(
      GetUiElementByName(kWebVrRoot), [](UiElement* element) {
        return element->draw_phase() == kPhaseOverlayForeground;
      });
}

UiScene::UiScene() {
  root_element_ = base::MakeUnique<UiElement>();
  root_element_->set_name(kRoot);
  root_element_->set_draw_phase(kPhaseNone);
  root_element_->SetVisible(true);
  root_element_->set_hit_testable(false);
}

UiScene::~UiScene() = default;

// TODO(vollick): we should bind to gl-initialized state. Elements will
// initialize when the binding fires, automatically.
void UiScene::OnGlInitialized() {
  gl_initialized_ = true;
  for (auto& element : *root_element_)
    element.Initialize();
}

}  // namespace vr
