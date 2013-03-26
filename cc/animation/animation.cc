// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation.h"

#include <cmath>

#include "base/debug/trace_event.h"
#include "base/string_util.h"
#include "cc/animation/animation_curve.h"

namespace {

// This should match the RunState enum.
static const char* const s_runStateNames[] = {
  "WaitingForNextTick",
  "WaitingForTargetAvailability",
  "WaitingForStartTime",
  "WaitingForDeletion",
  "Starting",
  "Running",
  "Paused",
  "Finished",
  "Aborted"
};

COMPILE_ASSERT(static_cast<int>(cc::Animation::RunStateEnumSize) ==
               arraysize(s_runStateNames),
               RunState_names_match_enum);

// This should match the TargetProperty enum.
static const char* const s_targetPropertyNames[] = {
  "Transform",
  "Opacity"
};

COMPILE_ASSERT(static_cast<int>(cc::Animation::TargetPropertyEnumSize) ==
               arraysize(s_targetPropertyNames),
               TargetProperty_names_match_enum);

}  // namespace

namespace cc {

scoped_ptr<Animation> Animation::Create(
    scoped_ptr<AnimationCurve> curve,
    int animation_id,
    int group_id,
    TargetProperty target_property) {
  return make_scoped_ptr(new Animation(curve.Pass(),
                                       animation_id,
                                       group_id,
                                       target_property)); }

Animation::Animation(scoped_ptr<AnimationCurve> curve,
                     int animation_id,
                     int group_id,
                     TargetProperty target_property)
    : curve_(curve.Pass()),
      id_(animation_id),
      group_(group_id),
      target_property_(target_property),
      run_state_(WaitingForTargetAvailability),
      iterations_(1),
      start_time_(0),
      alternates_direction_(false),
      time_offset_(0),
      needs_synchronized_start_time_(false),
      suspended_(false),
      pause_time_(0),
      total_paused_time_(0),
      is_controlling_instance_(false),
      is_impl_only_(false) {}

Animation::~Animation() {
  if (run_state_ == Running || run_state_ == Paused)
    SetRunState(Aborted, 0);
}

void Animation::SetRunState(RunState run_state, double monotonic_time) {
  if (suspended_)
    return;

  char name_buffer[256];
  base::snprintf(name_buffer,
                 sizeof(name_buffer),
                 "%s-%d%s",
                 s_targetPropertyNames[target_property_],
                 group_,
                 is_controlling_instance_ ? "(impl)" : "");

  bool is_waiting_to_start = run_state_ == WaitingForNextTick ||
                             run_state_ == WaitingForTargetAvailability ||
                             run_state_ == WaitingForStartTime ||
                             run_state_ == Starting;

  if (is_waiting_to_start && run_state == Running) {
    TRACE_EVENT_ASYNC_BEGIN1(
        "cc", "Animation", this, "Name", TRACE_STR_COPY(name_buffer));
  }

  bool was_finished = is_finished();

  const char* old_run_state_name = s_runStateNames[run_state_];

  if (run_state == Running && run_state_ == Paused)
    total_paused_time_ += monotonic_time - pause_time_;
  else if (run_state == Paused)
    pause_time_ = monotonic_time;
  run_state_ = run_state;

  const char* new_run_state_name = s_runStateNames[run_state];

  if (!was_finished && is_finished())
    TRACE_EVENT_ASYNC_END0("cc", "Animation", this);

  char state_buffer[256];
  base::snprintf(state_buffer,
                 sizeof(state_buffer),
                 "%s->%s",
                 old_run_state_name,
                 new_run_state_name);

  TRACE_EVENT_INSTANT2("cc",
                       "LayerAnimationController::SetRunState",
                       TRACE_EVENT_SCOPE_THREAD,
                       "Name",
                       TRACE_STR_COPY(name_buffer),
                       "State",
                       TRACE_STR_COPY(state_buffer));
}

void Animation::Suspend(double monotonic_time) {
  SetRunState(Paused, monotonic_time);
  suspended_ = true;
}

void Animation::Resume(double monotonic_time) {
  suspended_ = false;
  SetRunState(Running, monotonic_time);
}

bool Animation::IsFinishedAt(double monotonic_time) const {
  if (is_finished())
    return true;

  if (needs_synchronized_start_time_)
    return false;

  return run_state_ == Running &&
         iterations_ >= 0 &&
         iterations_ * curve_->Duration() <= (monotonic_time -
                                              start_time() -
                                              total_paused_time_);
}

double Animation::TrimTimeToCurrentIteration(double monotonic_time) const {
  double trimmed = monotonic_time + time_offset_;

  // If we're paused, time is 'stuck' at the pause time.
  if (run_state_ == Paused)
    trimmed = pause_time_;

  // Returned time should always be relative to the start time and should
  // subtract all time spent paused.
  trimmed -= start_time_ + total_paused_time_;

  // Zero is always the start of the animation.
  if (trimmed <= 0)
    return 0;

  // Always return zero if we have no iterations.
  if (!iterations_)
    return 0;

  // Don't attempt to trim if we have no duration.
  if (curve_->Duration() <= 0)
    return 0;

  // If less than an iteration duration, just return trimmed.
  if (trimmed < curve_->Duration())
    return trimmed;

  // If greater than or equal to the total duration, return iteration duration.
  if (iterations_ >= 0 && trimmed >= curve_->Duration() * iterations_) {
    if (alternates_direction_ && !(iterations_ % 2))
      return 0;
    return curve_->Duration();
  }

  // We need to know the current iteration if we're alternating.
  int iteration = static_cast<int>(trimmed / curve_->Duration());

  // Calculate x where trimmed = x + n * curve_->Duration() for some positive
  // integer n.
  trimmed = fmod(trimmed, curve_->Duration());

  // If we're alternating and on an odd iteration, reverse the direction.
  if (alternates_direction_ && iteration % 2 == 1)
    return curve_->Duration() - trimmed;

  return trimmed;
}

scoped_ptr<Animation> Animation::Clone(InstanceType instance_type) const {
  return CloneAndInitialize(instance_type, run_state_, start_time_);
}

scoped_ptr<Animation> Animation::CloneAndInitialize(InstanceType instance_type,
                                                    RunState initial_run_state,
                                                    double start_time) const {
  scoped_ptr<Animation> to_return(
      new Animation(curve_->Clone(), id_, group_, target_property_));
  to_return->run_state_ = initial_run_state;
  to_return->iterations_ = iterations_;
  to_return->start_time_ = start_time;
  to_return->pause_time_ = pause_time_;
  to_return->total_paused_time_ = total_paused_time_;
  to_return->time_offset_ = time_offset_;
  to_return->alternates_direction_ = alternates_direction_;
  to_return->is_controlling_instance_ = instance_type == ControllingInstance;
  return to_return.Pass();
}

void Animation::PushPropertiesTo(Animation* other) const {
  // Currently, we only push changes due to pausing and resuming animations on
  // the main thread.
  if (run_state_ == Animation::Paused ||
      other->run_state_ == Animation::Paused) {
    other->run_state_ = run_state_;
    other->pause_time_ = pause_time_;
    other->total_paused_time_ = total_paused_time_;
  }
}

}  // namespace cc
