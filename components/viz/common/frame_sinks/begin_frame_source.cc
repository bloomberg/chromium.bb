// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include <stddef.h>

#include "base/atomic_sequence_num.h"
#include "base/auto_reset.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/viz/common/frame_sinks/delay_based_time_source.h"

namespace viz {

namespace {
// kDoubleTickDivisor prevents the SyntheticBFS from sending BeginFrames too
// often to an observer.
static const double kDoubleTickDivisor = 2.0;
}  // namespace

// BeginFrameObserverBase -------------------------------------------------
BeginFrameObserverBase::BeginFrameObserverBase() = default;

BeginFrameObserverBase::~BeginFrameObserverBase() = default;

const BeginFrameArgs& BeginFrameObserverBase::LastUsedBeginFrameArgs() const {
  return last_begin_frame_args_;
}

void BeginFrameObserverBase::OnBeginFrame(const BeginFrameArgs& args) {
  DCHECK(args.IsValid());
  DCHECK(args.frame_time >= last_begin_frame_args_.frame_time);
  DCHECK(args.sequence_number > last_begin_frame_args_.sequence_number ||
         args.source_id != last_begin_frame_args_.source_id)
      << "current " << args.AsValue()->ToString() << ", last "
      << last_begin_frame_args_.AsValue()->ToString();
  bool used = OnBeginFrameDerivedImpl(args);
  if (used) {
    last_begin_frame_args_ = args;
  } else {
    ++dropped_begin_frame_args_;
  }
}

void BeginFrameObserverBase::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("dropped_begin_frame_args", dropped_begin_frame_args_);

  state->BeginDictionary("last_begin_frame_args");
  last_begin_frame_args_.AsValueInto(state);
  state->EndDictionary();
}

// BeginFrameSource -------------------------------------------------------
namespace {
static base::AtomicSequenceNumber g_next_source_id;
}  // namespace

BeginFrameSource::BeginFrameSource() : source_id_(g_next_source_id.GetNext()) {}

BeginFrameSource::~BeginFrameSource() = default;

void BeginFrameSource::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("source_id", source_id_);
}

// StubBeginFrameSource ---------------------------------------------------
bool StubBeginFrameSource::IsThrottled() const {
  return true;
}

// SyntheticBeginFrameSource ----------------------------------------------
SyntheticBeginFrameSource::~SyntheticBeginFrameSource() = default;

// BackToBackBeginFrameSource ---------------------------------------------
BackToBackBeginFrameSource::BackToBackBeginFrameSource(
    std::unique_ptr<DelayBasedTimeSource> time_source)
    : time_source_(std::move(time_source)),
      next_sequence_number_(BeginFrameArgs::kStartingFrameNumber),
      weak_factory_(this) {
  time_source_->SetClient(this);
  // The time_source_ ticks immediately, so we SetActive(true) for a single
  // tick when we need it, and keep it as SetActive(false) otherwise.
  time_source_->SetTimebaseAndInterval(base::TimeTicks(), base::TimeDelta());
}

BackToBackBeginFrameSource::~BackToBackBeginFrameSource() = default;

void BackToBackBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());
  observers_.insert(obs);
  pending_begin_frame_observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  time_source_->SetActive(true);
}

void BackToBackBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());
  observers_.erase(obs);
  pending_begin_frame_observers_.erase(obs);
  if (pending_begin_frame_observers_.empty())
    time_source_->SetActive(false);
}

void BackToBackBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs) {
  if (observers_.find(obs) != observers_.end()) {
    pending_begin_frame_observers_.insert(obs);
    time_source_->SetActive(true);
  }
}

bool BackToBackBeginFrameSource::IsThrottled() const {
  return false;
}

void BackToBackBeginFrameSource::OnTimerTick() {
  base::TimeTicks frame_time = time_source_->LastTickTime();
  base::TimeDelta default_interval = BeginFrameArgs::DefaultInterval();
  BeginFrameArgs args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), next_sequence_number_, frame_time,
      frame_time + default_interval, default_interval, BeginFrameArgs::NORMAL);
  next_sequence_number_++;

  // This must happen after getting the LastTickTime() from the time source.
  time_source_->SetActive(false);

  std::unordered_set<BeginFrameObserver*> pending_observers;
  pending_observers.swap(pending_begin_frame_observers_);
  DCHECK(!pending_observers.empty());
  for (BeginFrameObserver* obs : pending_observers)
    obs->OnBeginFrame(args);
}

// DelayBasedBeginFrameSource ---------------------------------------------
DelayBasedBeginFrameSource::DelayBasedBeginFrameSource(
    std::unique_ptr<DelayBasedTimeSource> time_source)
    : time_source_(std::move(time_source)),
      next_sequence_number_(BeginFrameArgs::kStartingFrameNumber) {
  time_source_->SetClient(this);
}

DelayBasedBeginFrameSource::~DelayBasedBeginFrameSource() = default;

void DelayBasedBeginFrameSource::OnUpdateVSyncParameters(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  if (!authoritative_interval_.is_zero()) {
    interval = authoritative_interval_;
  } else if (interval.is_zero()) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    interval = BeginFrameArgs::DefaultInterval();
  }

  last_timebase_ = timebase;
  time_source_->SetTimebaseAndInterval(timebase, interval);
}

void DelayBasedBeginFrameSource::SetAuthoritativeVSyncInterval(
    base::TimeDelta interval) {
  authoritative_interval_ = interval;
  OnUpdateVSyncParameters(last_timebase_, interval);
}

BeginFrameArgs DelayBasedBeginFrameSource::CreateBeginFrameArgs(
    base::TimeTicks frame_time) {
  uint64_t sequence_number = next_sequence_number_++;
  return BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), sequence_number, frame_time,
      time_source_->NextTickTime(), time_source_->Interval(),
      BeginFrameArgs::NORMAL);
}

void DelayBasedBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  time_source_->SetActive(true);

  // Missed args should correspond to |last_begin_frame_args_| (particularly,
  // have the same sequence number) if |last_begin_frame_args_| still correspond
  // to the last time the time source should have ticked. This may not be the
  // case if the time source was inactive before AddObserver() was called. In
  // such a case, we create new args with a new sequence number only if
  // sufficient time has passed since the last tick.
  base::TimeTicks last_or_missed_tick_time =
      time_source_->NextTickTime() - time_source_->Interval();
  if (!last_begin_frame_args_.IsValid() ||
      last_or_missed_tick_time >
          last_begin_frame_args_.frame_time +
              last_begin_frame_args_.interval / kDoubleTickDivisor) {
    last_begin_frame_args_ = CreateBeginFrameArgs(last_or_missed_tick_time);
  }
  BeginFrameArgs missed_args = last_begin_frame_args_;
  missed_args.type = BeginFrameArgs::MISSED;

  BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
  if (!last_args.IsValid() ||
      (missed_args.frame_time >
       last_args.frame_time + missed_args.interval / kDoubleTickDivisor)) {
    DCHECK(missed_args.sequence_number > last_args.sequence_number ||
           missed_args.source_id != last_args.source_id)
        << "missed " << missed_args.AsValue()->ToString() << ", last "
        << last_args.AsValue()->ToString();
    obs->OnBeginFrame(missed_args);
  }
}

void DelayBasedBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());

  observers_.erase(obs);
  if (observers_.empty())
    time_source_->SetActive(false);
}

bool DelayBasedBeginFrameSource::IsThrottled() const {
  return true;
}

void DelayBasedBeginFrameSource::OnTimerTick() {
  last_begin_frame_args_ = CreateBeginFrameArgs(time_source_->LastTickTime());
  std::unordered_set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers) {
    BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        (last_begin_frame_args_.frame_time >
         last_args.frame_time +
             last_begin_frame_args_.interval / kDoubleTickDivisor)) {
      obs->OnBeginFrame(last_begin_frame_args_);
    }
  }
}

// ExternalBeginFrameSource -----------------------------------------------
ExternalBeginFrameSource::ExternalBeginFrameSource(
    ExternalBeginFrameSourceClient* client)
    : client_(client) {
  DCHECK(client_);
}

ExternalBeginFrameSource::~ExternalBeginFrameSource() = default;

void ExternalBeginFrameSource::AsValueInto(
    base::trace_event::TracedValue* state) const {
  BeginFrameSource::AsValueInto(state);

  state->SetBoolean("paused", paused_);
  state->SetInteger("num_observers", observers_.size());

  state->BeginDictionary("last_begin_frame_args");
  last_begin_frame_args_.AsValueInto(state);
  state->EndDictionary();
}

void ExternalBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  bool observers_was_empty = observers_.empty();
  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(paused_);
  if (observers_was_empty)
    client_->OnNeedsBeginFrames(true);

  // Send a MISSED begin frame if necessary.
  if (last_begin_frame_args_.IsValid()) {
    const BeginFrameArgs& last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        (last_begin_frame_args_.frame_time > last_args.frame_time)) {
      DCHECK(
          (last_begin_frame_args_.source_id != last_args.source_id) ||
          (last_begin_frame_args_.sequence_number > last_args.sequence_number))
          << "current " << last_begin_frame_args_.AsValue()->ToString()
          << ", last " << last_args.AsValue()->ToString();
      BeginFrameArgs missed_args = last_begin_frame_args_;
      missed_args.type = BeginFrameArgs::MISSED;
      obs->OnBeginFrame(missed_args);
    }
  }
}

void ExternalBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());

  observers_.erase(obs);
  if (observers_.empty())
    client_->OnNeedsBeginFrames(false);
}

bool ExternalBeginFrameSource::IsThrottled() const {
  return true;
}

void ExternalBeginFrameSource::OnSetBeginFrameSourcePaused(bool paused) {
  if (paused_ == paused)
    return;
  paused_ = paused;
  std::unordered_set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers)
    obs->OnBeginFrameSourcePausedChanged(paused_);
}

void ExternalBeginFrameSource::OnBeginFrame(const BeginFrameArgs& args) {
  // Ignore out of order begin frames because of layer tree frame sink being
  // recreated.
  if (last_begin_frame_args_.IsValid() &&
      (args.frame_time <= last_begin_frame_args_.frame_time ||
       (args.source_id == last_begin_frame_args_.source_id &&
        args.sequence_number <= last_begin_frame_args_.sequence_number)))
    return;

  last_begin_frame_args_ = args;
  std::unordered_set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers) {
    // It is possible that the source in which |args| originate changes, or that
    // our hookup to this source changes, so we have to check for continuity.
    // See also https://crbug.com/690127 for what may happen without this check.
    const BeginFrameArgs& last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() || (args.frame_time > last_args.frame_time)) {
      DCHECK((args.source_id != last_args.source_id) ||
             (args.sequence_number > last_args.sequence_number))
          << "current " << args.AsValue()->ToString() << ", last "
          << last_args.AsValue()->ToString();
      obs->OnBeginFrame(args);
    }
  }
}

}  // namespace viz
