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

void TapSuppressionController::GestureFlingCancelAck(bool processed) {
  base::TimeTicks event_time = Now();
  switch (state_) {
    case NOTHING:
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
        StopTapDownTimer();
        client_->ForwardStashedTapDownForDeferral();
        state_ = NOTHING;
      }  // Else waiting for the timer to release the stashed tap down.
      break;
    case LAST_CANCEL_STOPPED_FLING:
      break;
  }
}

bool TapSuppressionController::ShouldDeferTapDown() {
  base::TimeTicks event_time = Now();
  switch (state_) {
    case NOTHING:
      return false;
    case GFC_IN_PROGRESS:
      state_ = TAP_DOWN_STASHED;
      StartTapDownTimer(
          base::TimeDelta::FromMilliseconds(client_->MaxTapGapTimeInMs()));
      return true;
    case TAP_DOWN_STASHED:
      NOTREACHED() << "TapDown on TAP_DOWN_STASHED state";
      state_ = NOTHING;
      return false;
    case LAST_CANCEL_STOPPED_FLING:
      if ((event_time - fling_cancel_time_).InMilliseconds()
          < client_->MaxCancelToDownTimeInMs()) {
        state_ = TAP_DOWN_STASHED;
        StartTapDownTimer(
            base::TimeDelta::FromMilliseconds(client_->MaxTapGapTimeInMs()));
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
      StopTapDownTimer();
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
      StopTapDownTimer();
      client_->DropStashedTapDown();
      return true;
    case LAST_CANCEL_STOPPED_FLING:
      NOTREACHED() << "Invalid TapCancel on LAST_CANCEL_STOPPED_FLING state";
  }
  return false;
}

base::TimeTicks TapSuppressionController::Now() {
  return base::TimeTicks::Now();
}

void TapSuppressionController::StartTapDownTimer(const base::TimeDelta& delay) {
  tap_down_timer_.Start(FROM_HERE, delay, this,
                        &TapSuppressionController::TapDownTimerExpired);
}

void TapSuppressionController::StopTapDownTimer() {
  tap_down_timer_.Stop();
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
