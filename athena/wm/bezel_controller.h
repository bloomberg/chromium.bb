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
    // |delta| is the difference between the x-coordinate of the current scroll
    // position and the bezel. It will be zero or negative for the right bezel.
    virtual void BezelScrollBegin(Bezel bezel, float delta) = 0;

    // End of the current bezel scroll
    virtual void BezelScrollEnd() = 0;

    // Update of the scroll position for the currently active bezel scroll.
    // |delta| has the same meaning as in ScrollBegin().
    virtual void BezelScrollUpdate(float delta) = 0;

    // Should return false if the delegate isn't going to react to the scroll
    // events.
    virtual bool BezelCanScroll() = 0;
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

  void SetState(State state);
  // |scroll_delta| only needs to be passed when |state| is one of the
  // BEZEL_SROLLING states.
  void SetState(State state, float scroll_delta);

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
