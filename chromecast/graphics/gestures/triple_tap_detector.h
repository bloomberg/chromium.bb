// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GRAPHICS_GESTURES_TRIPLE_TAP_DETECTOR_H_
#define CHROMECAST_GRAPHICS_GESTURES_TRIPLE_TAP_DETECTOR_H_

#include <deque>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {
class Event;
class TouchEvent;
}  // namespace ui

namespace chromecast {

class TripleTapDetectorDelegate {
 public:
  virtual ~TripleTapDetectorDelegate() = default;
  virtual void OnTripleTap(const gfx::Point& touch_location) = 0;
};

enum class TripleTapState {
  NONE,
  TOUCH,
  INTERVAL_WAIT,
};

// An event rewriter responsible for detecting triple-tap events on the root
// window.
class TripleTapDetector : public ui::EventRewriter {
 public:
  TripleTapDetector(aura::Window* root_window,
                    TripleTapDetectorDelegate* delegate);
  ~TripleTapDetector() override;

  void set_enabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

  // Overridden from ui::EventRewriter
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* rewritten_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

 private:
  friend class TripleTapDetectorTest;

  // Expiration event for maximum time between taps in a triple tap.
  void OnTripleTapIntervalTimerFired();
  // Expiration event for a finger that is pressed too long during a triple tap.
  void OnLongPressIntervalTimerFired();

  void TripleTapReset();

  // A default gesture detector config, so we can share the same
  // timeout and pixel slop constants.
  ui::GestureDetector::Config gesture_detector_config_;

  aura::Window* root_window_;
  TripleTapDetectorDelegate* delegate_;

  bool enabled_;

  TripleTapState triple_tap_state_;
  int tap_count_;
  gfx::Point last_tap_location_;
  base::OneShotTimer triple_tap_timer_;
  std::deque<ui::TouchEvent> stashed_events_;

  DISALLOW_COPY_AND_ASSIGN(TripleTapDetector);
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_GESTURES_TRIPLE_TAP_DETECTOR_H_
