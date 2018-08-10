// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/triple_tap_detector.h"

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
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    // If a press happened again before the minimum inter-tap interval, cancel
    // the triple tap.
    if (triple_tap_state_ == TripleTapState::INTERVAL_WAIT &&
        (event.time_stamp() - stashed_events_.back().time_stamp()) <
            gesture_detector_config_.double_tap_min_time) {
      stashed_events_.clear();
      TripleTapReset();
      return ui::EVENT_REWRITE_CONTINUE;
    }

    // If the user moved too far from the last tap position, it's not a triple
    // tap.
    if (tap_count_) {
      float distance = (touch_event.location() - last_tap_location_).Length();
      if (distance > gesture_detector_config_.double_tap_slop) {
        TripleTapReset();
        stashed_events_.clear();
        return ui::EVENT_REWRITE_CONTINUE;
      }
    }

    // Otherwise transition into a touched state.
    triple_tap_state_ = TripleTapState::TOUCH;
    last_tap_location_ = touch_event.location();

    // If this is pressed too long, it should be treated as a long-press, and
    // not part of a triple-tap, so set a timer to detect that.
    triple_tap_timer_.Start(FROM_HERE,
                            gesture_detector_config_.longpress_timeout, this,
                            &TripleTapDetector::OnLongPressIntervalTimerFired);

    // If we've already gotten one tap, discard this event, only the original
    // tap needs to get through.
    if (tap_count_) {
      return ui::EVENT_REWRITE_DISCARD;
    }

    // Copy the event so we can issue a cancel for it later if this turns out to
    // be a triple-tap.
    stashed_events_.push_back(touch_event);

    return ui::EVENT_REWRITE_CONTINUE;
  }

  // Finger was released while we were waiting for one, count it as a tap.
  if (touch_event.type() == ui::ET_TOUCH_RELEASED &&
      triple_tap_state_ == TripleTapState::TOUCH) {
    triple_tap_state_ = TripleTapState::INTERVAL_WAIT;
    triple_tap_timer_.Start(FROM_HERE,
                            gesture_detector_config_.double_tap_timeout, this,
                            &TripleTapDetector::OnTripleTapIntervalTimerFired);

    tap_count_++;
    if (tap_count_ == 3) {
      TripleTapReset();
      delegate_->OnTripleTap(touch_event.location());

      // Start issuing cancel events for old presses.
      DCHECK(!stashed_events_.empty());
      const auto& stashed_press = stashed_events_.front();
      std::unique_ptr<ui::TouchEvent> rewritten_touch_event =
          std::make_unique<ui::TouchEvent>(ui::ET_TOUCH_CANCELLED, gfx::Point(),
                                           touch_event.time_stamp(),
                                           stashed_press.pointer_details());
      rewritten_touch_event->set_location_f(stashed_press.location_f());
      rewritten_touch_event->set_root_location_f(
          stashed_press.root_location_f());
      rewritten_touch_event->set_flags(stashed_press.flags());
      *rewritten_event = std::move(rewritten_touch_event);
      stashed_events_.pop_front();

      if (!stashed_events_.empty())
        return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
      else
        return ui::EVENT_REWRITE_REWRITTEN;
    } else if (tap_count_ > 1) {
      return ui::EVENT_REWRITE_DISCARD;
    }
  }

  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TripleTapDetector::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  const auto& stashed_press = stashed_events_.front();
  std::unique_ptr<ui::TouchEvent> rewritten_touch_event =
      std::make_unique<ui::TouchEvent>(ui::ET_TOUCH_CANCELLED, gfx::Point(),
                                       last_event.time_stamp(),
                                       stashed_press.pointer_details());
  rewritten_touch_event->set_location_f(stashed_press.location_f());
  rewritten_touch_event->set_root_location_f(stashed_press.root_location_f());
  rewritten_touch_event->set_flags(stashed_press.flags());
  *new_event = std::move(rewritten_touch_event);
  stashed_events_.pop_front();

  if (!stashed_events_.empty())
    return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
  else
    return ui::EVENT_REWRITE_DISCARD;
}

void TripleTapDetector::OnTripleTapIntervalTimerFired() {
  TripleTapReset();
  stashed_events_.clear();
}

void TripleTapDetector::OnLongPressIntervalTimerFired() {
  TripleTapReset();
  stashed_events_.clear();
}

void TripleTapDetector::TripleTapReset() {
  triple_tap_state_ = TripleTapState::NONE;
  tap_count_ = 0;
  triple_tap_timer_.Stop();
}

}  // namespace chromecast
