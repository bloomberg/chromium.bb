// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/top_controls_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/time.h"
#include "cc/keyframed_animation_curve.h"
#include "cc/layer_tree_impl.h"
#include "cc/timing_function.h"
#include "cc/top_controls_manager_client.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {
namespace {
// These constants were chosen empirically for their visually pleasant behavior.
// Contact tedchoc@chromium.org for questions about changing these values.
const float kShowHideThreshold = 0.5f;
const int64 kShowHideMaxDurationMs = 175;
}

// static
scoped_ptr<TopControlsManager> TopControlsManager::Create(
    TopControlsManagerClient* client, float top_controls_height) {
  return make_scoped_ptr(new TopControlsManager(client, top_controls_height));
}

TopControlsManager::TopControlsManager(TopControlsManagerClient* client,
                                       float top_controls_height)
    : client_(client),
      animation_direction_(NO_ANIMATION),
      is_overlay_mode_(false),
      scroll_readjustment_enabled_(false),
      top_controls_height_(top_controls_height),
      controls_top_offset_(0),
      content_top_offset_(top_controls_height),
      previous_root_scroll_offset_(0.f) {
  CHECK(client_);
}

TopControlsManager::~TopControlsManager() {
}

void TopControlsManager::UpdateDrawPositions() {
  if (!client_->haveRootScrollLayer())
    return;

  // If the scroll position has changed underneath us (i.e. a javascript
  // scroll), then simulate a scroll that covers the delta.
  float scroll_total_y = RootScrollLayerTotalScrollY();
  if (scroll_readjustment_enabled_
      && scroll_total_y != previous_root_scroll_offset_) {
    ScrollBy(gfx::Vector2dF(0, scroll_total_y - previous_root_scroll_offset_));
    StartAnimationIfNecessary();
    previous_root_scroll_offset_ = RootScrollLayerTotalScrollY();
  }
}

void TopControlsManager::ScrollBegin() {
  ResetAnimations();
  scroll_readjustment_enabled_ = false;
}

gfx::Vector2dF TopControlsManager::ScrollBy(
    const gfx::Vector2dF pending_delta) {
  ResetAnimations();
  return ScrollInternal(pending_delta);
}

gfx::Vector2dF TopControlsManager::ScrollInternal(
    const gfx::Vector2dF pending_delta) {
  float scroll_total_y = RootScrollLayerTotalScrollY();
  float scroll_delta_y = pending_delta.y();

  float previous_controls_offset = controls_top_offset_;
  float previous_content_offset = content_top_offset_;
  bool previous_was_overlay = is_overlay_mode_;

  controls_top_offset_ -= scroll_delta_y;
  controls_top_offset_ = std::min(
      std::max(controls_top_offset_, -top_controls_height_), 0.f);

  if (scroll_total_y > 0 || (scroll_total_y == 0
      && content_top_offset_ < scroll_delta_y)) {
    is_overlay_mode_ = true;
    content_top_offset_ = 0;
  } else if (scroll_total_y <= 0 && (scroll_delta_y < 0
      || (scroll_delta_y > 0 && content_top_offset_ > 0))) {
    is_overlay_mode_ = false;
    content_top_offset_ -= scroll_delta_y;
  }
  content_top_offset_ = std::max(
      std::min(content_top_offset_,
               controls_top_offset_ + top_controls_height_), 0.f);

  gfx::Vector2dF applied_delta;
  if (!previous_was_overlay)
    applied_delta.set_y(previous_content_offset - content_top_offset_);

  if (is_overlay_mode_ != previous_was_overlay
      || previous_controls_offset != controls_top_offset_
      || previous_content_offset != content_top_offset_) {
    client_->setNeedsRedraw();
    client_->setNeedsUpdateDrawProperties();
  }

  return pending_delta - applied_delta;
}

void TopControlsManager::ScrollEnd() {
  StartAnimationIfNecessary();
  previous_root_scroll_offset_ = RootScrollLayerTotalScrollY();
  scroll_readjustment_enabled_ = true;
}

void TopControlsManager::Animate(base::TimeTicks monotonic_time) {
  if (!top_controls_animation_ || !client_->haveRootScrollLayer())
    return;

  double time = (monotonic_time - base::TimeTicks()).InMillisecondsF();
  float new_offset = top_controls_animation_->getValue(time);
  gfx::Vector2dF scroll_vector(0.f, -(new_offset - controls_top_offset_));
  ScrollInternal(scroll_vector);
  client_->setNeedsRedraw();

  if (IsAnimationCompleteAtTime(monotonic_time))
    ResetAnimations();
}

void TopControlsManager::ResetAnimations() {
  if (top_controls_animation_)
    top_controls_animation_.reset();

  animation_direction_ = NO_ANIMATION;
}

float TopControlsManager::RootScrollLayerTotalScrollY() {
  return client_->rootScrollLayerTotalScrollY();
}

void TopControlsManager::SetupAnimation(AnimationDirection direction) {
  top_controls_animation_ = KeyframedFloatAnimationCurve::create();
  double start_time =
      (base::TimeTicks::Now() - base::TimeTicks()).InMillisecondsF();
  top_controls_animation_->addKeyframe(
      FloatKeyframe::create(start_time, controls_top_offset_,
                            scoped_ptr<TimingFunction>()));
  float max_ending_offset =
      (direction == SHOWING_CONTROLS ? 1 : -1) * top_controls_height_;
  top_controls_animation_->addKeyframe(
      FloatKeyframe::create(start_time + kShowHideMaxDurationMs,
                            controls_top_offset_ + max_ending_offset,
                            EaseTimingFunction::create()));
  animation_direction_ = direction;
}

void TopControlsManager::StartAnimationIfNecessary() {
  float scroll_total_y = RootScrollLayerTotalScrollY();

  if (controls_top_offset_ != 0
      && controls_top_offset_ != -top_controls_height_) {
    AnimationDirection show_controls =
        controls_top_offset_ >= -(top_controls_height_ * kShowHideThreshold) ?
            SHOWING_CONTROLS : HIDING_CONTROLS;
    if (!top_controls_animation_ || animation_direction_ != show_controls) {
      SetupAnimation(show_controls);
      client_->setNeedsRedraw();
    }
  }
}

bool TopControlsManager::IsAnimationCompleteAtTime(base::TimeTicks time) {
  if (!top_controls_animation_)
    return true;

  double time_ms = (time - base::TimeTicks()).InMillisecondsF();
  float new_offset = top_controls_animation_->getValue(time_ms);

  if ((animation_direction_ == SHOWING_CONTROLS && new_offset >= 0) ||
      (animation_direction_ == HIDING_CONTROLS
          && new_offset <= -top_controls_height_)) {
    return true;
  }
  return false;
}

}  // namespace cc
