// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "ui/gfx/transform.h"

namespace vr {

template <typename F>
void ForAllElements(UiElement* e, F f) {
  f(e);
  for (auto& child : e->children()) {
    ForAllElements(child.get(), f);
  }
}

template <typename P>
UiElement* FindElement(UiElement* e, P predicate) {
  if (predicate(e)) {
    return e;
  }
  for (const auto& child : e->children()) {
    if (UiElement* match = FindElement(child.get(), predicate)) {
      return match;
    }
  }
  return nullptr;
}

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
  ForAllElements(root_element_.get(), [current_time](UiElement* element) {
    // Process all animations before calculating object transforms.
    element->Animate(current_time);
  });

  ForAllElements(root_element_.get(), [look_at](UiElement* element) {
    element->LayOutChildren();
    element->AdjustRotationForHeadPose(look_at);
  });

  root_element_->UpdateInheritedProperties();
}

void UiScene::PrepareToDraw() {
  ForAllElements(root_element_.get(),
                 [](UiElement* element) { element->PrepareToDraw(); });
}

UiElement& UiScene::root_element() {
  return *root_element_;
}

UiElement* UiScene::GetUiElementById(int element_id) const {
  return FindElement(root_element_.get(), [element_id](UiElement* element) {
    return element->id() == element_id;
  });
}

UiElement* UiScene::GetUiElementByName(UiElementName name) const {
  DCHECK(name != UiElementName::kNone);
  return FindElement(root_element_.get(), [name](UiElement* element) {
    return element->name() == name;
  });
}

std::vector<const UiElement*> UiScene::GetWorldElements() const {
  std::vector<const UiElement*> elements;
  ForAllElements(root_element_.get(), [&elements](UiElement* element) {
    if (element->IsVisible() && !element->viewport_aware() &&
        !element->is_overlay()) {
      elements.push_back(element);
    }
  });
  return elements;
}

std::vector<const UiElement*> UiScene::GetOverlayElements() const {
  std::vector<const UiElement*> elements;
  ForAllElements(root_element_.get(), [&elements](UiElement* element) {
    if (element->IsVisible() && element->is_overlay()) {
      elements.push_back(element);
    }
  });
  return elements;
}

std::vector<const UiElement*> UiScene::GetViewportAwareElements() const {
  std::vector<const UiElement*> elements;
  ForAllElements(root_element_.get(), [&elements](UiElement* element) {
    if (!element->viewport_aware())
      return;

    // Note that we need to exclude ViewportAwareRoot element. It is not a
    // visual element. Currently all of ViewportAwareRoot's children sets
    // viewport aware to true. So we check if the element's parent is a viewport
    // aware element to detect ViewportAwareRoot.
    if (element->parent() && !element->parent()->viewport_aware())
      return;

    if (element->IsVisible()) {
      elements.push_back(element);
    }
  });
  return elements;
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
  ForAllElements(root_element_.get(),
                 [](UiElement* element) { element->Initialize(); });
}

}  // namespace vr
