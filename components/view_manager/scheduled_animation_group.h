// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SCHEDULED_ANIMATION_GROUP_H_
#define COMPONENTS_VIEW_MANAGER_SCHEDULED_ANIMATION_GROUP_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/view_manager/public/interfaces/animations.mojom.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform.h"

namespace view_manager {

class ServerView;

struct ScheduledAnimationValue {
  ScheduledAnimationValue();
  ~ScheduledAnimationValue();

  float float_value;
  gfx::Transform transform;
};

struct ScheduledAnimationElement {
  ScheduledAnimationElement();
  ~ScheduledAnimationElement();

  mojo::AnimationProperty property;
  base::TimeDelta duration;
  gfx::Tween::Type tween_type;
  bool is_start_valid;
  ScheduledAnimationValue start_value;
  ScheduledAnimationValue target_value;
  // Start time is based on scheduled time and relative to any other elements
  // in the sequence.
  base::TimeTicks start_time;
};

struct ScheduledAnimationSequence {
  ScheduledAnimationSequence();
  ~ScheduledAnimationSequence();

  bool run_until_stopped;
  std::vector<ScheduledAnimationElement> elements;

  // Sum of the duration of all elements. This does not take into account
  // |cycle_count|.
  base::TimeDelta duration;

  // The following values are updated as the animation progresses.

  // Number of cycles remaining. This is only used if |run_until_stopped| is
  // false.
  uint32_t cycle_count;

  // Index into |elements| of the element currently animating.
  size_t current_index;
};

// Corresponds to a mojo::AnimationGroup and is responsible for running the
// actual animation.
class ScheduledAnimationGroup {
 public:
  ~ScheduledAnimationGroup();

  // Returns a new ScheduledAnimationGroup from the supplied parameters, or
  // null if |transport_group| isn't valid.
  static scoped_ptr<ScheduledAnimationGroup> Create(
      ServerView* view,
      base::TimeTicks now,
      uint32_t id,
      const mojo::AnimationGroup& transport_group);

  uint32_t id() const { return id_; }

  // Gets the start value for any elements that don't have an explicit start.
  // value.
  void ObtainStartValues();

  // Sets the values of any properties that are not in |other| to their final
  // value.
  void SetValuesToTargetValuesForPropertiesNotIn(
      const ScheduledAnimationGroup& other);

  // Advances the group. |time| is the current time. Returns true if the group
  // is done (nothing left to animate).
  bool Tick(base::TimeTicks time);

  ServerView* view() { return view_; }

 private:
  ScheduledAnimationGroup(ServerView* view,
                          uint32_t id,
                          base::TimeTicks time_scheduled);

  ServerView* view_;
  const uint32_t id_;
  base::TimeTicks time_scheduled_;
  std::vector<ScheduledAnimationSequence> sequences_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledAnimationGroup);
};

}  // namespace view_manager

#endif  // COMPONENTS_VIEW_MANAGER_SCHEDULED_ANIMATION_GROUP_H_
