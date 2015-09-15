// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_USER_MODEL_H_
#define COMPONENTS_SCHEDULER_RENDERER_USER_MODEL_H_

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace scheduler {

class SCHEDULER_EXPORT UserModel {
 public:
  UserModel();
  ~UserModel();

  // Tells us that the system started processing an input event. Must be paired
  // with a call to DidFinishProcessingInputEvent.
  void DidStartProcessingInputEvent(blink::WebInputEvent::Type type,
                                    const base::TimeTicks now);

  // Tells us that the system finished processing an input event.
  void DidFinishProcessingInputEvent(const base::TimeTicks now);

  // Returns the estimated amount of time left in the current user gesture, to a
  // maximum of |kGestureEstimationLimitMillis|.  After that time has elapased
  // this function should be called again.
  base::TimeDelta TimeLeftInUserGesture(base::TimeTicks now) const;

  // Tries to guess if a user gesture is expected soon. Currently this is
  // very simple, but one day I hope to do something more sophisticated here.
  // The prediction may change after |prediction_valid_duration| has elapsed.
  bool IsGestureExpectedSoon(RendererScheduler::UseCase use_case,
                             const base::TimeTicks now,
                             base::TimeDelta* prediction_valid_duration) const;

  void AsValueInto(base::trace_event::TracedValue* state) const;

  // The time we should stay in a priority-escalated mode after an input event.
  static const int kGestureEstimationLimitMillis = 100;

  // TODO(alexclarke): Get a real number on actual data.
  static const int kMinimumTypicalScrollDurationMillis = 500;

  // We consider further gesture start events to be likely if the user has
  // interacted with the device in the past two seconds.
  // TODO(alexclarke): Get a real number based on actual data.
  static const int kExpectSubsequentGestureMillis = 2000;

  // Clears input signals.
  void Reset();

 private:
  int pending_input_event_count_;
  base::TimeTicks last_input_signal_time_;
  base::TimeTicks last_gesture_start_time_;
  base::TimeTicks last_continuous_gesture_time_;  // Doesn't include Taps.

  DISALLOW_COPY_AND_ASSIGN(UserModel);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_USER_MODEL_H_
