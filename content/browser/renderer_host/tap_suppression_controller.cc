// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/tap_suppression_controller.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/browser/renderer_host/tap_suppression_controller_client.h"

namespace content {

TapSuppressionController::TapSuppressionController(
    TapSuppressionControllerClient* client)
    : client_(client),
      state_(TapSuppressionController::NOTHING) {
}

TapSuppressionController::~TapSuppressionController() {}

void TapSuppressionController::GestureFlingCancel() {
  switch (state_) {
    case NOTHING:
    case GFC_IN_PROGRESS:
    case LAST_CANCEL_STOPPED_FLING:
      state_ = GFC_IN_PROGRESS;
      break;
    case TAP_DOWN_STASHED:
      break;
  }
}

void TapSuppressionController::GestureFlingCancelAck(
    bool processed,
    const base::TimeTicks& event_time) {
  switch (state_) {
    case NOTHING:
      NOTREACHED() << "GFC_ACK without a GFC";
      break;
    case GFC_IN_PROGRESS:
      if (processed)
        fling_cancel_time_ = event_time;
      state_ = LAST_CANCEL_STOPPED_FLING;
      break;
    case TAP_DOWN_STASHED:
      if (!processed) {
        TRACE_EVENT0("browser",
                     "TapSuppressionController::GestureFlingCancelAck");
        tap_down_timer_.Stop();
        client_->ForwardStashedTapDownForDeferral();
        state_ = NOTHING;
      }  // Else waiting for the timer to release the stashed tap down.
      break;
    case LAST_CANCEL_STOPPED_FLING:
      break;
  }
}

bool TapSuppressionController::ShouldDeferTapDown(
    const base::TimeTicks& event_time) {
  switch (state_) {
    case NOTHING:
      return false;
    case GFC_IN_PROGRESS:
      state_ = TAP_DOWN_STASHED;
      StartTapDownTimer();
      return true;
    case TAP_DOWN_STASHED:
      NOTREACHED() << "TapDown on TAP_DOWN_STASHED state";
      state_ = NOTHING;
      return false;
    case LAST_CANCEL_STOPPED_FLING:
      if ((event_time - fling_cancel_time_).InMilliseconds()
          < client_->MaxCancelToDownTimeInMs()) {
        state_ = TAP_DOWN_STASHED;
        StartTapDownTimer();
        return true;
      } else {
        state_ = NOTHING;
        return false;
      }
  }
  NOTREACHED() << "Invalid state";
  return false;
}

bool TapSuppressionController::ShouldSuppressTapUp() {
  switch (state_) {
    case NOTHING:
    case GFC_IN_PROGRESS:
      return false;
    case TAP_DOWN_STASHED:
      state_ = NOTHING;
      tap_down_timer_.Stop();
      client_->DropStashedTapDown();
      return true;
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Invalid TapUp on LAST_CANCEL_STOPPED_FLING state";
  }
  return false;
}

bool TapSuppressionController::ShouldSuppressTapCancel() {
  switch (state_) {
    case NOTHING:
    case GFC_IN_PROGRESS:
      return false;
    case TAP_DOWN_STASHED:
      state_ = NOTHING;
      tap_down_timer_.Stop();
      client_->DropStashedTapDown();
      return true;
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Invalid TapCancel on LAST_CANCEL_STOPPED_FLING state";
  }
  return false;
}

void TapSuppressionController::StartTapDownTimer() {
  tap_down_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(client_->MaxTapGapTimeInMs()),
      this,
      &TapSuppressionController::TapDownTimerExpired);
}

void TapSuppressionController::TapDownTimerExpired() {
  switch (state_) {
    case NOTHING:
    case GFC_IN_PROGRESS:
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Timer fired on invalid state.";
      state_ = NOTHING;
      break;
    case TAP_DOWN_STASHED:
      TRACE_EVENT0("browser",
                   "TapSuppressionController::TapDownTimerExpired");
      client_->ForwardStashedTapDownSkipDeferral();
      state_ = NOTHING;
      break;
  }
}

}  // namespace content
