// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller_thinning.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

std::unique_ptr<ScrollbarAnimationControllerThinning>
ScrollbarAnimationControllerThinning::Create(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta fade_duration,
    base::TimeDelta thinning_duration) {
  return base::WrapUnique(new ScrollbarAnimationControllerThinning(
      scroll_layer_id, client, delay_before_starting,
      resize_delay_before_starting, fade_duration, thinning_duration));
}

ScrollbarAnimationControllerThinning::ScrollbarAnimationControllerThinning(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta fade_duration,
    base::TimeDelta thinning_duration)
    : ScrollbarAnimationController(scroll_layer_id,
                                   client,
                                   delay_before_starting,
                                   resize_delay_before_starting),
      opacity_(0.0f),
      fade_duration_(fade_duration) {
  vertical_controller_ = SingleScrollbarAnimationControllerThinning::Create(
      scroll_layer_id, ScrollbarOrientation::VERTICAL, client,
      thinning_duration);
  horizontal_controller_ = SingleScrollbarAnimationControllerThinning::Create(
      scroll_layer_id, ScrollbarOrientation::HORIZONTAL, client,
      thinning_duration);
  ApplyOpacityToScrollbars(0.0f);
}

ScrollbarAnimationControllerThinning::~ScrollbarAnimationControllerThinning() {}



bool ScrollbarAnimationControllerThinning::ScrollbarsHidden() const {
  return opacity_ == 0.0f;
}

bool ScrollbarAnimationControllerThinning::NeedThinningAnimation() const {
  return true;
}

void ScrollbarAnimationControllerThinning::RunAnimationFrame(float progress) {
  ApplyOpacityToScrollbars(1.f - progress);
  if (progress == 1.f)
    StopAnimation();
}

const base::TimeDelta& ScrollbarAnimationControllerThinning::Duration() {
  return fade_duration_;
}

void ScrollbarAnimationControllerThinning::DidScrollUpdate(bool on_resize) {
  if (Captured())
    return;

  ScrollbarAnimationController::DidScrollUpdate(on_resize);

  ApplyOpacityToScrollbars(1);
  vertical_controller_->UpdateThumbThicknessScale();
  horizontal_controller_->UpdateThumbThicknessScale();

  // we started a fade out timer in
  // |ScrollbarAnimationController::DidScrollUpdate| but don't want to
  // fade out if the mouse is nearby.
  if (mouse_is_near_any_scrollbar())
    StopAnimation();
}

void ScrollbarAnimationControllerThinning::DidScrollEnd() {
  ScrollbarAnimationController::DidScrollEnd();

  // Don't fade out the scrollbar when mouse is near.
  if (mouse_is_near_any_scrollbar())
    StopAnimation();
}

void ScrollbarAnimationControllerThinning::ApplyOpacityToScrollbars(
    float opacity) {
  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (!scrollbar->is_overlay_scrollbar())
      continue;
    float effective_opacity = scrollbar->CanScrollOrientation() ? opacity : 0;
    PropertyTrees* property_trees =
        scrollbar->layer_tree_impl()->property_trees();
    // If this method is called during LayerImpl::PushPropertiesTo, we may not
    // yet have valid layer_id_to_effect_node_index entries as property trees
    // are pushed after layers during activation. We can skip updating opacity
    // in that case as we are only registering a scrollbar and because opacity
    // will be overwritten anyway when property trees are pushed.
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT,
                                         scrollbar->id())) {
      property_trees->effect_tree.OnOpacityAnimated(
          effective_opacity,
          property_trees->layer_id_to_effect_node_index[scrollbar->id()],
          scrollbar->layer_tree_impl());
    }
  }

  bool previouslyVisible = opacity_ > 0.0f;
  bool currentlyVisible = opacity > 0.0f;

  opacity_ = opacity;

  if (previouslyVisible != currentlyVisible)
    client_->DidChangeScrollbarVisibility();
}

}  // namespace cc
