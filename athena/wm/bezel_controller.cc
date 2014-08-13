// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/bezel_controller.h"

#include "ui/aura/window.h"
#include "ui/events/event_handler.h"

namespace athena {
namespace {

// Using bezel swipes, the first touch that is registered is usually within
// 5-10 pixels from the edge, but sometimes as far as 29 pixels away.
// So setting this width fairly high for now.
const float kBezelWidth = 20.0f;

const float kScrollPositionNone = -100;

bool ShouldProcessGesture(ui::EventType event_type) {
  return event_type == ui::ET_GESTURE_SCROLL_UPDATE ||
         event_type == ui::ET_GESTURE_SCROLL_BEGIN ||
         event_type == ui::ET_GESTURE_BEGIN ||
         event_type == ui::ET_GESTURE_END;
}

}  // namespace

BezelController::BezelController(aura::Window* container)
    : container_(container),
      state_(NONE),
      scroll_bezel_(BEZEL_NONE),
      scroll_target_(NULL),
      left_right_delegate_(NULL) {
}

float BezelController::GetDistance(const gfx::PointF& position,
                                   BezelController::Bezel bezel) {
  DCHECK(bezel == BEZEL_LEFT || bezel == BEZEL_RIGHT);
  return bezel == BEZEL_LEFT
             ? position.x()
             : position.x() - container_->GetBoundsInScreen().width();
}

void BezelController::SetState(BezelController::State state,
                               const gfx::PointF& scroll_position) {
  if (!left_right_delegate_ || state == state_)
    return;

  if (state == BEZEL_SCROLLING_TWO_FINGERS) {
    float delta = GetDistance(scroll_position, scroll_bezel_);
    left_right_delegate_->ScrollBegin(scroll_bezel_, delta);
  } else if (state_ == BEZEL_SCROLLING_TWO_FINGERS) {
    left_right_delegate_->ScrollEnd();
  }
  state_ = state;
  if (state == NONE) {
    scroll_bezel_ = BEZEL_NONE;
    scroll_target_ = NULL;
  }
}

// Only implemented for LEFT and RIGHT bezels ATM.
BezelController::Bezel BezelController::GetBezel(const gfx::PointF& location) {
  if (location.x() < kBezelWidth) {
    return BEZEL_LEFT;
  } else if (location.x() >
             container_->GetBoundsInScreen().width() - kBezelWidth) {
    return BEZEL_RIGHT;
  } else {
    return BEZEL_NONE;
  }
}

void BezelController::OnGestureEvent(ui::GestureEvent* event) {
  // TODO (mfomitchev): Currently we aren't retargetting or consuming any of the
  // touch events. This means that content can prevent the generation of gesture
  // events and two-finger scroll won't work. Possible solution to this problem
  // is hosting our own gesture recognizer or retargetting touch events at the
  // bezel.

  if (!left_right_delegate_)
    return;

  ui::EventType type = event->type();
  if (!ShouldProcessGesture(type))
    return;

  if (scroll_target_ && event->target() != scroll_target_)
    return;

  const gfx::PointF& event_location = event->location_f();
  const ui::GestureEventDetails& event_details = event->details();
  int num_touch_points = event_details.touch_points();

  if (type == ui::ET_GESTURE_BEGIN) {
    if (num_touch_points > 2) {
      SetState(IGNORE_CURRENT_SCROLL,
               gfx::Point(kScrollPositionNone, kScrollPositionNone));
      return;
    }
    BezelController::Bezel event_bezel = GetBezel(event->location_f());
    switch (state_) {
      case NONE:
        scroll_bezel_ = event_bezel;
        scroll_target_ = event->target();
        if (event_bezel != BEZEL_LEFT && event_bezel != BEZEL_RIGHT) {
          SetState(IGNORE_CURRENT_SCROLL,
                   gfx::Point(kScrollPositionNone, kScrollPositionNone));
        } else {
          SetState(BEZEL_GESTURE_STARTED, event_location);
        }
        break;
      case IGNORE_CURRENT_SCROLL:
        break;
      case BEZEL_GESTURE_STARTED:
      case BEZEL_SCROLLING_ONE_FINGER:
        DCHECK_EQ(num_touch_points, 2);
        DCHECK(scroll_target_);
        DCHECK_NE(scroll_bezel_, BEZEL_NONE);

        if (event_bezel != scroll_bezel_) {
          SetState(IGNORE_CURRENT_SCROLL,
                   gfx::Point(kScrollPositionNone, kScrollPositionNone));
          return;
        }
        if (state_ == BEZEL_SCROLLING_ONE_FINGER)
          SetState(BEZEL_SCROLLING_TWO_FINGERS, event_location);
        break;
      case BEZEL_SCROLLING_TWO_FINGERS:
        // Should've exited above
        NOTREACHED();
        break;
    }
  } else if (type == ui::ET_GESTURE_END) {
    if (state_ == NONE)
      return;

    CHECK(scroll_target_);
    if (num_touch_points == 1) {
      SetState(NONE, gfx::Point(kScrollPositionNone, kScrollPositionNone));
    } else {
      SetState(IGNORE_CURRENT_SCROLL,
               gfx::Point(kScrollPositionNone, kScrollPositionNone));
    }
  } else if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
    DCHECK(state_ == IGNORE_CURRENT_SCROLL || state_ == BEZEL_GESTURE_STARTED);
    if (state_ != BEZEL_GESTURE_STARTED)
      return;

    if (num_touch_points == 1) {
      SetState(BEZEL_SCROLLING_ONE_FINGER, event_location);
      return;
    }

    DCHECK_EQ(num_touch_points, 2);
    SetState(BEZEL_SCROLLING_TWO_FINGERS, event_location);
    if (left_right_delegate_->CanScroll())
      event->SetHandled();
  } else if (type == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (state_ != BEZEL_SCROLLING_TWO_FINGERS)
      return;

    float scroll_delta = GetDistance(event_location, scroll_bezel_);
    left_right_delegate_->ScrollUpdate(scroll_delta);
    if (left_right_delegate_->CanScroll())
      event->SetHandled();
  }
}

}  // namespace athena
