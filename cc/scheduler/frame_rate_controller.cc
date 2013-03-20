// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/frame_rate_controller.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/base/thread.h"
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

  virtual void onTimerTick() OVERRIDE { frame_rate_controller_->OnTimerTick(); }

 private:
  explicit FrameRateControllerTimeSourceAdapter(
      FrameRateController* frame_rate_controller)
      : frame_rate_controller_(frame_rate_controller) {}

  FrameRateController* frame_rate_controller_;
};

FrameRateController::FrameRateController(scoped_refptr<TimeSource> timer)
    : client_(NULL),
      num_frames_pending_(0),
      max_frames_pending_(0),
      time_source_(timer),
      active_(false),
      swap_buffers_complete_supported_(true),
      is_time_source_throttling_(true),
      thread_(NULL),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  time_source_client_adapter_ =
      FrameRateControllerTimeSourceAdapter::Create(this);
  time_source_->setClient(time_source_client_adapter_.get());
}

FrameRateController::FrameRateController(Thread* thread)
    : client_(NULL),
      num_frames_pending_(0),
      max_frames_pending_(0),
      active_(false),
      swap_buffers_complete_supported_(true),
      is_time_source_throttling_(false),
      thread_(thread),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

FrameRateController::~FrameRateController() {
  if (is_time_source_throttling_)
    time_source_->setActive(false);
}

void FrameRateController::SetActive(bool active) {
  if (active_ == active)
    return;
  TRACE_EVENT1("cc", "FrameRateController::SetActive", "active", active);
  active_ = active;

  if (is_time_source_throttling_) {
    time_source_->setActive(active);
  } else {
    if (active)
      PostManualTick();
    else
      weak_factory_.InvalidateWeakPtrs();
  }
}

void FrameRateController::SetMaxFramesPending(int max_frames_pending) {
  DCHECK_GE(max_frames_pending, 0);
  max_frames_pending_ = max_frames_pending;
}

void FrameRateController::SetTimebaseAndInterval(base::TimeTicks timebase,
                                                 base::TimeDelta interval) {
  if (is_time_source_throttling_)
    time_source_->setTimebaseAndInterval(timebase, interval);
}

void FrameRateController::SetSwapBuffersCompleteSupported(bool supported) {
  swap_buffers_complete_supported_ = supported;
}

void FrameRateController::OnTimerTick() {
  DCHECK(active_);

  // Check if we have too many frames in flight.
  bool throttled =
      max_frames_pending_ && num_frames_pending_ >= max_frames_pending_;
  TRACE_COUNTER_ID1("cc", "ThrottledVSyncInterval", thread_, throttled);

  if (client_)
    client_->VSyncTick(throttled);

  if (swap_buffers_complete_supported_ && !is_time_source_throttling_ &&
      !throttled)
    PostManualTick();
}

void FrameRateController::PostManualTick() {
  if (active_) {
    thread_->PostTask(base::Bind(&FrameRateController::ManualTick,
                                 weak_factory_.GetWeakPtr()));
  }
}

void FrameRateController::ManualTick() { OnTimerTick(); }

void FrameRateController::DidBeginFrame() {
  if (swap_buffers_complete_supported_)
    num_frames_pending_++;
  else if (!is_time_source_throttling_)
    PostManualTick();
}

void FrameRateController::DidFinishFrame() {
  DCHECK(swap_buffers_complete_supported_);

  num_frames_pending_--;
  if (!is_time_source_throttling_)
    PostManualTick();
}

void FrameRateController::DidAbortAllPendingFrames() {
  num_frames_pending_ = 0;
}

base::TimeTicks FrameRateController::NextTickTime() {
  if (is_time_source_throttling_)
    return time_source_->nextTickTime();

  return base::TimeTicks();
}

base::TimeTicks FrameRateController::LastTickTime() {
  if (is_time_source_throttling_)
    return time_source_->lastTickTime();

  return base::TimeTicks::Now();
}

}  // namespace cc
