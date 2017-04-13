// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"
#include "chrome/browser/android/vr_shell/ui_element.h"
#include "device/vr/vr_math.h"

namespace vr_shell {

namespace {

void ApplyAnchoring(const UiElement& parent,
                    XAnchoring x_anchoring,
                    YAnchoring y_anchoring,
                    Transform* transform) {
  // To anchor a child, use the parent's size to find its edge.
  float x_offset;
  switch (x_anchoring) {
    case XLEFT:
      x_offset = -0.5f * parent.size.x();
      break;
    case XRIGHT:
      x_offset = 0.5f * parent.size.x();
      break;
    case XNONE:
      x_offset = 0.0f;
      break;
  }
  float y_offset;
  switch (y_anchoring) {
    case YTOP:
      y_offset = 0.5f * parent.size.y();
      break;
    case YBOTTOM:
      y_offset = -0.5f * parent.size.y();
      break;
    case YNONE:
      y_offset = 0.0f;
      break;
  }
  transform->Translate(gfx::Vector3dF(x_offset, y_offset, 0));
}

}  // namespace

void UiScene::AddUiElement(std::unique_ptr<UiElement> element) {
  CHECK_GE(element->id, 0);
  CHECK_EQ(GetUiElementById(element->id), nullptr);
  if (element->parent_id >= 0) {
    CHECK_NE(GetUiElementById(element->parent_id), nullptr);
  } else {
    CHECK_EQ(element->x_anchoring, XAnchoring::XNONE);
    CHECK_EQ(element->y_anchoring, YAnchoring::YNONE);
  }
  ui_elements_.push_back(std::move(element));
}

void UiScene::RemoveUiElement(int element_id) {
  for (auto it = ui_elements_.begin(); it != ui_elements_.end(); ++it) {
    if ((*it)->id == element_id) {
      if ((*it)->fill == Fill::CONTENT) {
        content_element_ = nullptr;
      }
      ui_elements_.erase(it);
      return;
    }
  }
}

void UiScene::AddAnimation(int element_id,
                           std::unique_ptr<Animation> animation) {
  UiElement* element = GetUiElementById(element_id);
  CHECK_NE(element, nullptr);
  for (auto& existing_animation : element->animations) {
    CHECK_NE(existing_animation->id, animation->id);
  }
  element->animations.emplace_back(std::move(animation));
}

void UiScene::RemoveAnimation(int element_id, int animation_id) {
  UiElement* element = GetUiElementById(element_id);
  CHECK_NE(element, nullptr);
  auto& animations = element->animations;
  for (auto it = animations.begin(); it != animations.end(); ++it) {
    const Animation& existing_animation = **it;
    if (existing_animation.id == animation_id) {
      animations.erase(it);
      return;
    }
  }
}

void UiScene::UpdateTransforms(const base::TimeTicks& time) {
  for (auto& element : ui_elements_) {
    // Process all animations before calculating object transforms.
    element->Animate(time);
    element->dirty = true;
  }
  for (auto& element : ui_elements_) {
    ApplyRecursiveTransforms(element.get());
  }
}

UiElement* UiScene::GetUiElementById(int element_id) {
  for (auto& element : ui_elements_) {
    if (element->id == element_id) {
      return element.get();
    }
  }
  return nullptr;
}

std::vector<const UiElement*> UiScene::GetWorldElements() const {
  std::vector<const UiElement*> elements;
  for (const auto& element : ui_elements_) {
    if (element->IsVisible() && !element->lock_to_fov) {
      elements.push_back(element.get());
    }
  }
  return elements;
}

std::vector<const UiElement*> UiScene::GetHeadLockedElements() const {
  std::vector<const UiElement*> elements;
  for (const auto& element : ui_elements_) {
    if (element->IsVisible() && element->lock_to_fov) {
      elements.push_back(element.get());
    }
  }
  return elements;
}

bool UiScene::HasVisibleHeadLockedElements() const {
  return !GetHeadLockedElements().empty();
}

const vr::Colorf& UiScene::GetBackgroundColor() const {
  return background_color_;
}

float UiScene::GetBackgroundDistance() const {
  return background_distance_;
}

bool UiScene::GetWebVrRenderingEnabled() const {
  return webvr_rendering_enabled_;
}

const std::vector<std::unique_ptr<UiElement>>& UiScene::GetUiElements() const {
  return ui_elements_;
}

UiScene::UiScene() = default;

UiScene::~UiScene() = default;

void UiScene::ApplyRecursiveTransforms(UiElement* element) {
  if (!element->dirty)
    return;

  UiElement* parent = nullptr;
  if (element->parent_id >= 0) {
    parent = GetUiElementById(element->parent_id);
    CHECK(parent != nullptr);
  }

  Transform* transform = element->mutable_transform();
  transform->MakeIdentity();
  transform->Scale(element->size);
  element->computed_opacity = element->opacity;
  element->computed_lock_to_fov = element->lock_to_fov;

  // Compute an inheritable transformation that can be applied to this element,
  // and it's children, if applicable.
  Transform* inheritable = &element->inheritable_transform;
  inheritable->MakeIdentity();
  inheritable->Scale(element->scale);
  inheritable->Rotate(element->rotation);
  inheritable->Translate(element->translation);
  if (parent) {
    ApplyAnchoring(*parent, element->x_anchoring, element->y_anchoring,
                   inheritable);
    ApplyRecursiveTransforms(parent);
    vr::MatrixMul(parent->inheritable_transform.to_world, inheritable->to_world,
                  &inheritable->to_world);

    element->computed_opacity *= parent->opacity;
    element->computed_lock_to_fov = parent->lock_to_fov;
  }

  vr::MatrixMul(inheritable->to_world, transform->to_world,
                &transform->to_world);
  element->dirty = false;
}

}  // namespace vr_shell
