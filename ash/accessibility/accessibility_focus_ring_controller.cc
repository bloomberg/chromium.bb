// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_focus_ring_controller.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_cursor_ring_layer.h"
#include "ash/accessibility/accessibility_focus_ring_group.h"
#include "ash/accessibility/accessibility_focus_ring_layer.h"
#include "ash/accessibility/accessibility_highlight_layer.h"
#include "ash/accessibility/focus_ring_layer.h"
#include "ash/accessibility/layer_animation_info.h"
#include "base/logging.h"

namespace ash {

namespace {

// Cursor constants.
constexpr int kCursorFadeInTimeMilliseconds = 400;
constexpr int kCursorFadeOutTimeMilliseconds = 1200;
constexpr int kCursorRingColorRed = 255;
constexpr int kCursorRingColorGreen = 51;
constexpr int kCursorRingColorBlue = 51;

// Caret constants.
constexpr int kCaretFadeInTimeMilliseconds = 100;
constexpr int kCaretFadeOutTimeMilliseconds = 1600;
constexpr int kCaretRingColorRed = 51;
constexpr int kCaretRingColorGreen = 51;
constexpr int kCaretRingColorBlue = 255;

// Highlight constants.
constexpr float kHighlightOpacity = 0.3f;

}  // namespace

AccessibilityFocusRingController::AccessibilityFocusRingController()
    : binding_(this) {
  cursor_animation_info_.fade_in_time =
      base::TimeDelta::FromMilliseconds(kCursorFadeInTimeMilliseconds);
  cursor_animation_info_.fade_out_time =
      base::TimeDelta::FromMilliseconds(kCursorFadeOutTimeMilliseconds);
  caret_animation_info_.fade_in_time =
      base::TimeDelta::FromMilliseconds(kCaretFadeInTimeMilliseconds);
  caret_animation_info_.fade_out_time =
      base::TimeDelta::FromMilliseconds(kCaretFadeOutTimeMilliseconds);
}

AccessibilityFocusRingController::~AccessibilityFocusRingController() = default;

void AccessibilityFocusRingController::BindRequest(
    mojom::AccessibilityFocusRingControllerRequest request) {
  binding_.Bind(std::move(request));
}

void AccessibilityFocusRingController::SetFocusRingColor(
    SkColor color,
    const std::string& caller_id) {
  AccessibilityFocusRingGroup* focus_ring_group =
      GetFocusRingGroupForCallerId(caller_id, /* Create if missing */ true);
  focus_ring_group->SetColor(color, this);
}

void AccessibilityFocusRingController::ResetFocusRingColor(
    const std::string& caller_id) {
  AccessibilityFocusRingGroup* focus_ring_group =
      GetFocusRingGroupForCallerId(caller_id, /* Do not create */ false);
  if (!focus_ring_group)
    return;
  focus_ring_group->ResetColor(this);
}

void AccessibilityFocusRingController::SetFocusRing(
    const std::vector<gfx::Rect>& rects,
    mojom::FocusRingBehavior focus_ring_behavior,
    const std::string& caller_id) {
  AccessibilityFocusRingGroup* focus_ring_group =
      GetFocusRingGroupForCallerId(caller_id, /* Create if missing */ true);
  if (focus_ring_group->SetFocusRectsAndBehavior(rects, focus_ring_behavior,
                                                 this))
    OnLayerChange(focus_ring_group->focus_animation_info());
}

void AccessibilityFocusRingController::HideFocusRing(
    const std::string& caller_id) {
  AccessibilityFocusRingGroup* focus_ring_group =
      GetFocusRingGroupForCallerId(caller_id, /* Do not create */ false);
  if (!focus_ring_group)
    return;
  focus_ring_group->ClearFocusRects(this);
  OnLayerChange(focus_ring_group->focus_animation_info());
}

void AccessibilityFocusRingController::SetHighlights(
    const std::vector<gfx::Rect>& rects,
    SkColor color) {
  highlight_rects_ = rects;
  GetColorAndOpacityFromColor(color, kHighlightOpacity, &highlight_color_,
                              &highlight_opacity_);
  UpdateHighlightFromHighlightRects();
}

void AccessibilityFocusRingController::HideHighlights() {
  highlight_rects_.clear();
  UpdateHighlightFromHighlightRects();
}

void AccessibilityFocusRingController::UpdateHighlightFromHighlightRects() {
  if (!highlight_layer_)
    highlight_layer_ = std::make_unique<AccessibilityHighlightLayer>(this);
  highlight_layer_->Set(highlight_rects_, highlight_color_);
  highlight_layer_->SetOpacity(highlight_opacity_);
}

void AccessibilityFocusRingController::OnLayerChange(
    LayerAnimationInfo* animation_info) {
  animation_info->change_time = base::TimeTicks::Now();
  if (animation_info->opacity == 0)
    animation_info->start_time = animation_info->change_time;
}

void AccessibilityFocusRingController::SetCursorRing(
    const gfx::Point& location) {
  cursor_location_ = location;
  if (!cursor_layer_) {
    cursor_layer_.reset(new AccessibilityCursorRingLayer(
        this, kCursorRingColorRed, kCursorRingColorGreen,
        kCursorRingColorBlue));
  }
  cursor_layer_->Set(location);
  OnLayerChange(&cursor_animation_info_);
}

void AccessibilityFocusRingController::HideCursorRing() {
  cursor_layer_.reset();
}

void AccessibilityFocusRingController::SetCaretRing(
    const gfx::Point& location) {
  caret_location_ = location;

  if (!caret_layer_) {
    caret_layer_.reset(new AccessibilityCursorRingLayer(
        this, kCaretRingColorRed, kCaretRingColorGreen, kCaretRingColorBlue));
  }

  caret_layer_->Set(location);
  OnLayerChange(&caret_animation_info_);
}

void AccessibilityFocusRingController::HideCaretRing() {
  caret_layer_.reset();
}

void AccessibilityFocusRingController::SetNoFadeForTesting() {
  no_fade_for_testing_ = true;
  for (auto iter = focus_ring_groups_.begin(); iter != focus_ring_groups_.end();
       ++iter) {
    iter->second->set_no_fade_for_testing();
    iter->second->focus_animation_info()->fade_in_time = base::TimeDelta();
    iter->second->focus_animation_info()->fade_out_time =
        base::TimeDelta::FromHours(1);
  }
  cursor_animation_info_.fade_in_time = base::TimeDelta();
  cursor_animation_info_.fade_out_time = base::TimeDelta::FromHours(1);
  caret_animation_info_.fade_in_time = base::TimeDelta();
  caret_animation_info_.fade_out_time = base::TimeDelta::FromHours(1);
}

const AccessibilityFocusRingGroup*
AccessibilityFocusRingController::GetFocusRingGroupForTesting(
    std::string caller_id) {
  return GetFocusRingGroupForCallerId(caller_id, /* create if missing */ false);
}

void AccessibilityFocusRingController::GetColorAndOpacityFromColor(
    SkColor color,
    float default_opacity,
    SkColor* result_color,
    float* result_opacity) {
  int alpha = SkColorGetA(color);
  if (alpha == 0xFF) {
    *result_opacity = default_opacity;
  } else {
    *result_opacity = SkColor4f::FromColor(color).fA;
  }
  *result_color = SkColorSetA(color, 0xFF);
}

void AccessibilityFocusRingController::OnDeviceScaleFactorChanged() {
  for (auto iter = focus_ring_groups_.begin(); iter != focus_ring_groups_.end();
       ++iter)
    iter->second->UpdateFocusRingsFromFocusRects(this);
}

void AccessibilityFocusRingController::OnAnimationStep(
    base::TimeTicks timestamp) {
  for (auto iter = focus_ring_groups_.begin(); iter != focus_ring_groups_.end();
       ++iter) {
    if (iter->second->CanAnimate())
      iter->second->AnimateFocusRings(timestamp);
  }

  if (cursor_layer_ && cursor_layer_->CanAnimate())
    AnimateCursorRing(timestamp);

  if (caret_layer_ && caret_layer_->CanAnimate())
    AnimateCaretRing(timestamp);
}

void AccessibilityFocusRingController::AnimateCursorRing(
    base::TimeTicks timestamp) {
  CHECK(cursor_layer_);

  ComputeOpacity(&cursor_animation_info_, timestamp);
  if (cursor_animation_info_.opacity == 0.0) {
    cursor_layer_.reset();
    return;
  }
  cursor_layer_->SetOpacity(cursor_animation_info_.opacity);
}

void AccessibilityFocusRingController::AnimateCaretRing(
    base::TimeTicks timestamp) {
  CHECK(caret_layer_);

  ComputeOpacity(&caret_animation_info_, timestamp);
  if (caret_animation_info_.opacity == 0.0) {
    caret_layer_.reset();
    return;
  }
  caret_layer_->SetOpacity(caret_animation_info_.opacity);
}

AccessibilityFocusRingGroup*
AccessibilityFocusRingController::GetFocusRingGroupForCallerId(
    std::string caller_id_to_focus_ring_group_,
    bool create) {
  auto iter = focus_ring_groups_.find(caller_id_to_focus_ring_group_);
  if (iter != focus_ring_groups_.end())
    return iter->second.get();

  if (!create)
    return nullptr;

  // Add it and then return it.
  focus_ring_groups_[caller_id_to_focus_ring_group_] =
      std::make_unique<AccessibilityFocusRingGroup>();
  if (no_fade_for_testing_) {
    SetNoFadeForTesting();
  }
  return focus_ring_groups_[caller_id_to_focus_ring_group_].get();
}

}  // namespace ash
