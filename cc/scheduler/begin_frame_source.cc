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

void BeginFrameObserverBase::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  dict->BeginDictionary("last_begin_frame_args_");
  last_begin_frame_args_.AsValueInto(dict);
  dict->EndDictionary();
  dict->SetInteger("dropped_begin_frame_args_", dropped_begin_frame_args_);
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
  for (auto& it : observers)
    it->OnBeginFrame(args);
}

void BeginFrameSourceBase::SetBeginFrameSourcePaused(bool paused) {
  if (paused_ == paused)
    return;
  paused_ = paused;
  std::set<BeginFrameObserver*> observers(observers_);
  for (auto& it : observers)
    it->OnBeginFrameSourcePausedChanged(paused_);
}

// Tracing support
void BeginFrameSourceBase::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  // As the observer might try to trace the source, prevent an infinte loop
  // from occuring.
  if (inside_as_value_into_) {
    dict->SetString("observer", "<loop detected>");
    return;
  }

  {
    base::AutoReset<bool> prevent_loops(
        const_cast<bool*>(&inside_as_value_into_), true);
    dict->BeginArray("observers");
    for (const auto& it : observers_) {
      dict->BeginDictionary();
      it->AsValueInto(dict);
      dict->EndDictionary();
    }
    dict->EndArray();
  }
}

// BackToBackBeginFrameSource --------------------------------------------
scoped_ptr<BackToBackBeginFrameSource> BackToBackBeginFrameSource::Create(
    base::SingleThreadTaskRunner* task_runner) {
  return make_scoped_ptr(new BackToBackBeginFrameSource(task_runner));
}

BackToBackBeginFrameSource::BackToBackBeginFrameSource(
    base::SingleThreadTaskRunner* task_runner)
    : BeginFrameSourceBase(),
      task_runner_(task_runner),
      weak_factory_(this) {
  DCHECK(task_runner);
}

BackToBackBeginFrameSource::~BackToBackBeginFrameSource() {
}

base::TimeTicks BackToBackBeginFrameSource::Now() {
  return base::TimeTicks::Now();
}

// BeginFrameSourceBase support
void BackToBackBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(observers_.empty())
      << "BackToBackBeginFrameSource only supports a single observer";
  BeginFrameSourceBase::AddObserver(obs);
}

void BackToBackBeginFrameSource::OnNeedsBeginFramesChanged(
    bool needs_begin_frames) {
  if (needs_begin_frames) {
    PostBeginFrame();
  } else {
    begin_frame_task_.Cancel();
  }
}

void BackToBackBeginFrameSource::PostBeginFrame() {
  DCHECK(needs_begin_frames());
  begin_frame_task_.Reset(base::Bind(&BackToBackBeginFrameSource::BeginFrame,
                                     weak_factory_.GetWeakPtr()));
  task_runner_->PostTask(FROM_HERE, begin_frame_task_.callback());
}

void BackToBackBeginFrameSource::BeginFrame() {
  DCHECK(needs_begin_frames());
  DCHECK(!begin_frame_task_.IsCancelled());
  begin_frame_task_.Cancel();
  base::TimeTicks now = Now();
  BeginFrameArgs args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, now, now + BeginFrameArgs::DefaultInterval(),
      BeginFrameArgs::DefaultInterval(), BeginFrameArgs::NORMAL);
  CallOnBeginFrame(args);
}

void BackToBackBeginFrameSource::DidFinishFrame(size_t remaining_frames) {
  BeginFrameSourceBase::DidFinishFrame(remaining_frames);
  if (needs_begin_frames() && remaining_frames == 0)
    PostBeginFrame();
}

// Tracing support
void BackToBackBeginFrameSource::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  dict->SetString("type", "BackToBackBeginFrameSource");
  BeginFrameSourceBase::AsValueInto(dict);
}

// SyntheticBeginFrameSource ---------------------------------------------
scoped_ptr<SyntheticBeginFrameSource> SyntheticBeginFrameSource::Create(
    base::SingleThreadTaskRunner* task_runner,
    base::TimeDelta initial_vsync_interval) {
  scoped_ptr<DelayBasedTimeSource> time_source =
      DelayBasedTimeSource::Create(initial_vsync_interval, task_runner);
  return make_scoped_ptr(new SyntheticBeginFrameSource(std::move(time_source)));
}

SyntheticBeginFrameSource::SyntheticBeginFrameSource(
    scoped_ptr<DelayBasedTimeSource> time_source)
    : BeginFrameSourceBase(), time_source_(std::move(time_source)) {
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

// Tracing support
void SyntheticBeginFrameSource::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  dict->SetString("type", "SyntheticBeginFrameSource");
  BeginFrameSourceBase::AsValueInto(dict);

  dict->BeginDictionary("time_source");
  time_source_->AsValueInto(dict);
  dict->EndDictionary();
}

// BeginFrameSourceMultiplexer -------------------------------------------
scoped_ptr<BeginFrameSourceMultiplexer> BeginFrameSourceMultiplexer::Create() {
  return make_scoped_ptr(new BeginFrameSourceMultiplexer());
}

BeginFrameSourceMultiplexer::BeginFrameSourceMultiplexer()
    : BeginFrameSourceBase(),
      active_source_(nullptr),
      inside_add_observer_(false) {}

BeginFrameSourceMultiplexer::BeginFrameSourceMultiplexer(
    base::TimeDelta minimum_interval)
    : BeginFrameSourceBase(),
      active_source_(nullptr),
      inside_add_observer_(false) {}

BeginFrameSourceMultiplexer::~BeginFrameSourceMultiplexer() {
  if (active_source_ && needs_begin_frames())
    active_source_->RemoveObserver(this);
}

void BeginFrameSourceMultiplexer::SetMinimumInterval(
    base::TimeDelta new_minimum_interval) {
  DEBUG_FRAMES("BeginFrameSourceMultiplexer::SetMinimumInterval",
               "current minimum (us)",
               minimum_interval_.InMicroseconds(),
               "new minimum (us)",
               new_minimum_interval.InMicroseconds());
  DCHECK_GE(new_minimum_interval.ToInternalValue(), 0);
  minimum_interval_ = new_minimum_interval;
}

void BeginFrameSourceMultiplexer::AddSource(BeginFrameSource* new_source) {
  DEBUG_FRAMES("BeginFrameSourceMultiplexer::AddSource", "current active",
               active_source_, "source to be added", new_source);
  DCHECK(new_source);
  DCHECK(!HasSource(new_source));

  source_list_.insert(new_source);

  // If there is no active source, set the new one as the active one.
  if (!active_source_)
    SetActiveSource(new_source);
}

void BeginFrameSourceMultiplexer::RemoveSource(
    BeginFrameSource* existing_source) {
  DEBUG_FRAMES("BeginFrameSourceMultiplexer::RemoveSource", "current active",
               active_source_, "source to be removed", existing_source);
  DCHECK(existing_source);
  DCHECK(HasSource(existing_source));
  DCHECK_NE(existing_source, active_source_);
  source_list_.erase(existing_source);
}

void BeginFrameSourceMultiplexer::SetActiveSource(
    BeginFrameSource* new_source) {
  DEBUG_FRAMES("BeginFrameSourceMultiplexer::SetActiveSource",
               "current active",
               active_source_,
               "to become active",
               new_source);

  DCHECK(HasSource(new_source) || new_source == nullptr);

  if (active_source_ == new_source)
    return;

  if (active_source_ && needs_begin_frames())
    active_source_->RemoveObserver(this);

  active_source_ = new_source;

  if (active_source_ && needs_begin_frames())
    active_source_->AddObserver(this);
}

const BeginFrameSource* BeginFrameSourceMultiplexer::ActiveSource() {
  return active_source_;
}

// BeginFrameObserver support
void BeginFrameSourceMultiplexer::OnBeginFrame(const BeginFrameArgs& args) {
  if (!IsIncreasing(args)) {
    DEBUG_FRAMES("BeginFrameSourceMultiplexer::OnBeginFrame",
                 "action",
                 "discarding",
                 "new args",
                 args.AsValue());
    return;
  }
  DEBUG_FRAMES("BeginFrameSourceMultiplexer::OnBeginFrame",
               "action",
               "using",
               "new args",
               args.AsValue());
  last_begin_frame_args_ = args;
  CallOnBeginFrame(args);
}

const BeginFrameArgs& BeginFrameSourceMultiplexer::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void BeginFrameSourceMultiplexer::OnBeginFrameSourcePausedChanged(bool paused) {
  if (paused_ == paused)
    return;
  paused_ = paused;
  if (!inside_add_observer_) {
    std::set<BeginFrameObserver*> observers(observers_);
    for (auto& it : observers)
      it->OnBeginFrameSourcePausedChanged(paused);
  }
}

// BeginFrameSource support
void BeginFrameSourceMultiplexer::DidFinishFrame(size_t remaining_frames) {
  DEBUG_FRAMES("BeginFrameSourceMultiplexer::DidFinishFrame",
               "active_source",
               active_source_,
               "remaining_frames",
               remaining_frames);
  if (active_source_)
    active_source_->DidFinishFrame(remaining_frames);
}

void BeginFrameSourceMultiplexer::AddObserver(BeginFrameObserver* obs) {
  base::AutoReset<bool> reset(&inside_add_observer_, true);
  BeginFrameSourceBase::AddObserver(obs);
}

void BeginFrameSourceMultiplexer::OnNeedsBeginFramesChanged(
    bool needs_begin_frames) {
  if (!active_source_)
    return;
  if (needs_begin_frames) {
    active_source_->AddObserver(this);
  } else {
    active_source_->RemoveObserver(this);
  }
}

// Tracing support
void BeginFrameSourceMultiplexer::AsValueInto(
    base::trace_event::TracedValue* dict) const {
  dict->SetString("type", "BeginFrameSourceMultiplexer");

  dict->SetInteger("minimum_interval_us", minimum_interval_.InMicroseconds());

  dict->BeginDictionary("last_begin_frame_args");
  last_begin_frame_args_.AsValueInto(dict);
  dict->EndDictionary();

  if (active_source_) {
    dict->BeginDictionary("active_source");
    active_source_->AsValueInto(dict);
    dict->EndDictionary();
  } else {
    dict->SetString("active_source", "NULL");
  }

  dict->BeginArray("sources");
  for (std::set<BeginFrameSource*>::const_iterator it = source_list_.begin();
       it != source_list_.end();
       ++it) {
    dict->BeginDictionary();
    (*it)->AsValueInto(dict);
    dict->EndDictionary();
  }
  dict->EndArray();
}

// protected methods
bool BeginFrameSourceMultiplexer::HasSource(BeginFrameSource* source) {
  return (source_list_.find(source) != source_list_.end());
}

bool BeginFrameSourceMultiplexer::IsIncreasing(const BeginFrameArgs& args) {
  DCHECK(args.IsValid());

  // If the last begin frame is invalid, then any new begin frame is valid.
  if (!last_begin_frame_args_.IsValid())
    return true;

  // Only allow new args have a *strictly bigger* frame_time value and statisfy
  // minimum interval requirement.
  return (args.frame_time >=
          last_begin_frame_args_.frame_time + minimum_interval_);
}

}  // namespace cc
