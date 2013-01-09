// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/tap_suppression_controller.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/common/content_switches.h"
#include "ui/base/gestures/gesture_configuration.h"

namespace content {

TapSuppressionController::TapSuppressionController(RenderWidgetHostImpl* rwhv)
     : render_widget_host_(rwhv),
       state_(TapSuppressionController::NOTHING) {
}

TapSuppressionController::~TapSuppressionController() { }

bool TapSuppressionController::ShouldSuppressMouseUp() {
  switch (state_) {
    case NOTHING:
    case GFC_IN_PROGRESS:
      return false;
    case MD_STASHED:
      state_ = NOTHING;
      mouse_down_timer_.Stop();
      return true;
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Invalid MouseUp on LAST_CANCEL_STOPPED_FLING state";
  }
  return false;
}

bool TapSuppressionController::ShouldDeferMouseDown(
    const WebKit::WebMouseEvent& event) {
  switch (state_) {
    case NOTHING:
      return false;
    case GFC_IN_PROGRESS:
      mouse_down_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(
              ui::GestureConfiguration::fling_max_tap_gap_time_in_ms()),
          this,
          &TapSuppressionController::MouseDownTimerExpired);
      stashed_mouse_down_ = event;
      state_ = MD_STASHED;
      return true;
    case MD_STASHED:
      NOTREACHED() << "MouseDown on MD_STASHED state";
      state_ = NOTHING;
      return false;
    case LAST_CANCEL_STOPPED_FLING:
      if ((base::TimeTicks::Now() - fling_cancel_time_).InMilliseconds()
          < ui::GestureConfiguration::fling_max_cancel_to_down_time_in_ms()) {
        state_ = MD_STASHED;
        mouse_down_timer_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                ui::GestureConfiguration::fling_max_tap_gap_time_in_ms()),
            this,
            &TapSuppressionController::MouseDownTimerExpired);
        stashed_mouse_down_ = event;
        return true;
      } else {
        state_ = NOTHING;
        return false;
      }
  }
  return false;
}

void TapSuppressionController::GestureFlingCancelAck(bool processed) {
  switch (state_) {
    case NOTHING:
      NOTREACHED() << "GFC_Ack without a GFC";
      break;
    case GFC_IN_PROGRESS:
      if (processed)
        fling_cancel_time_ = base::TimeTicks::Now();
      state_ = LAST_CANCEL_STOPPED_FLING;
      break;
    case MD_STASHED:
      if (!processed) {
        TRACE_EVENT0("browser",
                     "TapSuppressionController::GestureFlingCancelAck");
        mouse_down_timer_.Stop();
        render_widget_host_->ForwardMouseEventImmediately(stashed_mouse_down_);
        state_ = NOTHING;
      } // Else waiting for the timer to release the mouse event.
      break;
    case LAST_CANCEL_STOPPED_FLING:
      break;
  }
}

void TapSuppressionController::GestureFlingCancel(double cancel_time) {
  switch (state_) {
    case NOTHING:
    case GFC_IN_PROGRESS:
    case LAST_CANCEL_STOPPED_FLING:
      state_ = GFC_IN_PROGRESS;
      break;
    case MD_STASHED:
      break;
  }
}

void TapSuppressionController::MouseDownTimerExpired() {
  switch (state_) {
    case NOTHING:
    case GFC_IN_PROGRESS:
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Timer fired on invalid state.";
      state_ = NOTHING;
      break;
    case MD_STASHED:
      TRACE_EVENT0("browser",
                   "TapSuppressionController::MouseDownTimerExpired");
      render_widget_host_->ForwardMouseEventImmediately(stashed_mouse_down_);
      state_ = NOTHING;
      break;
  }
}

} // namespace content
