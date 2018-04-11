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

template <typename P, typename V>
void AddPredicatedVisibleSubTree(UiElement* root, P predicate, V* elements) {
  if (!root->IsVisible())
    return;
  if (predicate(root)) {
    elements->push_back(root);
  }
  for (auto& child : root->children()) {
    AddPredicatedVisibleSubTree(child.get(), predicate, elements);
  }
}

template <typename P>
UiScene::Elements GetVisibleElementsWithPredicate(UiElement* root,
                                                  P predicate) {
  UiScene::Elements result;
  AddPredicatedVisibleSubTree(root, predicate, &result);
  return result;
}

template <typename P>
UiScene::MutableElements GetVisibleElementsWithPredicateMutable(UiElement* root,
                                                                P predicate) {
  UiScene::MutableElements result;
  AddPredicatedVisibleSubTree(root, predicate, &result);
  return result;
}

void GetAllElementsRecursive(std::vector<UiElement*>* elements, UiElement* e) {
  e->set_descendants_updated(false);
  elements->push_back(e);
  for (auto& child : e->children())
    GetAllElementsRecursive(elements, child.get());
}

template <typename P>
UiElement* FindElement(UiElement* e, P predicate) {
  if (predicate.Run(e))
    return e;
  for (auto& child : e->children()) {
    if (UiElement* result = FindElement(child.get(), predicate)) {
      return result;
    }
  }
  return nullptr;
}

void InitializeElementRecursive(UiElement* e, SkiaSurfaceProvider* provider) {
  e->Initialize(provider);
  for (auto& child : e->children())
    InitializeElementRecursive(child.get(), provider);
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

  auto& elements = GetAllElements();

  for (auto* element : elements) {
    element->set_update_phase(UiElement::kDirty);
    element->set_last_frame_time(current_time);
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateBindings");

    // Propagate updates across bindings.
    root_element_->UpdateBindings();
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateAnimationsAndOpacity");

    // Process all animations and pre-binding work. I.e., induce any
    // time-related "dirtiness" on the scene graph.
    scene_dirty |= root_element_->DoBeginFrame(head_pose);
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateLayout");
    scene_dirty |= root_element_->SizeAndLayOut();
  }

  if (!scene_dirty) {
    // Nothing to update, so set all elements to the final update phase and
    // return early.
    for (auto* element : elements) {
      element->set_update_phase(UiElement::kUpdatedWorldSpaceTransform);
    }
    return false;
  }

  {
    TRACE_EVENT0("gpu", "UiScene::OnBeginFrame.UpdateWorldSpaceTransform");

    // Now that we have finalized our local values, we can safely update our
    // final, baked transform.
    const bool parent_transform_changed = false;
    root_element_->UpdateWorldSpaceTransform(parent_transform_changed);
  }

  return scene_dirty;
}

bool UiScene::UpdateTextures() {
  bool needs_redraw = false;
  std::vector<UiElement*> elements = GetVisibleElementsMutable();
  for (auto* element : elements) {
    needs_redraw |= element->UpdateTexture();
    element->set_update_phase(UiElement::kUpdatedTextures);
  }
  return needs_redraw;
}

UiElement& UiScene::root_element() {
  return *root_element_;
}

UiElement* UiScene::GetUiElementById(int element_id) const {
  return FindElement(
      root_element_.get(),
      base::BindRepeating([](int id, UiElement* e) { return e->id() == id; },
                          element_id));
}

UiElement* UiScene::GetUiElementByName(UiElementName name) const {
  return FindElement(
      root_element_.get(),
      base::BindRepeating(
          [](UiElementName name, UiElement* e) { return e->name() == name; },
          name));
}

std::vector<UiElement*>& UiScene::GetAllElements() {
  if (root_element_->descendants_updated()) {
    all_elements_.clear();
    GetAllElementsRecursive(&all_elements_, root_element_.get());
  }
  return all_elements_;
}

UiScene::Elements UiScene::GetVisibleElements() {
  return GetVisibleElementsWithPredicate(
      root_element_.get(), [](UiElement* element) { return true; });
}

UiScene::MutableElements UiScene::GetVisibleElementsMutable() {
  return GetVisibleElementsWithPredicateMutable(
      root_element_.get(), [](UiElement* element) { return true; });
}

UiScene::Elements UiScene::GetVisibleElementsToDraw() {
  return GetVisibleElementsWithPredicate(
      root_element_.get(), [](UiElement* element) {
        return element->draw_phase() == kPhaseForeground ||
               element->draw_phase() == kPhaseBackplanes ||
               element->draw_phase() == kPhaseBackground;
      });
}

UiScene::Elements UiScene::GetVisibleWebVrOverlayElementsToDraw() {
  auto* webvr_root = GetUiElementByName(kWebVrRoot);
  return GetVisibleElementsWithPredicate(webvr_root, [](UiElement* element) {
    return element->draw_phase() == kPhaseOverlayForeground;
  });
}

UiScene::UiScene() {
  root_element_ = std::make_unique<UiElement>();
  root_element_->SetName(kRoot);
}

UiScene::~UiScene() = default;

void UiScene::OnGlInitialized(SkiaSurfaceProvider* provider) {
  gl_initialized_ = true;
  provider_ = provider;
  InitializeElementRecursive(root_element_.get(), provider_);
}

void UiScene::InitializeElement(UiElement* element) {
  CHECK_GE(element->id(), 0);
  CHECK_EQ(GetUiElementById(element->id()), nullptr);
  CHECK_GE(element->draw_phase(), 0);
  if (gl_initialized_)
    InitializeElementRecursive(element, provider_);
}

}  // namespace vr
