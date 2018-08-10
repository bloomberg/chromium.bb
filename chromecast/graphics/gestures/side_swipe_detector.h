// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_SIDE_SWIPE_DETECTOR_H_
#define CHROMECAST_GRAPHICS_GESTURES_SIDE_SWIPE_DETECTOR_H_

#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromecast/graphics/cast_gesture_handler.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/gesture_detection/gesture_detector.h"

namespace aura {
class Window;
}  // namespace aura

namespace chromecast {

namespace test {
class SideSwipeDetectorTest;
}  // namespace test

// An event rewriter for detecting system-wide gestures performed on the margins
// of the root window.
// Recognizes swipe gestures that originate from the top, left, bottom, and
// right of the root window.  Stashes copies of touch events that occur during
// the side swipe, and replays them if the finger releases before leaving the
// margin area.
class SideSwipeDetector : public ui::EventRewriter {
 public:
  SideSwipeDetector(CastGestureHandler* gesture_handler,
                    aura::Window* root_window);

  ~SideSwipeDetector() override;

  bool GetMarginPosition(const gfx::Point& point,
                         const gfx::Rect& screen_bounds,
                         CastSideSwipeOrigin* side,
                         CastGestureHandler::Corner* corner) const;

  // Overridden from ui::EventRewriter
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* rewritten_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

 private:
  friend class test::SideSwipeDetectorTest;
  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> mock_timer);

  void StashEvent(const ui::TouchEvent& event);
  void OnCornerHoldTimerFired();
  void EndCornerHold(const ui::TouchEvent& event);
  void CancelCornerHoldCheck();

  ui::EventRewriteStatus StartNextDispatchStashedEvents(
      std::unique_ptr<ui::Event>* new_event);

  const int gesture_start_width_;
  const int gesture_start_height_;
  const int bottom_gesture_start_height_;

  CastGestureHandler* gesture_handler_;
  aura::Window* root_window_;
  CastSideSwipeOrigin current_swipe_origin_;
  int current_pointer_id_;
  base::ElapsedTimer gesture_elapsed_timer_;
  std::deque<ui::TouchEvent> stashed_events_;

  // A default gesture detector config, so we can share the same
  // longpress timeout for corner hold.
  ui::GestureDetector::Config gesture_detector_config_;
  std::unique_ptr<base::OneShotTimer> corner_hold_timer_;
  std::unique_ptr<ui::Event> corner_hold_start_event_;
  bool in_corner_hold_;
  CastGestureHandler::Corner current_corner_origin_;

  DISALLOW_COPY_AND_ASSIGN(SideSwipeDetector);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_GESTURES_SIDE_SWIPE_DETECTOR_H_
