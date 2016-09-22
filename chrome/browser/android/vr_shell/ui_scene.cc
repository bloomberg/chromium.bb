// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene.h"

namespace vr_shell {

void UiScene::AddUiElement(std::unique_ptr<ContentRectangle>& element) {
  DCHECK_GE(element->id, 0);
  DCHECK_EQ(GetElementById(element->id), nullptr);
  if (element->parent_id >= 0) {
    DCHECK_NE(GetElementById(element->parent_id), nullptr);
  } else {
    DCHECK_EQ(element->x_anchoring, XAnchoring::XNONE);
    DCHECK_EQ(element->y_anchoring, YAnchoring::YNONE);
  }
  ui_elements_.push_back(std::move(element));
}

void UiScene::RemoveUiElement(int id) {
  for (auto it = ui_elements_.begin(); it != ui_elements_.end(); ++it) {
    if ((*it)->id == id) {
      ui_elements_.erase(it);
      return;
    }
  }
}

void UiScene::AddAnimation(int id,
                           std::unique_ptr<Animation>& animation) {
  ContentRectangle* element = GetElementById(id);
  DCHECK_NE(element, nullptr);

  // Remove a previous animation if one was present.
  RemoveAnimationType(element, animation->property);

  element->animations.emplace_back(std::move(animation));
}

void UiScene::RemoveAnimation(int id, Animation::Property property) {
  ContentRectangle* element = GetElementById(id);
  DCHECK_NE(element, nullptr);
  RemoveAnimationType(element, property);
}

void UiScene::UpdateTransforms(float screen_tilt, int64_t time) {
  // Process all animations before calculating object transforms.
  for (auto& element : ui_elements_) {
    element->Animate(time);
  }
  for (auto& element : ui_elements_) {
    element->transform.MakeIdentity();
    element->transform.Scale(element->size.x, element->size.y, element->size.z);
    ApplyRecursiveTransforms(*element.get(), &element->transform);
    element->transform.Rotate(1.0f, 0.0f, 0.0f, screen_tilt);
  }
}

ContentRectangle* UiScene::GetElementById(int id) {
  for (auto& element : ui_elements_) {
    if (element->id == id) {
      return element.get();
    }
  }
  return nullptr;
}

const std::vector<std::unique_ptr<ContentRectangle>>&
UiScene::GetUiElements() const {
  return ui_elements_;
}

UiScene::UiScene() = default;

UiScene::~UiScene() = default;

void UiScene::ApplyAnchoring(const ContentRectangle& parent,
                             XAnchoring x_anchoring,
                             YAnchoring y_anchoring,
                             ReversibleTransform* transform) {
  // To anchor a child, use the parent's size to find its edge.
  float x_offset;
  switch (x_anchoring) {
    case XLEFT:
      x_offset = -0.5f * parent.size.x;
      break;
    case XRIGHT:
      x_offset = 0.5f * parent.size.x;
      break;
    case XNONE:
      x_offset = 0.0f;
      break;
  }
  float y_offset;
  switch (y_anchoring) {
    case YTOP:
      y_offset = 0.5f * parent.size.y;
      break;
    case YBOTTOM:
      y_offset = -0.5f * parent.size.y;
      break;
    case YNONE:
      y_offset = 0.0f;
      break;
  }
  transform->Translate(x_offset, y_offset, 0);
}

void UiScene::ApplyRecursiveTransforms(const ContentRectangle& element,
                                       ReversibleTransform* transform) {
  transform->Scale(element.scale.x, element.scale.y, element.scale.z);
  transform->Rotate(element.rotation.x, element.rotation.y,
                    element.rotation.z, element.rotation.angle);
  transform->Translate(element.translation.x, element.translation.y,
                       element.translation.z);

  if (element.parent_id >= 0) {
    const ContentRectangle* parent = GetElementById(element.parent_id);
    DCHECK(parent != nullptr);
    ApplyAnchoring(*parent, element.x_anchoring, element.y_anchoring,
                   transform);
    ApplyRecursiveTransforms(*parent, transform);
  }
}

void UiScene::RemoveAnimationType(ContentRectangle *element,
                                  Animation::Property property) {
  auto& animations = element->animations;
  for (auto it = animations.begin(); it != animations.end(); ++it) {
    const Animation& existing_animation = **it;
    if (existing_animation.property == property) {
      animations.erase(it);
      return;
    }
  }
}

}  // namespace vr_shell
