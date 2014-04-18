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
#include "ui/gfx/frame_time.h"

namespace cc {

FrameRateController::FrameRateController(scoped_refptr<TimeSource> timer)
    : client_(NULL),
      interval_(BeginFrameArgs::DefaultInterval()),
      time_source_(timer),
      active_(false) {
  time_source_->SetClient(this);
}

FrameRateController::~FrameRateController() { time_source_->SetActive(false); }

BeginFrameArgs FrameRateController::SetActive(bool active) {
  if (active_ == active)
    return BeginFrameArgs();
  TRACE_EVENT1("cc", "FrameRateController::SetActive", "active", active);
  active_ = active;

  base::TimeTicks missed_tick_time = time_source_->SetActive(active);
  if (!missed_tick_time.is_null()) {
    base::TimeTicks deadline = NextTickTime();
    return BeginFrameArgs::Create(
        missed_tick_time, deadline + deadline_adjustment_, interval_);
  }

  return BeginFrameArgs();
}

void FrameRateController::SetTimebaseAndInterval(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  interval_ = interval;
  time_source_->SetTimebaseAndInterval(timebase, interval);
}

void FrameRateController::SetDeadlineAdjustment(base::TimeDelta delta) {
  deadline_adjustment_ = delta;
}

void FrameRateController::OnTimerTick() {
  TRACE_EVENT0("cc", "FrameRateController::OnTimerTick");
  DCHECK(active_);

  if (client_) {
    // TODO(brianderson): Use an adaptive parent compositor deadline.
    base::TimeTicks frame_time = LastTickTime();
    base::TimeTicks deadline = NextTickTime();
    BeginFrameArgs args = BeginFrameArgs::Create(
        frame_time, deadline + deadline_adjustment_, interval_);
    client_->FrameRateControllerTick(args);
  }
}

base::TimeTicks FrameRateController::NextTickTime() {
  return time_source_->NextTickTime();
}

base::TimeTicks FrameRateController::LastTickTime() {
  return time_source_->LastTickTime();
}

}  // namespace cc
