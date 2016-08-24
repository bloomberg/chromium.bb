// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scrollbar_animation_controller_thinning.h"

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/trees/layer_tree_impl.h"

namespace {
const float kIdleThicknessScale = 0.4f;
const float kIdleOpacity = 0.7f;
const float kDefaultMouseMoveDistanceToTriggerAnimation = 25.f;
}

namespace cc {

std::unique_ptr<ScrollbarAnimationControllerThinning>
ScrollbarAnimationControllerThinning::Create(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta duration) {
  return base::WrapUnique(new ScrollbarAnimationControllerThinning(
      scroll_layer_id, client, delay_before_starting,
      resize_delay_before_starting, duration));
}

ScrollbarAnimationControllerThinning::ScrollbarAnimationControllerThinning(
    int scroll_layer_id,
    ScrollbarAnimationControllerClient* client,
    base::TimeDelta delay_before_starting,
    base::TimeDelta resize_delay_before_starting,
    base::TimeDelta duration)
    : ScrollbarAnimationController(scroll_layer_id,
                                   client,
                                   delay_before_starting,
                                   resize_delay_before_starting,
                                   duration),
      mouse_is_over_scrollbar_(false),
      mouse_is_near_scrollbar_(false),
      thickness_change_(NONE),
      opacity_change_(NONE),
      mouse_move_distance_to_trigger_animation_(
          kDefaultMouseMoveDistanceToTriggerAnimation) {
  ApplyOpacityAndThumbThicknessScale(kIdleOpacity, kIdleThicknessScale);
}

ScrollbarAnimationControllerThinning::~ScrollbarAnimationControllerThinning() {}

void ScrollbarAnimationControllerThinning::RunAnimationFrame(float progress) {
  float opacity = OpacityAtAnimationProgress(progress);
  float thumb_thickness_scale =
      ThumbThicknessScaleAtAnimationProgress(progress);
  ApplyOpacityAndThumbThicknessScale(opacity, thumb_thickness_scale);
  client_->SetNeedsRedrawForScrollbarAnimation();
  if (progress == 1.f) {
    opacity_change_ = NONE;
    thickness_change_ = NONE;
    StopAnimation();
  }
}

void ScrollbarAnimationControllerThinning::DidMouseMoveOffScrollbar() {
  mouse_is_over_scrollbar_ = false;
  mouse_is_near_scrollbar_ = false;
  opacity_change_ = DECREASE;
  thickness_change_ = DECREASE;
  StartAnimation();
}

void ScrollbarAnimationControllerThinning::DidScrollUpdate(bool on_resize) {
  ScrollbarAnimationController::DidScrollUpdate(on_resize);
  ApplyOpacityAndThumbThicknessScale(
      1, mouse_is_near_scrollbar_ ? 1.f : kIdleThicknessScale);

  if (!mouse_is_over_scrollbar_)
    opacity_change_ = DECREASE;
}

void ScrollbarAnimationControllerThinning::DidMouseMoveNear(float distance) {
  bool mouse_is_over_scrollbar = distance == 0.0;
  bool mouse_is_near_scrollbar =
      distance < mouse_move_distance_to_trigger_animation_;

  if (mouse_is_over_scrollbar == mouse_is_over_scrollbar_ &&
      mouse_is_near_scrollbar == mouse_is_near_scrollbar_)
    return;

  if (mouse_is_over_scrollbar_ != mouse_is_over_scrollbar) {
    mouse_is_over_scrollbar_ = mouse_is_over_scrollbar;
    opacity_change_ = mouse_is_over_scrollbar_ ? INCREASE : DECREASE;
  }

  if (mouse_is_near_scrollbar_ != mouse_is_near_scrollbar) {
    mouse_is_near_scrollbar_ = mouse_is_near_scrollbar;
    thickness_change_ = mouse_is_near_scrollbar_ ? INCREASE : DECREASE;
  }

  StartAnimation();
}

float ScrollbarAnimationControllerThinning::OpacityAtAnimationProgress(
    float progress) {
  if (opacity_change_ == NONE)
    return mouse_is_over_scrollbar_ ? 1.f : kIdleOpacity;
  float factor = opacity_change_ == INCREASE ? progress : (1.f - progress);
  float ret = ((1.f - kIdleOpacity) * factor) + kIdleOpacity;
  return ret;
}

float ScrollbarAnimationControllerThinning::
    ThumbThicknessScaleAtAnimationProgress(float progress) {
  if (thickness_change_ == NONE)
    return mouse_is_near_scrollbar_ ? 1.f : kIdleThicknessScale;
  float factor = thickness_change_ == INCREASE ? progress : (1.f - progress);
  return ((1.f - kIdleThicknessScale) * factor) + kIdleThicknessScale;
}

float ScrollbarAnimationControllerThinning::AdjustScale(
    float new_value,
    float current_value,
    AnimationChange animation_change) {
  if (animation_change == INCREASE && current_value > new_value)
    return current_value;
  if (animation_change == DECREASE && current_value < new_value)
    return current_value;
  return new_value;
}

void ScrollbarAnimationControllerThinning::ApplyOpacityAndThumbThicknessScale(
    float opacity,
    float thumb_thickness_scale) {
  for (ScrollbarLayerImplBase* scrollbar : Scrollbars()) {
    if (!scrollbar->is_overlay_scrollbar())
      continue;
    float effective_opacity =
        scrollbar->CanScrollOrientation()
            ? AdjustScale(opacity, scrollbar->Opacity(), opacity_change_)
            : 0;

    PropertyTrees* property_trees =
        scrollbar->layer_tree_impl()->property_trees();
    // If this method is called during LayerImpl::PushPropertiesTo, we may not
    // yet have valid effect_id_to_index_map entries as property trees are
    // pushed after layers during activation. We can skip updating opacity in
    // that case as we are only registering a scrollbar and because opacity will
    // be overwritten anyway when property trees are pushed.
    if (property_trees->IsInIdToIndexMap(PropertyTrees::TreeType::EFFECT,
                                         scrollbar->id())) {
      property_trees->effect_tree.OnOpacityAnimated(
          effective_opacity,
          property_trees->effect_id_to_index_map[scrollbar->id()],
          scrollbar->layer_tree_impl());
    }
    scrollbar->SetThumbThicknessScaleFactor(AdjustScale(
        thumb_thickness_scale, scrollbar->thumb_thickness_scale_factor(),
        thickness_change_));
  }
}

}  // namespace cc
