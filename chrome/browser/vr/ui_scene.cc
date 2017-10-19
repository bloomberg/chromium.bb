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
  is_dirty_ = true;
}

void UiScene::RemoveUiElement(int element_id) {
  UiElement* to_remove = GetUiElementById(element_id);
  CHECK_NE(nullptr, to_remove);
  CHECK_NE(nullptr, to_remove->parent());
  to_remove->parent()->RemoveChild(to_remove);
  is_dirty_ = true;
}

bool UiScene::OnBeginFrame(const base::TimeTicks& current_time,
                           const gfx::Vector3dF& look_at) {
  bool scene_dirty = !initialized_scene_ || is_dirty_;
  bool needs_redraw = false;
  initialized_scene_ = true;
  is_dirty_ = false;

  // Process all animations and pre-binding work. I.e., induce any time-related
  // "dirtiness" on the scene graph.
  for (auto& element : *root_element_) {
    element.set_update_phase(UiElement::kDirty);
    if (element.DoBeginFrame(current_time, look_at))
      scene_dirty = true;
    element.set_update_phase(UiElement::kUpdatedAnimations);
  }

  // Propagate updates across bindings.
  for (auto& element : *root_element_) {
    if (element.UpdateBindings())
      scene_dirty = true;
    element.set_update_phase(UiElement::kUpdatedBindings);
  }

  if (scene_dirty) {
    // We must now update visibility since some texture update optimizations
    // rely on accurate visibility information.
    root_element_->UpdateComputedOpacityRecursive();
  } else {
    for (auto& element : *root_element_)
      element.set_update_phase(UiElement::kUpdatedComputedOpacity);
  }

  // Update textures and sizes.
  // TODO(mthiesse): We should really only be updating the sizes here, and not
  // actually redrawing the textures because we draw all of the textures as a
  // second phase after OnBeginFrame, once we've processed input. For now this
  // is fine, it's just a small amount of duplicated work.
  // Textures will have to know what their size would be, if they were to draw
  // with their current state, and changing anything other than texture
  // synchronously in response to input should be prohibited.
  for (auto& element : *root_element_) {
    if (element.PrepareToDraw())
      needs_redraw = true;
    element.set_update_phase(UiElement::kUpdatedTexturesAndSizes);
  }

  if (!scene_dirty) {
    // Nothing to update, so set all elements to the final update phase and
    // return early.
    for (auto& element : *root_element_) {
      element.set_update_phase(UiElement::kUpdatedWorldSpaceTransform);
    }
    return needs_redraw;
  }

  // Update layout, which depends on size.
  for (auto& element : *root_element_) {
    element.LayOutChildren();
    element.set_update_phase(UiElement::kUpdatedLayout);
  }

  // Now that we have finalized our local values, we can safely update our
  // final, baked transform.
  root_element_->UpdateWorldSpaceTransformRecursive();
  return scene_dirty || needs_redraw;
}

bool UiScene::UpdateTextures() {
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
