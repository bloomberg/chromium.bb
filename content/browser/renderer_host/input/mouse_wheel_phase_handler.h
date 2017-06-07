// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOUSE_WHEEL_PHASE_HANDLER_H_
#define MOUSE_WHEEL_PHASE_HANDLER_H_

#include "base/timer/timer.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"

namespace content {
class RenderWidgetHostImpl;
class RenderWidgetHostViewBase;

// The duration after which a synthetic wheel with zero deltas and
// phase = |kPhaseEnded| will be sent after the last wheel event.
const int64_t kDefaultMouseWheelLatchingTransactionMs = 100;

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
  bool HasPendingWheelEndEvent() const {
    return mouse_wheel_end_dispatch_timer_.IsRunning();
  }

 private:
  void SendSyntheticWheelEventWithPhaseEnded(
      blink::WebMouseWheelEvent last_mouse_wheel_event,
      bool should_route_event);
  void ScheduleMouseWheelEndDispatching(blink::WebMouseWheelEvent wheel_event,
                                        bool should_route_event);

  RenderWidgetHostImpl* const host_;
  RenderWidgetHostViewBase* const host_view_;
  base::OneShotTimer mouse_wheel_end_dispatch_timer_;

  DISALLOW_COPY_AND_ASSIGN(MouseWheelPhaseHandler);
};

}  // namespace content

#endif  // MOUSE_WHEEL_PHASE_HANDLER_H_
