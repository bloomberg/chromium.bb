// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/vr/databinding/binding_base.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

template <typename P>
UiScene::Elements GetVisibleElements(UiElement* root,
                                     P predicate) {
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
  InitializeElement(element.get());
  GetUiElementByName(parent)->AddChild(std::move(element));
  is_dirty_ = true;
}

void UiScene::AddParentUiElement(UiElementName child,
                                 std::unique_ptr<UiElement> element) {
  InitializeElement(element.get());
  auto* child_ptr = GetUiElementByName(child);
  CHECK_NE(nullptr, child_ptr);
  auto* parent_ptr = child_ptr->parent();
  CHECK_NE(nullptr, parent_ptr);
  element->AddChild(parent_ptr->RemoveChild(child_ptr));
  parent_ptr->AddChild(std::move(element));
  is_dirty_ = true;
}

std::unique_ptr<UiElement> UiScene::RemoveUiElement(int element_id) {
  UiElement* to_remove = GetUiElementById(element_id);
  CHECK_NE(nullptr, to_remove);
  CHECK_NE(nullptr, to_remove->parent());
  is_dirty_ = true;
  return to_remove->parent()->RemoveChild(to_remove);
}

bool UiScene::OnBeginFrame(const base::TimeTicks& current_time,
                           const gfx::Transform& head_pose) {
  bool scene_dirty = !initialized_scene_ || is_dirty_;
  initialized_scene_ = true;
  is_dirty_ = false;

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateBindings");

    // Propagate updates across bindings.
    for (auto& element : *root_element_) {
      element.UpdateBindings();
      element.set_update_phase(UiElement::kUpdatedBindings);
    }
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateAnimationsAndOpacity");

    // Process all animations and pre-binding work. I.e., induce any
    // time-related "dirtiness" on the scene graph.
    for (auto& element : *root_element_) {
      element.set_update_phase(UiElement::kDirty);
      if ((element.DoBeginFrame(current_time, head_pose) ||
           element.updated_bindings_this_frame()) &&
          (element.IsVisible() || element.updated_visiblity_this_frame())) {
        scene_dirty = true;
      }
    }
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateLayout");

    // TODO(mthiesse): We should really only be updating the sizes here, and not
    // actually redrawing the textures because we draw all of the textures as a
    // second phase after OnBeginFrame, once we've processed input. For now this
    // is fine, it's just a small amount of duplicated work.
    // Textures will have to know what their size would be, if they were to draw
    // with their current state, and changing anything other than texture
    // synchronously in response to input should be prohibited.
    for (auto& element : *root_element_) {
      element.set_update_phase(UiElement::kUpdatedTexturesAndSizes);
    }
    if (root_element_->SizeAndLayOut())
      scene_dirty = true;
    for (auto& element : *root_element_) {
      element.set_update_phase(UiElement::kUpdatedLayout);
    }
  }

  if (!scene_dirty) {
    // Nothing to update, so set all elements to the final update phase and
    // return early.
    for (auto& element : *root_element_) {
      element.set_update_phase(UiElement::kUpdatedWorldSpaceTransform);
    }
    return false;
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateWorldSpaceTransform");

    // Now that we have finalized our local values, we can safely update our
    // final, baked transform.
    const bool parent_transform_changed = false;
    root_element_->UpdateWorldSpaceTransformRecursive(parent_transform_changed);
  }

  return scene_dirty;
}

bool UiScene::UpdateTextures() {
  TRACE_EVENT0("gpu", "UiScene::UpdateTextures");
  bool needs_redraw = false;
  // Update textures and sizes.
  for (auto& element : *root_element_) {
    if (element.PrepareToDraw())
      needs_redraw = true;
  }
  return needs_redraw;
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

UiScene::Elements UiScene::GetVisibleElementsToDraw() const {
  return GetVisibleElements(GetUiElementByName(kRoot), [](UiElement* element) {
    return element->draw_phase() == kPhaseForeground ||
           element->draw_phase() == kPhaseBackplanes ||
           element->draw_phase() == kPhaseBackground;
  });
}

UiScene::Elements UiScene::GetVisibleWebVrOverlayElementsToDraw() const {
  return GetVisibleElements(
      GetUiElementByName(kWebVrRoot), [](UiElement* element) {
        return element->draw_phase() == kPhaseOverlayForeground;
      });
}

UiScene::Elements UiScene::GetPotentiallyVisibleElements() const {
  UiScene::Elements elements;
  for (auto& element : *root_element_) {
    if (element.draw_phase() != kPhaseNone)
      elements.push_back(&element);
  }
  return elements;
}

UiScene::UiScene() {
  root_element_ = std::make_unique<UiElement>();
  root_element_->SetName(kRoot);
}

UiScene::~UiScene() = default;

void UiScene::OnGlInitialized(SkiaSurfaceProvider* provider) {
  gl_initialized_ = true;
  provider_ = provider;
  for (auto& element : *root_element_)
    element.Initialize(provider_);
}

void UiScene::InitializeElement(UiElement* element) {
  CHECK_GE(element->id(), 0);
  CHECK_EQ(GetUiElementById(element->id()), nullptr);
  CHECK_GE(element->draw_phase(), 0);
  if (gl_initialized_) {
    for (auto& child : *element) {
      child.Initialize(provider_);
    }
  }
}

}  // namespace vr
