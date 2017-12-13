// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOUSE_WHEEL_PHASE_HANDLER_H_
#define MOUSE_WHEEL_PHASE_HANDLER_H_

#include "base/timer/timer.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"

namespace content {
class RenderWidgetHostImpl;
class RenderWidgetHostViewBase;

// The duration after which a synthetic wheel with zero deltas and
// phase = |kPhaseEnded| will be sent after the last wheel event.
constexpr base::TimeDelta kDefaultMouseWheelLatchingTransaction =
    base::TimeDelta::FromMilliseconds(500);

// Maximum time that the phase handler waits for arrival of a wheel event with
// momentum_phase = kPhaseBegan before sending its previous wheel event with
// phase = kPhaseEnded.
constexpr base::TimeDelta kMaximumTimeBetweenPhaseEndedAndMomentumPhaseBegan =
    base::TimeDelta::FromMilliseconds(100);

// Maximum allowed difference between coordinates of two mouse wheel events in
// the same scroll sequence.
const double kWheelLatchingSlopRegion = 10.0;

// On ChromeOS wheel events don't have phase information; However, whenever the
// user puts down or lifts their fingers a GFC or GFS is received.
enum ScrollPhaseState {
  // Scrolling with normal mouse wheels doesn't give any information about the
  // state of scrolling.
  SCROLL_STATE_UNKNOWN = 0,
  // Shows that the user has put their fingers down and a scroll may start.
  SCROLL_MAY_BEGIN,
  // Scrolling has started and the user hasn't lift their fingers, yet.
  SCROLL_IN_PROGRESS,
};

class MouseWheelPhaseHandler {
 public:
  MouseWheelPhaseHandler(RenderWidgetHostImpl* const host,
                         RenderWidgetHostViewBase* const host_view);
  ~MouseWheelPhaseHandler() {}

  void AddPhaseIfNeededAndScheduleEndEvent(
      blink::WebMouseWheelEvent& mouse_wheel_event,
      bool should_route_event);
  void DispatchPendingWheelEndEvent();
  void IgnorePendingWheelEndEvent();
  void ResetScrollSequence();
  void SendWheelEndIfNeeded();
  void ScrollingMayBegin();

  // Used to set the timer timeout for testing.
  void set_mouse_wheel_end_dispatch_timeout(base::TimeDelta timeout) {
    mouse_wheel_end_dispatch_timeout_ = timeout;
  }

  bool HasPendingWheelEndEvent() const {
    return mouse_wheel_end_dispatch_timer_.IsRunning();
  }

 private:
  void SendSyntheticWheelEventWithPhaseEnded(
      bool should_route_event);
  void ScheduleMouseWheelEndDispatching(bool should_route_event,
                                        const base::TimeDelta timeout);
  bool IsWithinSlopRegion(blink::WebMouseWheelEvent wheel_event) const;

  RenderWidgetHostImpl* const host_;
  RenderWidgetHostViewBase* const host_view_;
  base::OneShotTimer mouse_wheel_end_dispatch_timer_;
  base::TimeDelta mouse_wheel_end_dispatch_timeout_;
  blink::WebMouseWheelEvent last_mouse_wheel_event_;
  ScrollPhaseState scroll_phase_state_;
  // This is used to break the timer based latching when the difference between
  // the locations of the first wheel event and the current wheel event is
  // larger than some threshold. The variable value is only valid while the
  // dispatch timer is running.
  gfx::Vector2dF first_wheel_location_;

  DISALLOW_COPY_AND_ASSIGN(MouseWheelPhaseHandler);
};

}  // namespace content

#endif  // MOUSE_WHEEL_PHASE_HANDLER_H_
