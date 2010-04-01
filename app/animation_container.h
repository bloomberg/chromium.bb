// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_ANIMATION_CONTAINER_H_
#define APP_ANIMATION_CONTAINER_H_

#include <set>

#include "base/ref_counted.h"
#include "base/time.h"
#include "base/timer.h"

class Animation;

// AnimationContainer is used by Animation to manage the underlying timer.
// Internally each Animation creates a single AnimationContainer. You can
// group a set of Animations into the same AnimationContainer by way of
// Animation::SetContainer. Grouping a set of Animations into the same
// AnimationContainer ensures they all update and start at the same time.
//
// AnimationContainer is ref counted. Each Animation contained within the
// AnimationContainer own it.
class AnimationContainer : public base::RefCounted<AnimationContainer> {
 public:
  // The observer is notified after every update of the animations managed by
  // the container.
  class Observer {
   public:
    // Invoked on every tick of the timer managed by the container and after
    // all the animations have updated.
    virtual void AnimationContainerProgressed(
        AnimationContainer* container) = 0;

    // Invoked when no more animations are being managed by this container.
    virtual void AnimationContainerEmpty(AnimationContainer* container) = 0;
  };

  AnimationContainer();

  void set_observer(Observer* observer) { observer_ = observer; }

  // The time the last animation ran at.
  base::TimeTicks last_tick_time() const { return last_tick_time_; }

  // Are there any timers running?
  bool is_running() const { return !animations_.empty(); }

 private:
  friend class Animation;
  friend class base::RefCounted<AnimationContainer>;

  typedef std::set<Animation*> Animations;

  ~AnimationContainer();

  // Invoked by Animation when it needs to start. Starts the timer if necessary.
  void Start(Animation* animation);

  // Invoked by Animation when it needs to stop. If there are no more animations
  // running the timer stops.
  void Stop(Animation* animation);

  // Timer callback method.
  void Run();

  // Sets min_timer_interval_ and restarts the timer.
  void SetMinTimerInterval(base::TimeDelta delta);

  // Returns the min timer interval of all the timers.
  base::TimeDelta GetMinInterval();

  // Represents one of two possible values:
  // . If only a single animation has been started and the timer hasn't yet
  //   fired this is the time the animation was added.
  // . The time the last animation ran at (::Run was invoked).
  base::TimeTicks last_tick_time_;

  // Set of animations being managed.
  Animations animations_;

  // Minimum interval the timers run at.
  base::TimeDelta min_timer_interval_;

  base::RepeatingTimer<AnimationContainer> timer_;

  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(AnimationContainer);
};

#endif  // APP_ANIMATION_CONTAINER_H_
