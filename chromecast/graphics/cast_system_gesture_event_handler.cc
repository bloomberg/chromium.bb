// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_system_gesture_event_handler.h"

#include <deque>

#include "base/auto_reset.h"
#include "chromecast/base/chromecast_switches.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
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

// An event rewriter for detecting system-wide gestures performed on the screen.
// Recognizes swipe gestures that originate from the top, left, bottom, and
// right of the root window.  Stashes copies of touch events that occur during
// the side swipe, and replays them if the finger releases before leaving the
// margin area.
class SideSwipeDetector : public ui::EventRewriter {
 public:
  SideSwipeDetector(CastGestureHandler* gesture_handler,
                    aura::Window* root_window);

  ~SideSwipeDetector() override;

  CastSideSwipeOrigin GetDragPosition(const gfx::Point& point,
                                      const gfx::Rect& screen_bounds) const;

  // Overridden from ui::EventRewriter
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* rewritten_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

 private:
  void StashEvent(const ui::TouchEvent& event);

  const int gesture_start_width_;
  const int gesture_start_height_;
  const int bottom_gesture_start_height_;

  CastGestureHandler* gesture_handler_;
  aura::Window* root_window_;
  CastSideSwipeOrigin current_swipe_;
  base::ElapsedTimer current_swipe_time_;

  std::deque<ui::TouchEvent> stashed_events_;

  DISALLOW_COPY_AND_ASSIGN(SideSwipeDetector);
};

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
      current_swipe_(CastSideSwipeOrigin::NONE) {
  DCHECK(gesture_handler);
  DCHECK(root_window);
  root_window_->GetHost()->GetEventSource()->AddEventRewriter(this);
}

SideSwipeDetector::~SideSwipeDetector() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

CastSideSwipeOrigin SideSwipeDetector::GetDragPosition(
    const gfx::Point& point,
    const gfx::Rect& screen_bounds) const {
  if (point.y() < (screen_bounds.y() + gesture_start_height_)) {
    return CastSideSwipeOrigin::TOP;
  }
  if (point.x() < (screen_bounds.x() + gesture_start_width_)) {
    return CastSideSwipeOrigin::LEFT;
  }
  if (point.x() >
      (screen_bounds.x() + screen_bounds.width() - gesture_start_width_)) {
    return CastSideSwipeOrigin::RIGHT;
  }
  if (point.y() > (screen_bounds.y() + screen_bounds.height() -
                   bottom_gesture_start_height_)) {
    return CastSideSwipeOrigin::BOTTOM;
  }
  return CastSideSwipeOrigin::NONE;
}

void SideSwipeDetector::StashEvent(const ui::TouchEvent& event) {
  // If the time since the gesture start is longer than our threshold, do not
  // stash the event (and clear the stashed events).
  if (current_swipe_time_.Elapsed() > kGestureMarginEventsTimeLimit) {
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
  CastSideSwipeOrigin side_swipe_origin =
      GetDragPosition(touch_location, screen_bounds);

  // A located event has occurred inside the margin. It might be the start of
  // our gesture, or a touch that we need to squash.
  if (current_swipe_ == CastSideSwipeOrigin::NONE &&
      side_swipe_origin != CastSideSwipeOrigin::NONE) {
    // Check to see if we have any potential consumers of events on this side.
    // If not, we can continue on without consuming it.
    if (!gesture_handler_->CanHandleSwipe(side_swipe_origin)) {
      return ui::EVENT_REWRITE_CONTINUE;
    }

    // Detect the beginning of a system gesture swipe.
    if (touch_event->type() != ui::ET_TOUCH_PRESSED) {
      return ui::EVENT_REWRITE_CONTINUE;
    }

    current_swipe_ = side_swipe_origin;

    // Let the subscribers know about the gesture begin.
    gesture_handler_->HandleSideSwipeBegin(side_swipe_origin, touch_location);

    VLOG(1) << "side swipe gesture begin @ " << touch_location.ToString();
    current_swipe_time_ = base::ElapsedTimer();


    // Stash a copy of the event should we decide to reconstitute it later if we
    // decide that this isn't in fact a side swipe.
    StashEvent(*touch_event);

    // And then stop the original event from propagating.
    return ui::EVENT_REWRITE_DISCARD;
  }

  if (current_swipe_ == CastSideSwipeOrigin::NONE) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  // A swipe is in progress, or has completed, so stop propagation of underlying
  // gesture/touch events, after stashing a copy of the original event.
  StashEvent(*touch_event);

  // The finger has lifted, which means the end of the gesture, or if the finger
  // hasn't travelled far enough, replay the original events.
  if (touch_event->type() == ui::ET_TOUCH_RELEASED) {
    VLOG(1) << "gesture release; time since press: "
            << current_swipe_time_.Elapsed().InMilliseconds() << "ms @ "
            << touch_location.ToString();
    gesture_handler_->HandleSideSwipeEnd(current_swipe_, touch_location);
    current_swipe_ = CastSideSwipeOrigin::NONE;

    // If the finger is still inside the touch margin at release, this is not
    // really a side swipe. Start streaming out events we stashed for later
    // retrieval.
    if (side_swipe_origin != CastSideSwipeOrigin::NONE &&
        !stashed_events_.empty()) {
      *new_event = ui::Event::Clone(stashed_events_.front());
      stashed_events_.pop_front();
      return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
    }

    // Otherwise, clear them.
    stashed_events_.clear();

    return ui::EVENT_REWRITE_DISCARD;
  }

  // The system gesture is ongoing...
  gesture_handler_->HandleSideSwipeContinue(current_swipe_, touch_location);
  VLOG(1) << "gesture continue; time since press: "
          << current_swipe_time_.Elapsed().InMilliseconds() << "ms @ "
          << touch_location.ToString();

  return ui::EVENT_REWRITE_DISCARD;
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

CastSystemGestureEventHandler::CastSystemGestureEventHandler(
    aura::Window* root_window)
    : EventHandler(),
      root_window_(root_window),
      side_swipe_detector_(
          std::make_unique<SideSwipeDetector>(this, root_window)) {
  DCHECK(root_window);
  root_window->AddPreTargetHandler(this);
}

CastSystemGestureEventHandler::~CastSystemGestureEventHandler() {
  DCHECK(gesture_handlers_.empty());
  root_window_->RemovePreTargetHandler(this);
}

void CastSystemGestureEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressedEvent(event);
  }
}

void CastSystemGestureEventHandler::ProcessPressedEvent(
    ui::GestureEvent* event) {
  if (gesture_handlers_.empty()) {
    return;
  }
  gfx::Point touch_location(event->location());
  // Let the subscriber know about the gesture begin.
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN: {
      for (auto* gesture_handler : gesture_handlers_) {
        gesture_handler->HandleTapDownGesture(touch_location);
      }
      break;
    }
    case ui::ET_GESTURE_TAP: {
      for (auto* gesture_handler : gesture_handlers_) {
        gesture_handler->HandleTapGesture(touch_location);
      }
      break;
    }
    default: { return; }
  }
}

void CastSystemGestureEventHandler::AddGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.insert(handler);
}

void CastSystemGestureEventHandler::RemoveGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.erase(handler);
}

bool CastSystemGestureEventHandler::CanHandleSwipe(
    CastSideSwipeOrigin swipe_origin) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      return true;
    }
  }
  return false;
}

void CastSystemGestureEventHandler::HandleSideSwipeBegin(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      gesture_handler->HandleSideSwipeBegin(swipe_origin, touch_location);
    }
  }
}
void CastSystemGestureEventHandler::HandleSideSwipeContinue(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      gesture_handler->HandleSideSwipeContinue(swipe_origin, touch_location);
    }
  }
}
void CastSystemGestureEventHandler::HandleSideSwipeEnd(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      gesture_handler->HandleSideSwipeEnd(swipe_origin, touch_location);
    }
  }
}

}  // namespace chromecast
