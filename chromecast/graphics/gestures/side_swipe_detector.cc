// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/side_swipe_detector.h"

#include <deque>

#include "base/auto_reset.h"
#include "chromecast/base/chromecast_switches.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromecast {
namespace {
// The number of pixels from the very left or right of the screen to consider as
// a valid origin for the left or right swipe gesture.
constexpr int kDefaultSideGestureStartWidth = 35;

// The number of pixels from the very top or bottom of the screen to consider as
// a valid origin for the top or bottom swipe gesture.
constexpr int kDefaultSideGestureStartHeight = 35;

// The amount of time after gesture start to allow events which occur in the
// margin to be stashed and replayed within. For example a tap event which
// occurs inside the gesture margin will be valid as long as it occurs within
// the time specified by this threshold.
constexpr base::TimeDelta kGestureMarginEventsTimeLimit =
    base::TimeDelta::FromMilliseconds(500);

// Get the correct bottom gesture start height by checking both margin flags in
// order, and then the default value if neither is set.
int BottomGestureStartHeight() {
  return GetSwitchValueInt(
      switches::kBottomSystemGestureStartHeight,
      GetSwitchValueInt(switches::kSystemGestureStartHeight,
                        kDefaultSideGestureStartHeight));
}

}  // namespace

SideSwipeDetector::SideSwipeDetector(CastGestureHandler* gesture_handler,
                                     aura::Window* root_window)
    : gesture_start_width_(GetSwitchValueInt(switches::kSystemGestureStartWidth,
                                             kDefaultSideGestureStartWidth)),
      gesture_start_height_(
          GetSwitchValueInt(switches::kSystemGestureStartHeight,
                            kDefaultSideGestureStartHeight)),
      bottom_gesture_start_height_(BottomGestureStartHeight()),
      gesture_handler_(gesture_handler),
      root_window_(root_window),
      current_swipe_origin_(CastSideSwipeOrigin::NONE),
      current_pointer_id_(ui::PointerDetails::kUnknownPointerId),
      corner_hold_timer_(new base::OneShotTimer),
      in_corner_hold_(false),
      current_corner_origin_(CastGestureHandler::NO_CORNERS) {
  DCHECK(gesture_handler);
  DCHECK(root_window);
  root_window_->GetHost()->GetEventSource()->AddEventRewriter(this);
}

SideSwipeDetector::~SideSwipeDetector() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

bool SideSwipeDetector::GetMarginPosition(
    const gfx::Point& point,
    const gfx::Rect& screen_bounds,
    CastSideSwipeOrigin* side,
    CastGestureHandler::Corner* corner) const {
  DCHECK(corner);
  DCHECK(side);
  *corner = CastGestureHandler::NO_CORNERS;
  *side = CastSideSwipeOrigin::NONE;

  const int top_margin_limit = screen_bounds.y() + gesture_start_height_;
  const int left_margin_limit = screen_bounds.x() + gesture_start_width_;
  const int right_margin_limit =
      screen_bounds.x() + screen_bounds.width() - gesture_start_width_;
  const int bottom_margin_limit =
      screen_bounds.y() + screen_bounds.height() - bottom_gesture_start_height_;

  // Find out if anybody is interested in corner events (hold or swipe). If not,
  // we won't bother trying to check them, so that we can be sure that swipe
  // events that happen in corners are treated as a side.
  CastGestureHandler::Corner corners = gesture_handler_->HandledCornerHolds();

  // Look for corners first if needed.
  if (corners != CastGestureHandler::NO_CORNERS) {
    if (corners && point.y() < top_margin_limit &&
        point.x() < left_margin_limit) {
      *corner = CastGestureHandler::TOP_LEFT_CORNER;
      return true;
    }
    if (point.y() < top_margin_limit && point.x() > right_margin_limit) {
      *corner = CastGestureHandler::TOP_RIGHT_CORNER;
      return true;
    }
    if (point.x() < left_margin_limit && point.y() > bottom_margin_limit) {
      *corner = CastGestureHandler::BOTTOM_LEFT_CORNER;
      return true;
    }
    if (point.x() > right_margin_limit && point.y() > bottom_margin_limit) {
      *corner = CastGestureHandler::BOTTOM_RIGHT_CORNER;
      return true;
    }
  }

  // Then sides.
  if (point.y() < top_margin_limit) {
    *side = CastSideSwipeOrigin::TOP;
    return true;
  }
  if (point.x() < left_margin_limit) {
    *side = CastSideSwipeOrigin::LEFT;
    return true;
  }
  if (point.x() > right_margin_limit) {
    *side = CastSideSwipeOrigin::RIGHT;
    return true;
  }
  if (point.y() > bottom_margin_limit) {
    *side = CastSideSwipeOrigin::BOTTOM;
    return true;
  }
  *side = CastSideSwipeOrigin::NONE;
  return false;
}

void SideSwipeDetector::StashEvent(const ui::TouchEvent& event) {
  // If the time since the gesture start is longer than our threshold, do not
  // stash the event (and clear the stashed events).
  if (gesture_elapsed_timer_.Elapsed() > kGestureMarginEventsTimeLimit) {
    stashed_events_.clear();
    return;
  }

  stashed_events_.push_back(event);
}

ui::EventRewriteStatus SideSwipeDetector::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* new_event) {
  if (!event.IsTouchEvent()) {
    return ui::EVENT_REWRITE_CONTINUE;
  }
  const ui::TouchEvent* touch_event = event.AsTouchEvent();

  // Touch events come through in screen pixels, but untransformed. This is the
  // raw coordinate not yet mapped to the root window's coordinate system or the
  // screen. Convert it into the root window's coordinate system, in DIP which
  // is what the rest of this class expects.
  gfx::Point touch_location = touch_event->root_location();
  root_window_->GetHost()->ConvertPixelsToDIP(&touch_location);
  gfx::Rect screen_bounds = display::Screen::GetScreen()
                                ->GetDisplayNearestPoint(touch_location)
                                .bounds();

  CastSideSwipeOrigin detected_side;
  CastGestureHandler::Corner detected_corner;
  bool is_margin_event = GetMarginPosition(touch_location, screen_bounds,
                                           &detected_side, &detected_corner);

  // Gesture initiation is a press event inside a margin for a new pointer id.
  if (touch_event->type() == ui::ET_TOUCH_PRESSED) {
    if (!is_margin_event) {
      return ui::EVENT_REWRITE_CONTINUE;
    }

    // We're entering the screen, so notify the dispatcher of that before
    // looking for a swipe.
    gesture_handler_->HandleScreenEnter(detected_side, touch_event->location());

    // There's already a finger down, so don't process any swipes or corners
    // with the new one.
    if (current_pointer_id_ != ui::PointerDetails::kUnknownPointerId) {
      return ui::EVENT_REWRITE_CONTINUE;
    }

    // Remember the finger.
    current_pointer_id_ = touch_event->pointer_details().id;

    // If we're in a corner (which means we're looking for that kind of thing),
    // start the corner hold timer and move on.
    in_corner_hold_ = false;
    current_corner_origin_ = CastGestureHandler::NO_CORNERS;
    if (detected_corner != CastGestureHandler::NO_CORNERS) {
      current_corner_origin_ = detected_corner;
      corner_hold_start_event_ = ui::Event::Clone(*touch_event);
      corner_hold_timer_->Start(
          FROM_HERE, gesture_detector_config_.longpress_timeout, this,
          &SideSwipeDetector::OnCornerHoldTimerFired);
    } else if (detected_side != CastSideSwipeOrigin::NONE &&
               gesture_handler_->CanHandleSwipe(detected_side)) {
      current_swipe_origin_ = detected_side;

      // Let the subscribers know about the gesture begin.
      gesture_handler_->HandleSideSwipeBegin(detected_side, touch_location);

      VLOG(1) << "side swipe gesture begin @ " << touch_location.ToString();
    } else {
      return ui::EVENT_REWRITE_CONTINUE;
    }
    gesture_elapsed_timer_ = base::ElapsedTimer();

    // Stash a copy of the event should we decide to reconstitute it later if
    // we decide that this isn't in fact a side swipe or corner hold.
    StashEvent(*touch_event);

    // Avoid corrupt gesture state caused by a missing kGestureScrollEnd event
    // as we potentially transition between web views.
    root_window_->CleanupGestureState();

    return ui::EVENT_REWRITE_DISCARD;
  }

  // If we're releasing on our way out of the screen, let the world know.
  if (touch_event->type() == ui::ET_TOUCH_RELEASED &&
      detected_side != CastSideSwipeOrigin::NONE) {
    gesture_handler_->HandleScreenExit(detected_side, touch_event->location());
  }

  // If we're not in a gesture of any kind, or the the non-press events we're
  // getting are not for the same pointer id, we're not interested in them.
  if ((current_corner_origin_ == CastGestureHandler::NO_CORNERS &&
       current_swipe_origin_ == CastSideSwipeOrigin::NONE) ||
      touch_event->pointer_details().id != current_pointer_id_) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  // While we're in a swipe, stash events for later playback.
  if (in_corner_hold_ || current_swipe_origin_ != CastSideSwipeOrigin::NONE) {
    StashEvent(*touch_event);
  }

  if (touch_event->type() == ui::ET_TOUCH_RELEASED) {
    current_pointer_id_ = ui::PointerDetails::kUnknownPointerId;

    // If a corner event was in progress, we will need to either finish a hold
    // or cancel it.
    if (current_corner_origin_ != CastGestureHandler::NO_CORNERS) {
      // If the corner was being held, notify consumers that it's over,
      // clear events, and discard.
      if (in_corner_hold_) {
        EndCornerHold(*touch_event);

        stashed_events_.clear();
        return ui::EVENT_REWRITE_DISCARD;
      }

      // Otherwise cancel and replay held events.
      CancelCornerHoldCheck();

      return StartNextDispatchStashedEvents(new_event);
    }

    // If there was no side swipe in progress, we can just move on.
    if (current_swipe_origin_ == CastSideSwipeOrigin::NONE) {
      return ui::EVENT_REWRITE_CONTINUE;
    }

    VLOG(1) << "gesture swipe release; time since press: "
            << gesture_elapsed_timer_.Elapsed().InMilliseconds() << "ms @ "
            << touch_location.ToString();
    gesture_handler_->HandleSideSwipeEnd(current_swipe_origin_, touch_location);
    current_swipe_origin_ = CastSideSwipeOrigin::NONE;

    // If the finger is still inside the touch margin at release, this is not
    // really a side swipe. Start streaming out events we stashed for later
    // retrieval.
    if (detected_side != CastSideSwipeOrigin::NONE) {
      return StartNextDispatchStashedEvents(new_event);
    }

    // Otherwise, clear them.
    stashed_events_.clear();

    return ui::EVENT_REWRITE_DISCARD;
  }

  // If we're looking for corner hold and the timer has not expired and we've
  // left the corner, cancel the hold check now and replay the stashed events.
  if (current_corner_origin_ != CastGestureHandler::NO_CORNERS &&
      detected_corner != current_corner_origin_ &&
      corner_hold_timer_->IsRunning()) {
    CancelCornerHoldCheck();

    return StartNextDispatchStashedEvents(new_event);
  }

  // If a swipe is ongoing, let consumers know about it.
  if (current_swipe_origin_ != CastSideSwipeOrigin::NONE) {
    gesture_handler_->HandleSideSwipeContinue(current_swipe_origin_,
                                              touch_location);
    VLOG(1) << "gesture continue; time since press: "
            << gesture_elapsed_timer_.Elapsed().InMilliseconds() << "ms @ "
            << touch_location.ToString();
  }

  // If we're in corner hold and we've left the corner, that's an end.
  if (in_corner_hold_ && detected_corner != current_corner_origin_) {
    EndCornerHold(*touch_event);

    stashed_events_.clear();
  }

  return ui::EVENT_REWRITE_DISCARD;
}

void SideSwipeDetector::CancelCornerHoldCheck() {
  corner_hold_timer_->Stop();
  corner_hold_start_event_.reset();
  in_corner_hold_ = false;
  current_corner_origin_ = CastGestureHandler::NO_CORNERS;
}

void SideSwipeDetector::EndCornerHold(const ui::TouchEvent& event) {
  gesture_handler_->HandleCornerHoldEnd(current_corner_origin_, event);
  in_corner_hold_ = false;
  current_corner_origin_ = CastGestureHandler::NO_CORNERS;
  corner_hold_start_event_.reset();
}

ui::EventRewriteStatus SideSwipeDetector::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  if (stashed_events_.empty()) {
    return ui::EVENT_REWRITE_DISCARD;
  }

  *new_event = ui::Event::Clone(stashed_events_.front());
  stashed_events_.pop_front();

  return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
}

void SideSwipeDetector::OnCornerHoldTimerFired() {
  DCHECK(corner_hold_start_event_);
  DCHECK(current_corner_origin_ != CastGestureHandler::NO_CORNERS);
  DCHECK(!in_corner_hold_);
  in_corner_hold_ = true;
  gesture_handler_->HandleCornerHold(current_corner_origin_,
                                     *corner_hold_start_event_->AsTouchEvent());
  corner_hold_start_event_.reset();
}

void SideSwipeDetector::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  corner_hold_timer_ = std::move(timer);
}

ui::EventRewriteStatus SideSwipeDetector::StartNextDispatchStashedEvents(
    std::unique_ptr<ui::Event>* new_event) {
  if (!stashed_events_.empty()) {
    *new_event = ui::Event::Clone(stashed_events_.front());
    stashed_events_.pop_front();
    return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
  }
  stashed_events_.clear();
  return ui::EVENT_REWRITE_DISCARD;
}

}  // namespace chromecast
