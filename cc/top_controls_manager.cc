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
const int64 kShowHideMaxDurationMs = 175;
}

// static
scoped_ptr<TopControlsManager> TopControlsManager::Create(
    TopControlsManagerClient* client,
    float top_controls_height,
    float top_controls_show_threshold,
    float top_controls_hide_threshold) {
  return make_scoped_ptr(new TopControlsManager(client,
                                                top_controls_height,
                                                top_controls_show_threshold,
                                                top_controls_hide_threshold));
}

TopControlsManager::TopControlsManager(TopControlsManagerClient* client,
                                       float top_controls_height,
                                       float top_controls_show_threshold,
                                       float top_controls_hide_threshold)
    : client_(client),
      animation_direction_(NO_ANIMATION),
      enable_hiding_(true),
      in_scroll_gesture_(false),
      top_controls_height_(top_controls_height),
      controls_top_offset_(0),
      content_top_offset_(top_controls_height),
      previous_root_scroll_offset_(0.f),
      scroll_start_offset_(0.f),
      current_scroll_delta_(0.f),
      top_controls_show_height_(
          top_controls_height * top_controls_hide_threshold),
      top_controls_hide_height_(
          top_controls_height * (1.f - top_controls_show_threshold)) {
  CHECK(client_);
}

TopControlsManager::~TopControlsManager() {
}

void TopControlsManager::ScrollBegin() {
  ResetAnimations();
  in_scroll_gesture_ = true;
  scroll_start_offset_ = RootScrollLayerTotalScrollY() + controls_top_offset_;
  current_scroll_delta_ = 0.f;
}

gfx::Vector2dF TopControlsManager::ScrollBy(
    const gfx::Vector2dF pending_delta) {
  if (pending_delta.y() == 0 || !enable_hiding_)
    return pending_delta;

  current_scroll_delta_ += pending_delta.y();

  float scroll_total_y = RootScrollLayerTotalScrollY();

  if (in_scroll_gesture_ && pending_delta.y() > 0 && controls_top_offset_ == 0)
    scroll_start_offset_ = scroll_total_y;
  else if (in_scroll_gesture_ &&
      ((pending_delta.y() > 0 && scroll_total_y < scroll_start_offset_) ||
       (pending_delta.y() < 0 &&
           scroll_total_y > scroll_start_offset_ + top_controls_height_))) {
    return pending_delta;
  }

  ResetAnimations();
  return ScrollInternal(pending_delta);
}

gfx::Vector2dF TopControlsManager::ScrollInternal(
    const gfx::Vector2dF pending_delta) {
  float scroll_total_y = RootScrollLayerTotalScrollY();
  float scroll_delta_y = pending_delta.y();

  float previous_controls_offset = controls_top_offset_;
  float previous_content_offset = content_top_offset_;

  controls_top_offset_ -= scroll_delta_y;
  controls_top_offset_ = std::min(
      std::max(controls_top_offset_, -top_controls_height_), 0.f);

  // To determine if the scroll delta should be applied to the content position
  // as well, we check the following:
  // 1.) Scrolling down the page and the content position isn't already at 0.
  //     This case handles corrections where the page content shifts outside of
  //     the knowledge of the top controls manager.
  // 2.) Scrolling either direction while the root scroll layer is scrolled to
  //     the very top.
  if ((scroll_delta_y > 0 && content_top_offset_ > 0) || scroll_total_y <= 0) {
    float content_scroll_delta_y = scroll_delta_y;
    if (content_scroll_delta_y > 0)
      content_scroll_delta_y -= previous_controls_offset - controls_top_offset_;
    content_top_offset_ -= content_scroll_delta_y;
  }

  content_top_offset_ = std::max(
      std::min(content_top_offset_,
               controls_top_offset_ + top_controls_height_), 0.f);

  gfx::Vector2dF applied_delta(
      0.f, previous_content_offset - content_top_offset_);

  if (previous_controls_offset != controls_top_offset_ ||
      previous_content_offset != content_top_offset_) {
    client_->setNeedsRedraw();
    client_->setActiveTreeNeedsUpdateDrawProperties();
  }

  return pending_delta - applied_delta;
}

void TopControlsManager::ScrollEnd() {
  StartAnimationIfNecessary();
  previous_root_scroll_offset_ = RootScrollLayerTotalScrollY();
  in_scroll_gesture_ = false;
  current_scroll_delta_ = 0.f;
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
  if (controls_top_offset_ != 0
      && controls_top_offset_ != -top_controls_height_) {
    AnimationDirection show_controls = NO_ANIMATION;

    if (controls_top_offset_ >= -top_controls_show_height_) {
      // If we're showing so much that the hide threshold won't trigger, show.
      show_controls = SHOWING_CONTROLS;
    } else if (controls_top_offset_ <= -top_controls_hide_height_) {
      // If we're showing so little that the show threshold won't trigger, hide.
      show_controls = HIDING_CONTROLS;
    } else {
      // If we could be either showing or hiding, we determine which one to
      // do based on whether or not the total scroll delta was moving up or
      // down.
      show_controls = current_scroll_delta_ <= 0.f ?
          SHOWING_CONTROLS : HIDING_CONTROLS;
    }

    if (show_controls != NO_ANIMATION &&
        (!top_controls_animation_ || animation_direction_ != show_controls)) {
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
