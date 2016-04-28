// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/begin_frame_source.h"

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/scheduler.h"

namespace cc {

namespace {
// kDoubleTickDivisor prevents the SyntheticBFS from sending BeginFrames too
// often to an observer.
static const double kDoubleTickDivisor = 2.0;
}

// BeginFrameObserverBase -----------------------------------------------
BeginFrameObserverBase::BeginFrameObserverBase()
    : last_begin_frame_args_(), dropped_begin_frame_args_(0) {
}

const BeginFrameArgs& BeginFrameObserverBase::LastUsedBeginFrameArgs() const {
  return last_begin_frame_args_;
}
void BeginFrameObserverBase::OnBeginFrame(const BeginFrameArgs& args) {
  DEBUG_FRAMES("BeginFrameObserverBase::OnBeginFrame",
               "last args",
               last_begin_frame_args_.AsValue(),
               "new args",
               args.AsValue());
  DCHECK(args.IsValid());
  DCHECK(args.frame_time >= last_begin_frame_args_.frame_time);
  bool used = OnBeginFrameDerivedImpl(args);
  if (used) {
    last_begin_frame_args_ = args;
  } else {
    ++dropped_begin_frame_args_;
  }
}

// BeginFrameSourceBase ------------------------------------------------------
BeginFrameSourceBase::BeginFrameSourceBase()
    : paused_(false), inside_as_value_into_(false) {}

BeginFrameSourceBase::~BeginFrameSourceBase() {}

void BeginFrameSourceBase::AddObserver(BeginFrameObserver* obs) {
  DEBUG_FRAMES("BeginFrameSourceBase::AddObserver", "num observers",
               observers_.size(), "to add observer", obs);
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end())
      << "AddObserver cannot be called with an observer that was already added";
  bool observers_was_empty = observers_.empty();
  observers_.insert(obs);
  if (observers_was_empty)
    OnNeedsBeginFramesChanged(true);
  obs->OnBeginFrameSourcePausedChanged(paused_);
}

void BeginFrameSourceBase::RemoveObserver(BeginFrameObserver* obs) {
  DEBUG_FRAMES("BeginFrameSourceBase::RemoveObserver", "num observers",
               observers_.size(), "removed observer", obs);
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end())
      << "RemoveObserver cannot be called with an observer that wasn't added";
  observers_.erase(obs);
  if (observers_.empty())
    OnNeedsBeginFramesChanged(false);
}

void BeginFrameSourceBase::CallOnBeginFrame(const BeginFrameArgs& args) {
  DEBUG_FRAMES("BeginFrameSourceBase::CallOnBeginFrame", "num observers",
               observers_.size(), "args", args.AsValue());
  std::set<BeginFrameObserver*> observers(observers_);
  for (BeginFrameObserver* obs : observers)
    obs->OnBeginFrame(args);
}

void BeginFrameSourceBase::SetBeginFrameSourcePaused(bool paused) {
  if (paused_ == paused)
    return;
  paused_ = paused;
  std::set<BeginFrameObserver*> observers(observers_);
  for (BeginFrameObserver* obs : observers)
    obs->OnBeginFrameSourcePausedChanged(paused_);
}

// BackToBackBeginFrameSource --------------------------------------------
BackToBackBeginFrameSource::BackToBackBeginFrameSource(
    base::SingleThreadTaskRunner* task_runner)
    : BeginFrameSourceBase(), task_runner_(task_runner), weak_factory_(this) {
  DCHECK(task_runner);
}

BackToBackBeginFrameSource::~BackToBackBeginFrameSource() {
}

base::TimeTicks BackToBackBeginFrameSource::Now() {
  return base::TimeTicks::Now();
}

// BeginFrameSourceBase support
void BackToBackBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  BeginFrameSourceBase::AddObserver(obs);
  pending_begin_frame_observers_.insert(obs);
  PostPendingBeginFramesTask();
}

void BackToBackBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  BeginFrameSourceBase::RemoveObserver(obs);
  pending_begin_frame_observers_.erase(obs);
  if (pending_begin_frame_observers_.empty())
    begin_frame_task_.Cancel();
}

void BackToBackBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs,
                                                size_t remaining_frames) {
  BeginFrameSourceBase::DidFinishFrame(obs, remaining_frames);
  if (remaining_frames == 0 && observers_.find(obs) != observers_.end()) {
    pending_begin_frame_observers_.insert(obs);
    PostPendingBeginFramesTask();
  }
}

void BackToBackBeginFrameSource::PostPendingBeginFramesTask() {
  DCHECK(needs_begin_frames());
  DCHECK(!pending_begin_frame_observers_.empty());
  if (begin_frame_task_.IsCancelled()) {
    begin_frame_task_.Reset(
        base::Bind(&BackToBackBeginFrameSource::SendPendingBeginFrames,
                   weak_factory_.GetWeakPtr()));
    task_runner_->PostTask(FROM_HERE, begin_frame_task_.callback());
  }
}

void BackToBackBeginFrameSource::SendPendingBeginFrames() {
  DCHECK(needs_begin_frames());
  DCHECK(!begin_frame_task_.IsCancelled());
  begin_frame_task_.Cancel();

  base::TimeTicks now = Now();
  BeginFrameArgs args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, now, now + BeginFrameArgs::DefaultInterval(),
      BeginFrameArgs::DefaultInterval(), BeginFrameArgs::NORMAL);

  std::set<BeginFrameObserver*> pending_observers;
  pending_observers.swap(pending_begin_frame_observers_);
  for (BeginFrameObserver* obs : pending_observers)
    obs->OnBeginFrame(args);
}

// SyntheticBeginFrameSource ---------------------------------------------
SyntheticBeginFrameSource::SyntheticBeginFrameSource(
    base::SingleThreadTaskRunner* task_runner,
    base::TimeDelta initial_vsync_interval)
    : time_source_(
          DelayBasedTimeSource::Create(initial_vsync_interval, task_runner)) {
  time_source_->SetClient(this);
}

SyntheticBeginFrameSource::SyntheticBeginFrameSource(
    std::unique_ptr<DelayBasedTimeSource> time_source)
    : time_source_(std::move(time_source)) {
  time_source_->SetClient(this);
}

SyntheticBeginFrameSource::~SyntheticBeginFrameSource() {}

void SyntheticBeginFrameSource::OnUpdateVSyncParameters(
    base::TimeTicks new_vsync_timebase,
    base::TimeDelta new_vsync_interval) {
  time_source_->SetTimebaseAndInterval(new_vsync_timebase, new_vsync_interval);
}

BeginFrameArgs SyntheticBeginFrameSource::CreateBeginFrameArgs(
    base::TimeTicks frame_time,
    BeginFrameArgs::BeginFrameArgsType type) {
  return BeginFrameArgs::Create(BEGINFRAME_FROM_HERE, frame_time,
                                time_source_->NextTickTime(),
                                time_source_->Interval(), type);
}

// BeginFrameSource support
void SyntheticBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  BeginFrameSourceBase::AddObserver(obs);
  BeginFrameArgs args = CreateBeginFrameArgs(
      time_source_->NextTickTime() - time_source_->Interval(),
      BeginFrameArgs::MISSED);
  BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
  if (!last_args.IsValid() ||
      (args.frame_time >
       last_args.frame_time + args.interval / kDoubleTickDivisor)) {
    obs->OnBeginFrame(args);
  }
}

void SyntheticBeginFrameSource::OnNeedsBeginFramesChanged(
    bool needs_begin_frames) {
  time_source_->SetActive(needs_begin_frames);
}

// DelayBasedTimeSourceClient support
void SyntheticBeginFrameSource::OnTimerTick() {
  BeginFrameArgs args = CreateBeginFrameArgs(time_source_->LastTickTime(),
                                             BeginFrameArgs::NORMAL);
  std::set<BeginFrameObserver*> observers(observers_);
  for (auto& it : observers) {
    BeginFrameArgs last_args = it->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        (args.frame_time >
         last_args.frame_time + args.interval / kDoubleTickDivisor)) {
      it->OnBeginFrame(args);
    }
  }
}

}  // namespace cc
