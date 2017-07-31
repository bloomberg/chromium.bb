// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

void UiScene::AddUiElement(std::unique_ptr<UiElement> element) {
  CHECK_GE(element->id(), 0);
  CHECK_EQ(GetUiElementById(element->id()), nullptr);
  if (!element->parent()) {
    CHECK_EQ(element->x_anchoring(), XAnchoring::XNONE);
    CHECK_EQ(element->y_anchoring(), YAnchoring::YNONE);
  }
  if (gl_initialized_)
    element->Initialize();
  ui_elements_.push_back(std::move(element));
}

void UiScene::RemoveUiElement(int element_id) {
  for (auto it = ui_elements_.begin(); it != ui_elements_.end(); ++it) {
    if ((*it)->id() == element_id) {
      ui_elements_.erase(it);
      return;
    }
  }
}

void UiScene::AddAnimation(int element_id,
                           std::unique_ptr<cc::Animation> animation) {
  UiElement* element = GetUiElementById(element_id);
  element->animation_player().AddAnimation(std::move(animation));
}

void UiScene::RemoveAnimation(int element_id, int animation_id) {
  UiElement* element = GetUiElementById(element_id);
  element->animation_player().RemoveAnimation(animation_id);
}

void UiScene::OnBeginFrame(const base::TimeTicks& current_time) {
  for (const auto& element : ui_elements_) {
    // Process all animations before calculating object transforms.
    element->Animate(current_time);
    element->set_dirty(true);
  }
  for (auto& element : ui_elements_) {
    element->LayOutChildren();
  }
  for (auto& element : ui_elements_) {
    ApplyRecursiveTransforms(element.get());
  }
}

void UiScene::PrepareToDraw() {
  for (const auto& element : ui_elements_) {
    element->PrepareToDraw();
  }
}

UiElement* UiScene::GetUiElementById(int element_id) const {
  for (const auto& element : ui_elements_) {
    if (element->id() == element_id) {
      return element.get();
    }
  }
  return nullptr;
}

UiElement* UiScene::GetUiElementByDebugId(UiElementDebugId debug_id) const {
  DCHECK(debug_id != UiElementDebugId::kNone);
  for (const auto& element : ui_elements_) {
    if (element->debug_id() == debug_id) {
      return element.get();
    }
  }
  return nullptr;
}

std::vector<const UiElement*> UiScene::GetWorldElements() const {
  std::vector<const UiElement*> elements;
  for (const auto& element : ui_elements_) {
    if (element->IsVisible() && !element->lock_to_fov() &&
        !element->is_overlay()) {
      elements.push_back(element.get());
    }
  }
  return elements;
}

std::vector<const UiElement*> UiScene::GetOverlayElements() const {
  std::vector<const UiElement*> elements;
  for (const auto& element : ui_elements_) {
    if (element->IsVisible() && element->is_overlay()) {
      elements.push_back(element.get());
    }
  }
  return elements;
}

std::vector<const UiElement*> UiScene::GetHeadLockedElements() const {
  std::vector<const UiElement*> elements;
  for (const auto& element : ui_elements_) {
    if (element->IsVisible() && element->lock_to_fov()) {
      elements.push_back(element.get());
    }
  }
  return elements;
}

bool UiScene::HasVisibleHeadLockedElements() const {
  return !GetHeadLockedElements().empty();
}

const std::vector<std::unique_ptr<UiElement>>& UiScene::GetUiElements() const {
  return ui_elements_;
}

UiScene::UiScene() = default;

UiScene::~UiScene() = default;

void UiScene::ApplyRecursiveTransforms(UiElement* element) {
  if (!element->dirty())
    return;

  UiElement* parent = element->parent();

  gfx::Transform transform;
  transform.Scale(element->size().width(), element->size().height());
  element->set_computed_opacity(element->opacity());
  element->set_computed_lock_to_fov(element->lock_to_fov());

  // Compute an inheritable transformation that can be applied to this element,
  // and it's children, if applicable.
  gfx::Transform inheritable = element->LocalTransform();

  if (parent) {
    ApplyRecursiveTransforms(parent);
    inheritable.ConcatTransform(parent->inheritable_transform());

    element->set_computed_opacity(element->computed_opacity() *
                                  parent->opacity());
    element->set_computed_lock_to_fov(parent->lock_to_fov());
  }

  transform.ConcatTransform(inheritable);
  element->set_world_space_transform(transform);
  element->set_inheritable_transform(inheritable);
  element->set_dirty(false);
}

// TODO(mthiesse): Move this to UiSceneManager.
void UiScene::OnGlInitialized() {
  gl_initialized_ = true;
  for (auto& element : ui_elements_) {
    element->Initialize();
  }
}

}  // namespace vr
