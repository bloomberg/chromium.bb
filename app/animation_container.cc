// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/animation_container.h"

#include "app/animation.h"

using base::TimeDelta;
using base::TimeTicks;

AnimationContainer::AnimationContainer()
    : last_tick_time_(TimeTicks::Now()),
      observer_(NULL) {
}

AnimationContainer::~AnimationContainer() {
  // The animations own us and stop themselves before being deleted. If
  // animations_ is not empty, something is wrong.
  DCHECK(animations_.empty());
}

void AnimationContainer::Start(Animation* animation) {
  DCHECK(animations_.count(animation) == 0);  // Start should only be invoked
                                              // if the animation isn't running.

  if (animations_.empty()) {
    last_tick_time_ = TimeTicks::Now();
    SetMinTimerInterval(animation->timer_interval());
  } else if (animation->timer_interval() < min_timer_interval_) {
    SetMinTimerInterval(animation->timer_interval());
  }

  animation->set_start_time(last_tick_time_);
  animations_.insert(animation);
}

void AnimationContainer::Stop(Animation* animation) {
  DCHECK(animations_.count(animation) > 0);  // The animation must be running.

  animations_.erase(animation);

  if (animations_.empty()) {
    timer_.Stop();
    if (observer_)
      observer_->AnimationContainerEmpty(this);
  } else {
    TimeDelta min_timer_interval = GetMinInterval();
    if (min_timer_interval > min_timer_interval_)
      SetMinTimerInterval(min_timer_interval);
  }
}

void AnimationContainer::Run() {
  // We notify the observer after updating all the animations. If all the
  // animations are deleted as a result of updating then our ref count would go
  // to zero and we would be deleted before we notify our observer. We add a
  // reference to ourself here to make sure we're still valid after running all
  // the animations.
  scoped_refptr<AnimationContainer> this_ref(this);

  TimeTicks current_time = TimeTicks::Now();

  last_tick_time_ = current_time;

  // Make a copy of the animations to iterate over so that if any animations
  // are removed as part of invoking Step there aren't any problems.
  Animations animations = animations_;

  for (Animations::const_iterator i = animations.begin();
       i != animations.end(); ++i) {
    // Make sure the animation is still valid.
    if (animations_.find(*i) != animations_.end())
      (*i)->Step(current_time);
  }

  if (observer_)
    observer_->AnimationContainerProgressed(this);
}

void AnimationContainer::SetMinTimerInterval(base::TimeDelta delta) {
  // This doesn't take into account how far along current animation is, but that
  // shouldn't be a problem for uses of Animation/AnimationContainer.
  timer_.Stop();
  min_timer_interval_ = delta;
  timer_.Start(min_timer_interval_, this, &AnimationContainer::Run);
}

TimeDelta AnimationContainer::GetMinInterval() {
  DCHECK(!animations_.empty());

  TimeDelta min;
  Animations::const_iterator i = animations_.begin();
  min = (*i)->timer_interval();
  for (++i; i != animations_.end(); ++i) {
    if ((*i)->timer_interval() < min)
      min = (*i)->timer_interval();
  }
  return min;
}
