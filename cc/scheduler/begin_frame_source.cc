// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/begin_frame_source.h"

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
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/scheduler/scheduler.h"

namespace cc {

namespace {
// kDoubleTickDivisor prevents the SyntheticBFS from sending BeginFrames too
// often to an observer.
static const double kDoubleTickDivisor = 2.0;
}

// BeginFrameObserverBase -------------------------------------------------
BeginFrameObserverBase::BeginFrameObserverBase()
    : last_begin_frame_args_(), dropped_begin_frame_args_(0) {
}

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

// BeginFrameSource -------------------------------------------------------
namespace {
static base::StaticAtomicSequenceNumber g_next_source_id;
}  // namespace

BeginFrameSource::BeginFrameSource() : source_id_(g_next_source_id.GetNext()) {}

uint32_t BeginFrameSource::source_id() const {
  return source_id_;
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

void BackToBackBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs,
                                                const BeginFrameAck& ack) {
  if (ack.remaining_frames == 0 && observers_.find(obs) != observers_.end()) {
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
    base::TimeTicks frame_time,
    BeginFrameArgs::BeginFrameArgsType type) {
  uint64_t sequence_number = next_sequence_number_++;
  return BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), sequence_number, frame_time,
      time_source_->NextTickTime(), time_source_->Interval(), type);
}

void DelayBasedBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(false);
  time_source_->SetActive(true);

  // Missed args should correspond to |current_begin_frame_args_| (particularly,
  // have the same sequence number) if |current_begin_frame_args_| still
  // correspond to the last time the time source should have ticked. This may
  // not be the case if OnTimerTick() has never run yet, the time source was
  // inactive before AddObserver() was called, or the interval changed. In such
  // a case, we create new args with a new sequence number.
  base::TimeTicks last_or_missed_tick_time =
      time_source_->NextTickTime() - time_source_->Interval();
  if (current_begin_frame_args_.IsValid() &&
      current_begin_frame_args_.frame_time == last_or_missed_tick_time &&
      current_begin_frame_args_.interval == time_source_->Interval()) {
    // Ensure that the args have the right type.
    current_begin_frame_args_.type = BeginFrameArgs::MISSED;
  } else {
    // The args are not up to date and we need to create new ones with the
    // missed tick's time and a new sequence number.
    current_begin_frame_args_ =
        CreateBeginFrameArgs(last_or_missed_tick_time, BeginFrameArgs::MISSED);
  }

  BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
  if (!last_args.IsValid() ||
      (current_begin_frame_args_.frame_time >
       last_args.frame_time +
           current_begin_frame_args_.interval / kDoubleTickDivisor)) {
    DCHECK(current_begin_frame_args_.sequence_number >
               last_args.sequence_number ||
           current_begin_frame_args_.source_id != last_args.source_id)
        << "current " << current_begin_frame_args_.AsValue()->ToString()
        << ", last " << last_args.AsValue()->ToString();
    obs->OnBeginFrame(current_begin_frame_args_);
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
  current_begin_frame_args_ = CreateBeginFrameArgs(time_source_->LastTickTime(),
                                                   BeginFrameArgs::NORMAL);
  std::unordered_set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers) {
    BeginFrameArgs last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        (current_begin_frame_args_.frame_time >
         last_args.frame_time +
             current_begin_frame_args_.interval / kDoubleTickDivisor)) {
      obs->OnBeginFrame(current_begin_frame_args_);
    }
  }
}

// BeginFrameObserverAckTracker -------------------------------------------
BeginFrameObserverAckTracker::BeginFrameObserverAckTracker()
    : current_source_id_(0),
      current_sequence_number_(BeginFrameArgs::kStartingFrameNumber),
      observers_had_damage_(false) {}

BeginFrameObserverAckTracker::~BeginFrameObserverAckTracker() {}

void BeginFrameObserverAckTracker::OnBeginFrame(const BeginFrameArgs& args) {
  if (current_source_id_ != args.source_id)
    SourceChanged(args);

  DCHECK_GE(args.sequence_number, current_sequence_number_);
  // Reset for new BeginFrame.
  current_sequence_number_ = args.sequence_number;
  finished_observers_.clear();
  observers_had_damage_ = false;
}

void BeginFrameObserverAckTracker::SourceChanged(const BeginFrameArgs& args) {
  current_source_id_ = args.source_id;
  current_sequence_number_ = args.sequence_number;

  // Mark all observers invalid: We report an invalid frame until every observer
  // has confirmed the frame.
  for (auto& entry : latest_confirmed_sequence_numbers_)
    entry.second = BeginFrameArgs::kInvalidFrameNumber;
}

void BeginFrameObserverAckTracker::OnObserverFinishedFrame(
    BeginFrameObserver* obs,
    const BeginFrameAck& ack) {
  if (ack.source_id != current_source_id_)
    return;

  DCHECK_LE(ack.sequence_number, current_sequence_number_);
  if (ack.sequence_number != current_sequence_number_)
    return;

  finished_observers_.insert(obs);
  observers_had_damage_ |= ack.has_damage;

  // We max() with the current value in |latest_confirmed_sequence_numbers_| to
  // handle situations where an observer just started observing (again) and may
  // acknowledge with an ancient latest_confirmed_sequence_number.
  latest_confirmed_sequence_numbers_[obs] =
      std::max(ack.latest_confirmed_sequence_number,
               latest_confirmed_sequence_numbers_[obs]);
}

void BeginFrameObserverAckTracker::OnObserverAdded(BeginFrameObserver* obs) {
  observers_.insert(obs);

  // Since the observer didn't want BeginFrames before, we consider it
  // up-to-date up to the last BeginFrame, except if it already handled the
  // current BeginFrame. In which case, we consider it up-to-date up to the
  // current one.
  DCHECK_LT(BeginFrameArgs::kInvalidFrameNumber, current_sequence_number_);
  const BeginFrameArgs& last_args = obs->LastUsedBeginFrameArgs();
  if (last_args.IsValid() &&
      last_args.sequence_number == current_sequence_number_ &&
      last_args.source_id == current_source_id_) {
    latest_confirmed_sequence_numbers_[obs] = current_sequence_number_;
    finished_observers_.insert(obs);
  } else {
    latest_confirmed_sequence_numbers_[obs] = current_sequence_number_ - 1;
  }
}

void BeginFrameObserverAckTracker::OnObserverRemoved(BeginFrameObserver* obs) {
  observers_.erase(obs);
  finished_observers_.erase(obs);
  latest_confirmed_sequence_numbers_.erase(obs);
}

bool BeginFrameObserverAckTracker::AllObserversFinishedFrame() const {
  if (finished_observers_.size() < observers_.size())
    return false;
  return base::STLIncludes(finished_observers_, observers_);
}

bool BeginFrameObserverAckTracker::AnyObserversHadDamage() const {
  return observers_had_damage_;
}

uint64_t BeginFrameObserverAckTracker::LatestConfirmedSequenceNumber() const {
  uint64_t latest_confirmed_sequence_number = current_sequence_number_;
  for (const auto& entry : latest_confirmed_sequence_numbers_) {
    latest_confirmed_sequence_number =
        std::min(latest_confirmed_sequence_number, entry.second);
  }
  return latest_confirmed_sequence_number;
}

// ExternalBeginFrameSource -----------------------------------------------
ExternalBeginFrameSource::ExternalBeginFrameSource(
    ExternalBeginFrameSourceClient* client)
    : client_(client) {
  DCHECK(client_);
}

ExternalBeginFrameSource::~ExternalBeginFrameSource() = default;

void ExternalBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  bool observers_was_empty = observers_.empty();
  observers_.insert(obs);
  ack_tracker_.OnObserverAdded(obs);
  obs->OnBeginFrameSourcePausedChanged(paused_);
  if (observers_was_empty)
    client_->OnNeedsBeginFrames(true);

  // Send a MISSED begin frame if necessary.
  if (missed_begin_frame_args_.IsValid()) {
    const BeginFrameArgs& last_args = obs->LastUsedBeginFrameArgs();
    if (!last_args.IsValid() ||
        (missed_begin_frame_args_.frame_time > last_args.frame_time)) {
      DCHECK((missed_begin_frame_args_.source_id != last_args.source_id) ||
             (missed_begin_frame_args_.sequence_number >
              last_args.sequence_number))
          << "current " << missed_begin_frame_args_.AsValue()->ToString()
          << ", last " << last_args.AsValue()->ToString();
      obs->OnBeginFrame(missed_begin_frame_args_);
    }
  }
}

void ExternalBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());

  observers_.erase(obs);
  ack_tracker_.OnObserverRemoved(obs);
  MaybeFinishFrame();
  if (observers_.empty()) {
    missed_begin_frame_args_ = BeginFrameArgs();
    client_->OnNeedsBeginFrames(false);
  }
}

void ExternalBeginFrameSource::DidFinishFrame(BeginFrameObserver* obs,
                                              const BeginFrameAck& ack) {
  ack_tracker_.OnObserverFinishedFrame(obs, ack);
  MaybeFinishFrame();
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
  if (frame_active_)
    FinishFrame();

  frame_active_ = true;
  missed_begin_frame_args_ = args;
  missed_begin_frame_args_.type = BeginFrameArgs::MISSED;
  ack_tracker_.OnBeginFrame(args);
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
  MaybeFinishFrame();
}

void ExternalBeginFrameSource::MaybeFinishFrame() {
  if (!frame_active_ || !ack_tracker_.AllObserversFinishedFrame())
    return;
  FinishFrame();
}

void ExternalBeginFrameSource::FinishFrame() {
  frame_active_ = false;

  BeginFrameAck ack(missed_begin_frame_args_.source_id,
                    missed_begin_frame_args_.sequence_number,
                    ack_tracker_.LatestConfirmedSequenceNumber(), 0,
                    ack_tracker_.AnyObserversHadDamage());
  client_->OnDidFinishFrame(ack);
}

}  // namespace cc
