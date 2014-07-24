// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_BEZEL_CONTROLLER_H_
#define ATHENA_WM_BEZEL_CONTROLLER_H_

#include "ui/events/event_handler.h"

namespace aura {
class Window;
}

namespace gfx {
class PointF;
}

namespace ui {
class EventTarget;
}

namespace athena {

// Responsible for detecting bezel gestures and notifying the delegate(s)
// subscribed to them.
class BezelController : public ui::EventHandler {
 public:
  enum Bezel { BEZEL_NONE, BEZEL_LEFT, BEZEL_RIGHT, BEZEL_TOP, BEZEL_BOTTOM };

  // Responsible for handling scroll gestures initiated from the bezel.
  // Two touch points are need to perform the bezel scroll gesture from
  // the left and right bezel.
  class ScrollDelegate {
   public:
    virtual ~ScrollDelegate() {}

    // Beginning of a bezel scroll gesture started from the |bezel|.
    virtual void ScrollBegin(Bezel bezel, float delta) = 0;

    // End of the current bezel scroll
    virtual void ScrollEnd() = 0;

    // Update of the scroll position for the currently active bezel scroll.
    virtual void ScrollUpdate(float delta) = 0;

    // Should return false if the delegate isn't going to react to the scroll
    // events.
    virtual bool CanScroll() = 0;
  };

  explicit BezelController(aura::Window* container);
  virtual ~BezelController() {}

  // Set the delegate to handle the gestures from left and right bezels.
  // Two touch points are need to perform the bezel scroll gesture from
  // the left and right bezel.
  void set_left_right_delegate(ScrollDelegate* delegate) {
    left_right_delegate_ = delegate;
  }

 private:
  enum State {
    NONE,
    IGNORE_CURRENT_SCROLL,
    BEZEL_GESTURE_STARTED,
    BEZEL_SCROLLING_ONE_FINGER,
    BEZEL_SCROLLING_TWO_FINGERS,
  };

  // Calculates the distance from |position| to the |bezel|.
  float GetDistance(const gfx::PointF& position, Bezel bezel);

  // |scroll_position| only needs to be passed in the scrolling state
  void SetState(State state, const gfx::PointF& scroll_position);

  // Returns the bezel corresponding to the |location| or BEZEL_NONE if the
  // location is outside of the bezel area.
  Bezel GetBezel(const gfx::PointF& location);

  // ui::EventHandler overrides
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  aura::Window* container_;

  State state_;

  // The bezel where the currently active scroll was started.
  Bezel scroll_bezel_;

  // The target of the bezel scroll gesture. Used to filter out other gestures
  // when the bezel scroll is in progress.
  ui::EventTarget* scroll_target_;

  // Responsible for handling gestures started from the left and right bezels.
  // Not owned.
  ScrollDelegate* left_right_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BezelController);
};

}  // namespace athena

#endif  // ATHENA_WM_BEZEL_CONTROLLER_H_
