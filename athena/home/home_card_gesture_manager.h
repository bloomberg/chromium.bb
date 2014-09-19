// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_HOME_CARD_GESTURE_MANAGER_H_
#define ATHENA_HOME_HOME_CARD_GESTURE_MANAGER_H_

#include "athena/home/public/home_card.h"
#include "athena/athena_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class GestureEvent;
}

namespace athena {

// Handles the touch gestures over the home card.
class ATHENA_EXPORT HomeCardGestureManager {
 public:
  class Delegate {
   public:
    // Called when the gesture has ended. The state of the home card will
    // end up with |final_state|. |is_fling| is true only when the gesture has
    // ended with a fling action.
    virtual void OnGestureEnded(HomeCard::State final_state,
                                bool is_fling) = 0;

    // Called when the gesture position is updated so that |delegate| updates
    // the visual. The arguments indicate that the gesture is switching between
    // |from_state| and |to_state|, and that the level of progress is at
    // |progress|, which is in the range (0, 1]. The home card was previously
    // at either |from_state| or |to_state|. In particular, the home card may
    // never have been at |from_state|. |from_state| is never equal to
    // |to_state|.
    virtual void OnGestureProgressed(
        HomeCard::State from_state,
        HomeCard::State to_state,
        float progress) = 0;
  };

  HomeCardGestureManager(Delegate* delegate,
                         const gfx::Rect& screen_bounds);
  ~HomeCardGestureManager();

  void ProcessGestureEvent(ui::GestureEvent* event);

 private:
  // Get the final state from the last position.
  HomeCard::State GetFinalState() const;

  // Update the current position and emits OnGestureProgressed().
  void UpdateScrollState(const ui::GestureEvent& event);

  Delegate* delegate_;  // Not owned.

  // The state when the gesture starts.
  HomeCard::State original_state_;

  // The offset from the top edge of the home card and the initial position of
  // gesture.
  int y_offset_;

  // The estimated height of the home card after the last touch event.
  int last_estimated_height_;

  // The bounds of the screen to compute the home card bounds.
  gfx::Rect screen_bounds_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardGestureManager);
};

}  // namespace athena

#endif  // ATHENA_HOME_HOME_CARD_GESTURE_MANAGER_H_
