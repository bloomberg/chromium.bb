// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_H_
#define CC_ANIMATION_ANIMATION_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"

namespace cc {

class AnimationCurve;

// An Animation, contains all the state required to play an AnimationCurve.
// Specifically, the affected property, the run state (paused, finished, etc.),
// loop count, last pause time, and the total time spent paused.
class CC_EXPORT Animation {
 public:
  // Animations begin in one of the 'waiting' states. Animations waiting for the
  // next tick will start the next time the controller animates. Animations
  // waiting for target availibility will run as soon as their target property
  // is free (and all the animations animating with it are also able to run).
  // Animations waiting for their start time to come have be scheduled to run at
  // a particular point in time. When this time arrives, the controller will
  // move the animations into the Starting state, and then into the Running
  // state. Running animations may toggle between Running and Paused, and may be
  // stopped by moving into either the Aborted or Finished states. A Finished
  // animation was allowed to run to completion, but an Aborted animation was
  // not.
  enum RunState {
    WaitingForNextTick = 0,
    WaitingForTargetAvailability,
    WaitingForStartTime,
    WaitingForDeletion,
    Starting,
    Running,
    Paused,
    Finished,
    Aborted,
    // This sentinel must be last.
    RunStateEnumSize
  };

  enum TargetProperty {
    Transform = 0,
    Opacity,
    // This sentinel must be last.
    TargetPropertyEnumSize
  };

  static scoped_ptr<Animation> Create(scoped_ptr<AnimationCurve> curve,
                                      int animation_id,
                                      int group_id,
                                      TargetProperty target_property);

  virtual ~Animation();

  int id() const { return id_; }
  int group() const { return group_; }
  TargetProperty target_property() const { return target_property_; }

  RunState run_state() const { return run_state_; }
  void SetRunState(RunState run_state, double monotonic_time);

  // This is the number of times that the animation will play. If this
  // value is zero the animation will not play. If it is negative, then
  // the animation will loop indefinitely.
  int iterations() const { return iterations_; }
  void set_iterations(int n) { iterations_ = n; }

  double start_time() const { return start_time_; }
  void set_start_time(double monotonic_time) { start_time_ = monotonic_time; }
  bool has_set_start_time() const { return !!start_time_; }

  double time_offset() const { return time_offset_; }
  void set_time_offset(double monotonic_time) { time_offset_ = monotonic_time; }

  void Suspend(double monotonic_time);
  void Resume(double monotonic_time);

  // If alternates_direction is true, on odd numbered iterations we reverse the
  // curve.
  bool alternates_direction() const { return alternates_direction_; }
  void set_alternates_direction(bool alternates) {
    alternates_direction_ = alternates;
  }

  bool IsFinishedAt(double monotonic_time) const;
  bool is_finished() const {
    return run_state_ == Finished ||
        run_state_ == Aborted ||
        run_state_ == WaitingForDeletion;
  }

  AnimationCurve* curve() { return curve_.get(); }
  const AnimationCurve* curve() const { return curve_.get(); }

  // If this is true, even if the animation is running, it will not be tickable
  // until it is given a start time. This is true for animations running on the
  // main thread.
  bool needs_synchronized_start_time() const {
    return needs_synchronized_start_time_;
  }
  void set_needs_synchronized_start_time(bool needs_synchronized_start_time) {
    needs_synchronized_start_time_ = needs_synchronized_start_time;
  }

  // Takes the given absolute time, and using the start time and the number
  // of iterations, returns the relative time in the current iteration.
  double TrimTimeToCurrentIteration(double monotonic_time) const;

  enum InstanceType {
    ControllingInstance = 0,
    NonControllingInstance
  };

  scoped_ptr<Animation> Clone(InstanceType instance_type) const;
  scoped_ptr<Animation> CloneAndInitialize(InstanceType instance_type,
                                           RunState initial_run_state,
                                           double start_time) const;
  bool is_controlling_instance() const { return is_controlling_instance_; }

  void PushPropertiesTo(Animation* other) const;

  void set_is_impl_only(bool is_impl_only) { is_impl_only_ = is_impl_only; }
  bool is_impl_only() const { return is_impl_only_; }

 private:
  Animation(scoped_ptr<AnimationCurve> curve,
            int animation_id,
            int group_id,
            TargetProperty target_property);

  scoped_ptr<AnimationCurve> curve_;

  // IDs are not necessarily unique.
  int id_;

  // Animations that must be run together are called 'grouped' and have the same
  // group id. Grouped animations are guaranteed to start at the same time and
  // no other animations may animate any of the group's target properties until
  // all animations in the group have finished animating. Note: an active
  // animation's group id and target property uniquely identify that animation.
  int group_;

  TargetProperty target_property_;
  RunState run_state_;
  int iterations_;
  double start_time_;
  bool alternates_direction_;

  // The time offset effectively pushes the start of the animation back in time.
  // This is used for resuming paused animations -- an animation is added with a
  // non-zero time offset, causing the animation to skip ahead to the desired
  // point in time.
  double time_offset_;

  bool needs_synchronized_start_time_;

  // When an animation is suspended, it behaves as if it is paused and it also
  // ignores all run state changes until it is resumed. This is used for testing
  // purposes.
  bool suspended_;

  // These are used in TrimTimeToCurrentIteration to account for time
  // spent while paused. This is not included in AnimationState since it
  // there is absolutely no need for clients of this controller to know
  // about these values.
  double pause_time_;
  double total_paused_time_;

  // Animations lead dual lives. An active animation will be conceptually owned
  // by two controllers, one on the impl thread and one on the main. In reality,
  // there will be two separate Animation instances for the same animation. They
  // will have the same group id and the same target property (these two values
  // uniquely identify an animation). The instance on the impl thread is the
  // instance that ultimately controls the values of the animating layer and so
  // we will refer to it as the 'controlling instance'.
  bool is_controlling_instance_;

  bool is_impl_only_;

  DISALLOW_COPY_AND_ASSIGN(Animation);
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_H_
