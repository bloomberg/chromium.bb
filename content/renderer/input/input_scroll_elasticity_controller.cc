// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_scroll_elasticity_controller.h"

#include <math.h>

// ScrollElasticityController and ScrollElasticityControllerClient are based on
// WebKit/Source/platform/mac/ScrollElasticityController.mm
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

namespace content {

namespace {

// TODO(ccameron): This is advertised as system uptime, but it used to compute
// deltas against the event timestamps, which are in seconds since epoch. Find
// out which is right. Also, avoid querying time, and use frame time instead.
double SystemUptime() {
  return base::Time::Now().ToDoubleT();
}

const float kScrollVelocityZeroingTimeout = 0.10f;
const float kRubberbandDirectionLockStretchRatio = 1;
const float kRubberbandMinimumRequiredDeltaBeforeStretch = 10;

const float kRubberbandStiffness = 20;
const float kRubberbandAmplitude = 0.31f;
const float kRubberbandPeriod = 1.6f;

float ElasticDeltaForTimeDelta(float initial_position,
                               float initial_velocity,
                               float elapsed_time) {
  float amplitude = kRubberbandAmplitude;
  float period = kRubberbandPeriod;
  float critical_dampening_factor =
      expf((-elapsed_time * kRubberbandStiffness) / period);

  return (initial_position + (-initial_velocity * elapsed_time * amplitude)) *
         critical_dampening_factor;
}

float ElasticDeltaForReboundDelta(float delta) {
  float stiffness = std::max(kRubberbandStiffness, 1.0f);
  return delta / stiffness;
}

float ReboundDeltaForElasticDelta(float delta) {
  return delta * kRubberbandStiffness;
}

float ScrollWheelMultiplier() {
  static float multiplier = -1;
  if (multiplier < 0) {
    // TODO(ccameron): Find a place fo this in an Objective C file, or find an
    // equivalent C call.
    // multiplier = [[NSUserDefaults standardUserDefaults]
    //     floatForKey:@"NSScrollWheelMultiplier"];
    if (multiplier <= 0)
      multiplier = 1;
  }
  return multiplier;
}

}  // namespace

ScrollElasticityController::ScrollElasticityController(
    ScrollElasticityControllerClient* client)
    : client_(client),
      in_scroll_gesture_(false),
      has_scrolled_(false),
      momentum_scroll_in_progress_(false),
      ignore_momentum_scrolls_(false),
      last_momentum_scroll_timestamp_(0),
      snap_rubberband_timer_is_active_(false) {
}

bool ScrollElasticityController::HandleWheelEvent(
    const blink::WebMouseWheelEvent& wheel_event) {
  if (wheel_event.phase == blink::WebMouseWheelEvent::PhaseMayBegin)
    return false;

  if (wheel_event.phase == blink::WebMouseWheelEvent::PhaseBegan) {
    in_scroll_gesture_ = true;
    has_scrolled_ = false;
    momentum_scroll_in_progress_ = false;
    ignore_momentum_scrolls_ = false;
    last_momentum_scroll_timestamp_ = 0;
    momentum_velocity_ = gfx::Vector2dF();

    gfx::Vector2dF stretch_amount = client_->StretchAmount();
    stretch_scroll_force_.set_x(
        ReboundDeltaForElasticDelta(stretch_amount.x()));
    stretch_scroll_force_.set_y(
        ReboundDeltaForElasticDelta(stretch_amount.y()));
    overflow_scroll_delta_ = gfx::Vector2dF();

    StopSnapRubberbandTimer();

    // TODO(erikchen): Use the commented out line once Chromium uses the
    // return value correctly.
    // crbug.com/375512
    // return ShouldHandleEvent(wheel_event);

    // This logic is incorrect, since diagonal wheel events are not consumed.
    if (client_->PinnedInDirection(gfx::Vector2dF(-wheel_event.deltaX, 0))) {
      if (wheel_event.deltaX > 0 && !wheel_event.canRubberbandLeft)
        return false;
      if (wheel_event.deltaX < 0 && !wheel_event.canRubberbandRight)
        return false;
    }

    return true;
  }

  if (wheel_event.phase == blink::WebMouseWheelEvent::PhaseEnded ||
      wheel_event.phase == blink::WebMouseWheelEvent::PhaseCancelled) {
    SnapRubberband();
    return has_scrolled_;
  }

  bool isMomentumScrollEvent =
      (wheel_event.momentumPhase != blink::WebMouseWheelEvent::PhaseNone);
  if (ignore_momentum_scrolls_ &&
      (isMomentumScrollEvent || snap_rubberband_timer_is_active_)) {
    if (wheel_event.momentumPhase == blink::WebMouseWheelEvent::PhaseEnded) {
      ignore_momentum_scrolls_ = false;
      return true;
    }
    return false;
  }

  if (!ShouldHandleEvent(wheel_event))
    return false;

  float delta_x = overflow_scroll_delta_.x() - wheel_event.deltaX;
  float delta_y = overflow_scroll_delta_.y() - wheel_event.deltaY;
  float event_coalesced_delta_x = -wheel_event.deltaX;
  float event_coalesced_delta_y = -wheel_event.deltaY;

  // Reset overflow values because we may decide to remove delta at various
  // points and put it into overflow.
  overflow_scroll_delta_ = gfx::Vector2dF();

  gfx::Vector2dF stretch_amount = client_->StretchAmount();
  bool is_vertically_stretched = stretch_amount.y() != 0.f;
  bool is_horizontally_stretched = stretch_amount.x() != 0.f;

  // Slightly prefer scrolling vertically by applying the = case to delta_y
  if (fabsf(delta_y) >= fabsf(delta_x))
    delta_x = 0;
  else
    delta_y = 0;

  bool should_stretch = false;

  blink::WebMouseWheelEvent::Phase momentum_phase = wheel_event.momentumPhase;

  // If we are starting momentum scrolling then do some setup.
  if (!momentum_scroll_in_progress_ &&
      (momentum_phase == blink::WebMouseWheelEvent::PhaseBegan ||
       momentum_phase == blink::WebMouseWheelEvent::PhaseChanged)) {
    momentum_scroll_in_progress_ = true;
    // Start the snap rubber band timer if it's not running. This is needed to
    // snap back from the over scroll caused by momentum events.
    if (!snap_rubberband_timer_is_active_ && start_time_ == base::Time())
      SnapRubberband();
  }

  double time_delta =
      wheel_event.timeStampSeconds - last_momentum_scroll_timestamp_;
  if (in_scroll_gesture_ || momentum_scroll_in_progress_) {
    if (last_momentum_scroll_timestamp_ && time_delta > 0 &&
        time_delta < kScrollVelocityZeroingTimeout) {
      momentum_velocity_.set_x(event_coalesced_delta_x / (float)time_delta);
      momentum_velocity_.set_y(event_coalesced_delta_y / (float)time_delta);
      last_momentum_scroll_timestamp_ = wheel_event.timeStampSeconds;
    } else {
      last_momentum_scroll_timestamp_ = wheel_event.timeStampSeconds;
      momentum_velocity_ = gfx::Vector2dF();
    }

    if (is_vertically_stretched) {
      if (!is_horizontally_stretched &&
          client_->PinnedInDirection(gfx::Vector2dF(delta_x, 0))) {
        // Stretching only in the vertical.
        if (delta_y != 0 &&
            (fabsf(delta_x / delta_y) < kRubberbandDirectionLockStretchRatio))
          delta_x = 0;
        else if (fabsf(delta_x) <
                 kRubberbandMinimumRequiredDeltaBeforeStretch) {
          overflow_scroll_delta_.set_x(overflow_scroll_delta_.x() + delta_x);
          delta_x = 0;
        } else
          overflow_scroll_delta_.set_x(overflow_scroll_delta_.x() + delta_x);
      }
    } else if (is_horizontally_stretched) {
      // Stretching only in the horizontal.
      if (client_->PinnedInDirection(gfx::Vector2dF(0, delta_y))) {
        if (delta_x != 0 &&
            (fabsf(delta_y / delta_x) < kRubberbandDirectionLockStretchRatio))
          delta_y = 0;
        else if (fabsf(delta_y) <
                 kRubberbandMinimumRequiredDeltaBeforeStretch) {
          overflow_scroll_delta_.set_y(overflow_scroll_delta_.y() + delta_y);
          delta_y = 0;
        } else
          overflow_scroll_delta_.set_y(overflow_scroll_delta_.y() + delta_y);
      }
    } else {
      // Not stretching at all yet.
      if (client_->PinnedInDirection(gfx::Vector2dF(delta_x, delta_y))) {
        if (fabsf(delta_y) >= fabsf(delta_x)) {
          if (fabsf(delta_x) < kRubberbandMinimumRequiredDeltaBeforeStretch) {
            overflow_scroll_delta_.set_x(overflow_scroll_delta_.x() + delta_x);
            delta_x = 0;
          } else
            overflow_scroll_delta_.set_x(overflow_scroll_delta_.x() + delta_x);
        }
        should_stretch = true;
      }
    }
  }

  if (delta_x != 0 || delta_y != 0) {
    has_scrolled_ = true;
    if (!(should_stretch || is_vertically_stretched ||
          is_horizontally_stretched)) {
      if (delta_y != 0) {
        delta_y *= ScrollWheelMultiplier();
        client_->ImmediateScrollBy(gfx::Vector2dF(0, delta_y));
      }
      if (delta_x != 0) {
        delta_x *= ScrollWheelMultiplier();
        client_->ImmediateScrollBy(gfx::Vector2dF(delta_x, 0));
      }
    } else {
      if (!client_->AllowsHorizontalStretching()) {
        delta_x = 0;
        event_coalesced_delta_x = 0;
      } else if ((delta_x != 0) && !is_horizontally_stretched &&
                 !client_->PinnedInDirection(gfx::Vector2dF(delta_x, 0))) {
        delta_x *= ScrollWheelMultiplier();

        client_->ImmediateScrollByWithoutContentEdgeConstraints(
            gfx::Vector2dF(delta_x, 0));
        delta_x = 0;
      }

      if (!client_->AllowsVerticalStretching()) {
        delta_y = 0;
        event_coalesced_delta_y = 0;
      } else if ((delta_y != 0) && !is_vertically_stretched &&
                 !client_->PinnedInDirection(gfx::Vector2dF(0, delta_y))) {
        delta_y *= ScrollWheelMultiplier();

        client_->ImmediateScrollByWithoutContentEdgeConstraints(
            gfx::Vector2dF(0, delta_y));
        delta_y = 0;
      }

      gfx::Vector2dF stretch_amount = client_->StretchAmount();

      if (momentum_scroll_in_progress_) {
        if ((client_->PinnedInDirection(gfx::Vector2dF(
                 event_coalesced_delta_x, event_coalesced_delta_y)) ||
             (fabsf(event_coalesced_delta_x) + fabsf(event_coalesced_delta_y) <=
              0)) &&
            last_momentum_scroll_timestamp_) {
          ignore_momentum_scrolls_ = true;
          momentum_scroll_in_progress_ = false;
          SnapRubberband();
        }
      }

      stretch_scroll_force_.set_x(stretch_scroll_force_.x() + delta_x);
      stretch_scroll_force_.set_y(stretch_scroll_force_.y() + delta_y);

      gfx::Vector2dF damped_delta(
          ceilf(ElasticDeltaForReboundDelta(stretch_scroll_force_.x())),
          ceilf(ElasticDeltaForReboundDelta(stretch_scroll_force_.y())));

      client_->ImmediateScrollByWithoutContentEdgeConstraints(damped_delta -
                                                              stretch_amount);
    }
  }

  if (momentum_scroll_in_progress_ &&
      momentum_phase == blink::WebMouseWheelEvent::PhaseEnded) {
    momentum_scroll_in_progress_ = false;
    ignore_momentum_scrolls_ = false;
    last_momentum_scroll_timestamp_ = 0;
  }

  return true;
}

namespace {

float RoundTowardZero(float num) {
  return num > 0 ? ceilf(num - 0.5f) : floorf(num + 0.5f);
}

float RoundToDevicePixelTowardZero(float num) {
  float rounded_num = roundf(num);
  if (fabs(num - rounded_num) < 0.125)
    num = rounded_num;

  return RoundTowardZero(num);
}

}  // namespace

void ScrollElasticityController::SnapRubberbandTimerFired() {
  if (!momentum_scroll_in_progress_ || ignore_momentum_scrolls_) {
    float time_delta = (base::Time::Now() - start_time_).InSecondsF();

    if (start_stretch_ == gfx::Vector2dF()) {
      start_stretch_ = client_->StretchAmount();
      if (start_stretch_ == gfx::Vector2dF()) {
        StopSnapRubberbandTimer();

        stretch_scroll_force_ = gfx::Vector2dF();
        start_time_ = base::Time();
        orig_origin_ = gfx::Vector2dF();
        orig_velocity_ = gfx::Vector2dF();
        return;
      }

      orig_origin_ = client_->AbsoluteScrollPosition() - start_stretch_;
      orig_velocity_ = momentum_velocity_;

      // Just like normal scrolling, prefer vertical rubberbanding
      if (fabsf(orig_velocity_.y()) >= fabsf(orig_velocity_.x()))
        orig_velocity_.set_x(0);

      // Don't rubber-band horizontally if it's not possible to scroll
      // horizontally
      if (!client_->CanScrollHorizontally())
        orig_velocity_.set_x(0);

      // Don't rubber-band vertically if it's not possible to scroll
      // vertically
      if (!client_->CanScrollVertically())
        orig_velocity_.set_y(0);
    }

    gfx::Vector2dF delta(
        RoundToDevicePixelTowardZero(ElasticDeltaForTimeDelta(
            start_stretch_.x(), -orig_velocity_.x(), time_delta)),
        RoundToDevicePixelTowardZero(ElasticDeltaForTimeDelta(
            start_stretch_.y(), -orig_velocity_.y(), time_delta)));

    if (fabs(delta.x()) >= 1 || fabs(delta.y()) >= 1) {
      client_->ImmediateScrollByWithoutContentEdgeConstraints(
          gfx::Vector2dF(delta.x(), delta.y()) - client_->StretchAmount());

      gfx::Vector2dF new_stretch = client_->StretchAmount();

      stretch_scroll_force_.set_x(ReboundDeltaForElasticDelta(new_stretch.x()));
      stretch_scroll_force_.set_y(ReboundDeltaForElasticDelta(new_stretch.y()));
    } else {
      client_->AdjustScrollPositionToBoundsIfNecessary();

      StopSnapRubberbandTimer();
      stretch_scroll_force_ = gfx::Vector2dF();
      start_time_ = base::Time();
      start_stretch_ = gfx::Vector2dF();
      orig_origin_ = gfx::Vector2dF();
      orig_velocity_ = gfx::Vector2dF();
    }
  } else {
    start_time_ = base::Time::Now();
    start_stretch_ = gfx::Vector2dF();
  }
}

bool ScrollElasticityController::IsRubberbandInProgress() const {
  if (!in_scroll_gesture_ && !momentum_scroll_in_progress_ &&
      !snap_rubberband_timer_is_active_)
    return false;

  return !client_->StretchAmount().IsZero();
}

void ScrollElasticityController::StopSnapRubberbandTimer() {
  client_->StopSnapRubberbandTimer();
  snap_rubberband_timer_is_active_ = false;
}

void ScrollElasticityController::SnapRubberband() {
  double time_delta = SystemUptime() - last_momentum_scroll_timestamp_;
  if (last_momentum_scroll_timestamp_ &&
      time_delta >= kScrollVelocityZeroingTimeout)
    momentum_velocity_ = gfx::Vector2dF();

  in_scroll_gesture_ = false;

  if (snap_rubberband_timer_is_active_)
    return;

  start_stretch_ = gfx::Vector2dF();
  orig_origin_ = gfx::Vector2dF();
  orig_velocity_ = gfx::Vector2dF();

  // If there's no momentum scroll or stretch amount, no need to start the
  // timer.
  if (!momentum_scroll_in_progress_ &&
      client_->StretchAmount() == gfx::Vector2dF()) {
    start_time_ = base::Time();
    stretch_scroll_force_ = gfx::Vector2dF();
    return;
  }

  start_time_ = base::Time::Now();
  client_->StartSnapRubberbandTimer();
  snap_rubberband_timer_is_active_ = true;
}

bool ScrollElasticityController::ShouldHandleEvent(
    const blink::WebMouseWheelEvent& wheel_event) {
  // Once any scrolling has happened, all future events should be handled.
  if (has_scrolled_)
    return true;

  // The event can't cause scrolling to start if its delta is 0.
  if (wheel_event.deltaX == 0 && wheel_event.deltaY == 0)
    return false;

  // If the client isn't pinned, then the event is guaranteed to cause
  // scrolling.
  if (!client_->PinnedInDirection(gfx::Vector2dF(-wheel_event.deltaX, 0)))
    return true;

  // If the event is pinned, then the client can't scroll, but it might rubber
  // band.
  // Check if the event allows rubber banding.
  if (wheel_event.deltaY == 0) {
    if (wheel_event.deltaX > 0 && !wheel_event.canRubberbandLeft)
      return false;
    if (wheel_event.deltaX < 0 && !wheel_event.canRubberbandRight)
      return false;
  }

  // The event is going to either cause scrolling or rubber banding.
  return true;
}

}  // namespace content
