// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene.h"

#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "chrome/browser/vr/databinding/binding_base.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {

template <typename P>
UiScene::Elements GetVisibleElements(UiElement* root,
                                     UiElement* reticle_element,
                                     P predicate) {
  Reticle* reticle = static_cast<Reticle*>(reticle_element);
  UiElement* target = reticle ? reticle->TargetElement() : nullptr;
  UiScene::Elements elements;
  for (auto& element : *root) {
    if (element.IsVisible() && predicate(&element)) {
      elements.push_back(&element);
      if (target && target->id() == element.id()) {
        elements.push_back(reticle);
        reticle->set_draw_phase(element.draw_phase());
      }
    }
  }
  return elements;
}

}  // namespace

void UiScene::AddUiElement(UiElementName parent,
                           std::unique_ptr<UiElement> element) {
  CHECK_GE(element->id(), 0);
  CHECK_EQ(GetUiElementById(element->id()), nullptr);
  CHECK_GE(element->draw_phase(), 0);
  if (gl_initialized_) {
    for (auto& child : *element) {
      child.Initialize(provider_);
    }
  }
  GetUiElementByName(parent)->AddChild(std::move(element));
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
                           const gfx::Vector3dF& look_at) {
  bool scene_dirty = !initialized_scene_ || is_dirty_;
  bool needs_redraw = false;
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
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateAnimations");

    // Process all animations and pre-binding work. I.e., induce any
    // time-related "dirtiness" on the scene graph.
    for (auto& element : *root_element_) {
      element.set_update_phase(UiElement::kDirty);
      if ((element.DoBeginFrame(current_time, look_at) ||
           element.updated_bindings_this_frame()) &&
          (element.IsVisible() || element.updated_visiblity_this_frame())) {
        scene_dirty = true;
      }
    }
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateTexturesAndSizes");

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
  }

  if (!scene_dirty) {
    // Nothing to update, so set all elements to the final update phase and
    // return early.
    for (auto& element : *root_element_) {
      element.set_update_phase(UiElement::kUpdatedWorldSpaceTransform);
    }
    return needs_redraw;
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateLayout");

    // Update layout, which depends on size. Note that the layout phase changes
    // the size of layout-type elements, as they adjust to fit the cumulative
    // size of their children. This must be done in reverse order, such that
    // children are correctly sized when laid out by their parent.
    for (auto& element : base::Reversed(*root_element_)) {
      element.DoLayOutChildren();
      element.set_update_phase(UiElement::kUpdatedLayout);
    }
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateWorldSpaceTransform");

    // Now that we have finalized our local values, we can safely update our
    // final, baked transform.
    root_element_->UpdateWorldSpaceTransformRecursive();
  }

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
      GetUiElementByName(k2dBrowsingRoot), GetUiElementByName(kReticle),
      [](UiElement* element) {
        return element->draw_phase() == kPhaseForeground ||
               element->draw_phase() == kPhaseFloorCeiling ||
               element->draw_phase() == kPhaseBackground;
      });
}

UiScene::Elements UiScene::GetVisible2dBrowsingOverlayElements() const {
  return GetVisibleElements(
      GetUiElementByName(k2dBrowsingRoot), GetUiElementByName(kReticle),
      [](UiElement* element) {
        return element->draw_phase() == kPhaseOverlayBackground ||
               element->draw_phase() == kPhaseOverlayForeground;
      });
}

UiScene::Elements UiScene::GetVisibleSplashScreenElements() const {
  return GetVisibleElements(
      GetUiElementByName(kSplashScreenRoot), GetUiElementByName(kReticle),
      [](UiElement* element) {
        return element->draw_phase() == kPhaseOverlayBackground ||
               element->draw_phase() == kPhaseOverlayForeground;
      });
}

UiScene::Elements UiScene::GetVisibleWebVrOverlayForegroundElements() const {
  return GetVisibleElements(
      GetUiElementByName(kWebVrRoot), GetUiElementByName(kReticle),
      [](UiElement* element) {
        return element->draw_phase() == kPhaseOverlayForeground;
      });
}

UiScene::Elements UiScene::GetVisibleControllerElements() const {
  return GetVisibleElements(
      GetUiElementByName(kControllerGroup), nullptr, [](UiElement* element) {
        if (element->name() == kReticle) {
          Reticle* reticle = static_cast<Reticle*>(element);
          // If the reticle has a non-null target element,
          // it would have been positioned elsewhere.
          bool need_to_add_reticle = !reticle->TargetElement();
          if (need_to_add_reticle) {
            // We must always update the reticle's draw phase when it is
            // included in a list of elements we vend. The other controller
            // elements are drawn in the foreground phase, so we will update the
            // reticle to match here.
            reticle->set_draw_phase(kPhaseForeground);
          }
          return need_to_add_reticle;
        }
        return element->draw_phase() == kPhaseForeground;
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

void UiScene::OnGlInitialized(SkiaSurfaceProvider* provider) {
  gl_initialized_ = true;
  provider_ = provider;

  for (auto& element : *root_element_)
    element.Initialize(provider);
}

}  // namespace vr
