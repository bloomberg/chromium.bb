// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/frame_rate_controller.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/time_source.h"

namespace cc {

class FrameRateControllerTimeSourceAdapter : public TimeSourceClient {
 public:
  static scoped_ptr<FrameRateControllerTimeSourceAdapter> Create(
      FrameRateController* frame_rate_controller) {
    return make_scoped_ptr(
        new FrameRateControllerTimeSourceAdapter(frame_rate_controller));
  }
  virtual ~FrameRateControllerTimeSourceAdapter() {}

  virtual void OnTimerTick() OVERRIDE {
    frame_rate_controller_->OnTimerTick();
  }

 private:
  explicit FrameRateControllerTimeSourceAdapter(
      FrameRateController* frame_rate_controller)
      : frame_rate_controller_(frame_rate_controller) {}

  FrameRateController* frame_rate_controller_;
};

FrameRateController::FrameRateController(scoped_refptr<TimeSource> timer)
    : client_(NULL),
      num_frames_pending_(0),
      max_swaps_pending_(0),
      interval_(BeginFrameArgs::DefaultInterval()),
      time_source_(timer),
      active_(false),
      is_time_source_throttling_(true),
      weak_factory_(this),
      task_runner_(NULL) {
  time_source_client_adapter_ =
      FrameRateControllerTimeSourceAdapter::Create(this);
  time_source_->SetClient(time_source_client_adapter_.get());
}

FrameRateController::FrameRateController(
    base::SingleThreadTaskRunner* task_runner)
    : client_(NULL),
      num_frames_pending_(0),
      max_swaps_pending_(0),
      interval_(BeginFrameArgs::DefaultInterval()),
      active_(false),
      is_time_source_throttling_(false),
      weak_factory_(this),
      task_runner_(task_runner) {}

FrameRateController::~FrameRateController() {
  if (is_time_source_throttling_)
    time_source_->SetActive(false);
}

BeginFrameArgs FrameRateController::SetActive(bool active) {
  if (active_ == active)
    return BeginFrameArgs();
  TRACE_EVENT1("cc", "FrameRateController::SetActive", "active", active);
  bool just_activated = active && !active_;
  active_ = active;

  if (is_time_source_throttling_) {
    time_source_->SetActive(active);
  } else {
    if (active)
      PostManualTick();
    else
      weak_factory_.InvalidateWeakPtrs();
  }

  if (just_activated) {
    // TODO(brianderson): Use an adaptive parent compositor deadline.
    base::TimeTicks frame_time = NextTickTime() - interval_;
    base::TimeTicks deadline = NextTickTime();
    return BeginFrameArgs::Create(frame_time, deadline, interval_);
  }
  return BeginFrameArgs();
}

void FrameRateController::SetMaxSwapsPending(int max_swaps_pending) {
  DCHECK_GE(max_swaps_pending, 0);
  max_swaps_pending_ = max_swaps_pending;
}

void FrameRateController::SetTimebaseAndInterval(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  interval_ = interval;
  if (is_time_source_throttling_)
    time_source_->SetTimebaseAndInterval(timebase, interval);
}

void FrameRateController::SetDeadlineAdjustment(base::TimeDelta delta) {
  deadline_adjustment_ = delta;
}

void FrameRateController::OnTimerTick() {
  TRACE_EVENT0("cc", "FrameRateController::OnTimerTick");
  DCHECK(active_);

  // Check if we have too many frames in flight.
  bool throttled =
      max_swaps_pending_ && num_frames_pending_ >= max_swaps_pending_;
  TRACE_COUNTER_ID1("cc", "ThrottledCompositor", task_runner_, throttled);

  if (client_) {
    // TODO(brianderson): Use an adaptive parent compositor deadline.
    base::TimeTicks frame_time = LastTickTime();
    base::TimeTicks deadline = NextTickTime() + deadline_adjustment_;
    client_->FrameRateControllerTick(
        throttled,
        BeginFrameArgs::Create(frame_time, deadline, interval_));
  }

  if (!is_time_source_throttling_ && !throttled)
    PostManualTick();
}

void FrameRateController::PostManualTick() {
  if (active_) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&FrameRateController::ManualTick,
                                      weak_factory_.GetWeakPtr()));
  }
}

void FrameRateController::ManualTick() {
  OnTimerTick();
}

void FrameRateController::DidSwapBuffers() {
  num_frames_pending_++;
}

void FrameRateController::DidSwapBuffersComplete() {
  DCHECK_GT(num_frames_pending_, 0);
  num_frames_pending_--;
  if (!is_time_source_throttling_)
    PostManualTick();
}

void FrameRateController::DidAbortAllPendingFrames() {
  num_frames_pending_ = 0;
}

base::TimeTicks FrameRateController::NextTickTime() {
  if (is_time_source_throttling_)
    return time_source_->NextTickTime();

  return base::TimeTicks();
}

base::TimeTicks FrameRateController::LastTickTime() {
  if (is_time_source_throttling_)
    return time_source_->LastTickTime();

  return base::TimeTicks::Now();
}

}  // namespace cc
