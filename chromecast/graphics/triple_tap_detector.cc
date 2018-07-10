// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/triple_tap_detector.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"

namespace chromecast {

TripleTapDetector::TripleTapDetector(aura::Window* root_window,
                                     TripleTapDetectorDelegate* delegate)
    : root_window_(root_window),
      delegate_(delegate),
      enabled_(false),
      triple_tap_state_(TripleTapState::NONE),
      tap_count_(0) {
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
}

TripleTapDetector::~TripleTapDetector() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

ui::EventRewriteStatus TripleTapDetector::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (!enabled_ || !delegate_ || !event.IsTouchEvent()) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  const ui::TouchEvent& touch_event = static_cast<const ui::TouchEvent&>(event);
  return HandleTripleTapState(touch_event, triple_tap_state_);
}

ui::EventRewriteStatus TripleTapDetector::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  // Only called if the rewriter has returned EVENT_REWRITE_DISPATCH_ANOTHER,
  // and we don't do that.
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TripleTapDetector::HandleTripleTapState(
    const ui::TouchEvent& event,
    TripleTapState state) {
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    // If a press happened again before the minimum inter-tap interval, cancel
    // the triple tap.
    if (state == TripleTapState::INTERVAL_WAIT &&
        (event.time_stamp() - stashed_events_.back().time_stamp()) <
            gesture_detector_config_.double_tap_min_time) {
      TripleTapReset();
      return ui::EVENT_REWRITE_DISCARD;
    }

    // Otherwise transition into a touched state.
    triple_tap_state_ = TripleTapState::TOUCH;
    last_tap_location_ = event.location();

    // If this is pressed too long, it should be treated as a long-press, and
    // not part of a triple-tap, so set a timer to detect that.
    triple_tap_timer_.Start(FROM_HERE,
                            gesture_detector_config_.longpress_timeout, this,
                            &TripleTapDetector::OnLongPressIntervalTimerFired);

    // Copy the event and then kill the original touch pressed event, we'll
    // produce a new one when the timer expires if it turns out this wasn't a
    // triple-tap.
    stashed_events_.push_back(event);

    return ui::EVENT_REWRITE_DISCARD;
  }

  // Finger was released while we were waiting for one, count it as a tap.
  if (event.type() == ui::ET_TOUCH_RELEASED && state == TripleTapState::TOUCH) {
    triple_tap_state_ = TripleTapState::INTERVAL_WAIT;
    triple_tap_timer_.Start(FROM_HERE,
                            gesture_detector_config_.double_tap_timeout, this,
                            &TripleTapDetector::OnTripleTapIntervalTimerFired);
    stashed_events_.push_back(event);
    tap_count_++;
    if (tap_count_ == 3) {
      stashed_events_.clear();
      TripleTapReset();
      delegate_->OnTripleTap(event.location());
    }
    return ui::EVENT_REWRITE_DISCARD;
  }

  if (state != TripleTapState::NONE) {
    stashed_events_.push_back(event);
    return ui::EVENT_REWRITE_DISCARD;
  }

  return ui::EVENT_REWRITE_CONTINUE;
}

void TripleTapDetector::OnTripleTapIntervalTimerFired() {
  TripleTapReset();
}

void TripleTapDetector::OnLongPressIntervalTimerFired() {
  TripleTapReset();
}

void TripleTapDetector::TripleTapReset() {
  triple_tap_state_ = TripleTapState::NONE;
  tap_count_ = 0;
  triple_tap_timer_.Stop();
  for (auto& event : stashed_events_) {
    DispatchEvent(&event);
  }
  stashed_events_.clear();
}

void TripleTapDetector::DispatchEvent(ui::TouchEvent* event) {
  // Turn off triple-tap before re-dispatching to avoid infinite recursion into
  // this detector.
  base::AutoReset<bool> toggle_enable(&enabled_, false);
  DCHECK(!root_window_->GetHost()
              ->dispatcher()
              ->OnEventFromSource(event)
              .dispatcher_destroyed);
}

}  // namespace chromecast
