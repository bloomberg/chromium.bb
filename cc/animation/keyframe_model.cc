// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframe_model.h"

#include <cmath>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "cc/animation/animation_curve.h"

namespace {

// This should match the RunState enum.
static const char* const s_runStateNames[] = {"WAITING_FOR_TARGET_AVAILABILITY",
                                              "WAITING_FOR_DELETION",
                                              "STARTING",
                                              "RUNNING",
                                              "PAUSED",
                                              "FINISHED",
                                              "ABORTED",
                                              "ABORTED_BUT_NEEDS_COMPLETION"};

static_assert(static_cast<int>(cc::KeyframeModel::LAST_RUN_STATE) + 1 ==
                  arraysize(s_runStateNames),
              "RunStateEnumSize should equal the number of elements in "
              "s_runStateNames");

static const char* const s_curveTypeNames[] = {
    "COLOR", "FLOAT", "TRANSFORM", "FILTER", "SCROLL_OFFSET", "SIZE"};

static_assert(static_cast<int>(cc::AnimationCurve::LAST_CURVE_TYPE) + 1 ==
                  arraysize(s_curveTypeNames),
              "CurveType enum should equal the number of elements in "
              "s_runStateNames");

}  // namespace

namespace cc {

std::string KeyframeModel::ToString(RunState state) {
  return s_runStateNames[state];
}

std::unique_ptr<KeyframeModel> KeyframeModel::Create(
    std::unique_ptr<AnimationCurve> curve,
    int keyframe_model_id,
    int group_id,
    int target_property_id) {
  return base::WrapUnique(new KeyframeModel(std::move(curve), keyframe_model_id,
                                            group_id, target_property_id));
}

std::unique_ptr<KeyframeModel> KeyframeModel::CreateImplInstance(
    RunState initial_run_state) const {
  // Should never clone a model that is the controlling instance as it ends up
  // creating multiple controlling instances.
  DCHECK(!is_controlling_instance_);
  std::unique_ptr<KeyframeModel> to_return(
      new KeyframeModel(curve_->Clone(), id_, group_, target_property_id_));
  to_return->element_id_ = element_id_;
  to_return->run_state_ = initial_run_state;
  to_return->iterations_ = iterations_;
  to_return->iteration_start_ = iteration_start_;
  to_return->start_time_ = start_time_;
  to_return->pause_time_ = pause_time_;
  to_return->total_paused_duration_ = total_paused_duration_;
  to_return->time_offset_ = time_offset_;
  to_return->direction_ = direction_;
  to_return->playback_rate_ = playback_rate_;
  to_return->fill_mode_ = fill_mode_;
  DCHECK(!to_return->is_controlling_instance_);
  to_return->is_controlling_instance_ = true;
  return to_return;
}

KeyframeModel::KeyframeModel(std::unique_ptr<AnimationCurve> curve,
                             int keyframe_model_id,
                             int group_id,
                             int target_property_id)
    : curve_(std::move(curve)),
      id_(keyframe_model_id),
      group_(group_id),
      target_property_id_(target_property_id),
      run_state_(WAITING_FOR_TARGET_AVAILABILITY),
      iterations_(1),
      iteration_start_(0),
      direction_(Direction::NORMAL),
      playback_rate_(1),
      fill_mode_(FillMode::BOTH),
      needs_synchronized_start_time_(false),
      received_finished_event_(false),
      is_controlling_instance_(false),
      is_impl_only_(false),
      affects_active_elements_(true),
      affects_pending_elements_(true) {}

KeyframeModel::~KeyframeModel() {
  if (run_state_ == RUNNING || run_state_ == PAUSED)
    SetRunState(ABORTED, base::TimeTicks());
}

void KeyframeModel::SetRunState(RunState run_state,
                                base::TimeTicks monotonic_time) {
  char name_buffer[256];
  base::snprintf(name_buffer, sizeof(name_buffer), "%s-%d-%d",
                 s_curveTypeNames[curve_->Type()], target_property_id_, group_);

  bool is_waiting_to_start =
      run_state_ == WAITING_FOR_TARGET_AVAILABILITY || run_state_ == STARTING;

  if (is_controlling_instance_ && is_waiting_to_start && run_state == RUNNING) {
    TRACE_EVENT_ASYNC_BEGIN1("cc", "KeyframeModel", this, "Name",
                             TRACE_STR_COPY(name_buffer));
  }

  bool was_finished = is_finished();

  const char* old_run_state_name = s_runStateNames[run_state_];

  if (run_state == RUNNING && run_state_ == PAUSED)
    total_paused_duration_ += (monotonic_time - pause_time_);
  else if (run_state == PAUSED)
    pause_time_ = monotonic_time;
  run_state_ = run_state;

  const char* new_run_state_name = s_runStateNames[run_state];

  if (is_controlling_instance_ && !was_finished && is_finished())
    TRACE_EVENT_ASYNC_END0("cc", "KeyframeModel", this);

  char state_buffer[256];
  base::snprintf(state_buffer, sizeof(state_buffer), "%s->%s",
                 old_run_state_name, new_run_state_name);

  TRACE_EVENT_INSTANT2(
      "cc", "ElementAnimations::SetRunState", TRACE_EVENT_SCOPE_THREAD, "Name",
      TRACE_STR_COPY(name_buffer), "State", TRACE_STR_COPY(state_buffer));
}

void KeyframeModel::Pause(base::TimeDelta pause_offset) {
  // Convert pause offset to monotonic time.

  // TODO(crbug.com/840364): This conversion is incorrect. pause_offset is
  // actually a local time so to convert it to monotonic time we should include
  // total_paused_duration_ but exclude time_offset. The current calculation is
  // is incorrect for animations that have start-delay or are paused and
  // unpaused multiple times.
  base::TimeTicks monotonic_time = pause_offset + start_time_ + time_offset_;
  SetRunState(PAUSED, monotonic_time);
}

bool KeyframeModel::IsFinishedAt(base::TimeTicks monotonic_time) const {
  if (is_finished())
    return true;

  if (needs_synchronized_start_time_)
    return false;

  if (playback_rate_ == 0)
    return false;

  return run_state_ == RUNNING && iterations_ >= 0 &&
         (curve_->Duration() * (iterations_ / std::abs(playback_rate_))) <=
             (ConvertMonotonicTimeToLocalTime(monotonic_time) + time_offset_);
}

bool KeyframeModel::InEffect(base::TimeTicks monotonic_time) const {
  return ConvertMonotonicTimeToLocalTime(monotonic_time) + time_offset_ >=
             base::TimeDelta() ||
         (fill_mode_ == FillMode::BOTH || fill_mode_ == FillMode::BACKWARDS);
}

base::TimeDelta KeyframeModel::ConvertMonotonicTimeToLocalTime(
    base::TimeTicks monotonic_time) const {
  // When waiting on receiving a start time, then our global clock is 'stuck' at
  // the initial state.
  if ((run_state_ == STARTING && !has_set_start_time()) ||
      needs_synchronized_start_time())
    return base::TimeDelta();

  // If we're paused, time is 'stuck' at the pause time.
  base::TimeTicks time =
      (run_state_ == PAUSED) ? pause_time_ - time_offset_ : monotonic_time;
  return time - start_time_ - total_paused_duration_;
}

base::TimeDelta KeyframeModel::TrimTimeToCurrentIteration(
    base::TimeTicks monotonic_time) const {
  base::TimeDelta local_time = ConvertMonotonicTimeToLocalTime(monotonic_time);
  return TrimLocalTimeToCurrentIteration(local_time);
}

base::TimeDelta KeyframeModel::TrimLocalTimeToCurrentIteration(
    base::TimeDelta local_time) const {
  // Check for valid parameters
  DCHECK(playback_rate_);
  DCHECK_GE(iteration_start_, 0);

  base::TimeDelta active_time = local_time + time_offset_;
  base::TimeDelta start_offset = curve_->Duration() * iteration_start_;

  // Return start offset if we are before the start of the keyframe model
  if (active_time < base::TimeDelta())
    return start_offset;
  // Always return zero if we have no iterations.
  if (!iterations_)
    return base::TimeDelta();

  // Don't attempt to trim if we have no duration.
  if (curve_->Duration() <= base::TimeDelta())
    return base::TimeDelta();

  base::TimeDelta repeated_duration = curve_->Duration() * iterations_;
  base::TimeDelta active_duration =
      repeated_duration / std::abs(playback_rate_);

  // Check if we are past active duration
  if (iterations_ > 0 && active_time >= active_duration)
    active_time = active_duration;

  // Calculate the scaled active time
  base::TimeDelta scaled_active_time;
  if (playback_rate_ < 0) {
    scaled_active_time =
        ((active_time - active_duration) * playback_rate_) + start_offset;
  } else {
    scaled_active_time = (active_time * playback_rate_) + start_offset;
  }

  // Calculate the iteration time
  base::TimeDelta iteration_time;
  if (scaled_active_time - start_offset == repeated_duration &&
      fmod(iterations_ + iteration_start_, 1) == 0)
    iteration_time = curve_->Duration();
  else
    iteration_time = scaled_active_time % curve_->Duration();

  // Calculate the current iteration
  int iteration;
  if (scaled_active_time <= base::TimeDelta())
    iteration = 0;
  else if (iteration_time == curve_->Duration())
    iteration = ceil(iteration_start_ + iterations_ - 1);
  else
    iteration = static_cast<int>(scaled_active_time / curve_->Duration());

  // Check if we are running the keyframe model in reverse direction for the
  // current iteration
  bool reverse =
      (direction_ == Direction::REVERSE) ||
      (direction_ == Direction::ALTERNATE_NORMAL && iteration % 2 == 1) ||
      (direction_ == Direction::ALTERNATE_REVERSE && iteration % 2 == 0);

  // If we are running the keyframe model in reverse direction, reverse the
  // result
  if (reverse)
    iteration_time = curve_->Duration() - iteration_time;

  return iteration_time;
}

void KeyframeModel::PushPropertiesTo(KeyframeModel* other) const {
  other->element_id_ = element_id_;
  if (run_state_ == KeyframeModel::PAUSED ||
      other->run_state_ == KeyframeModel::PAUSED) {
    other->run_state_ = run_state_;
    other->pause_time_ = pause_time_;
    other->total_paused_duration_ = total_paused_duration_;
  }
}

std::string KeyframeModel::ToString() const {
  return base::StringPrintf(
      "KeyframeModel{id=%d, group=%d, target_property_id=%d, "
      "run_state=%s}",
      id_, group_, target_property_id_,
      KeyframeModel::ToString(run_state_).c_str());
}

void KeyframeModel::SetIsImplOnly() {
  is_impl_only_ = true;
  // Impl only animations have a single instance which by definition is the
  // controlling instance.
  is_controlling_instance_ = true;
}
}  // namespace cc
