// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Inspired by NSAnimation

#ifndef APP_ANIMATION_H_
#define APP_ANIMATION_H_

#include "base/ref_counted.h"
#include "base/time.h"

class Animation;
class AnimationContainer;

namespace gfx {
class Rect;
}

// AnimationDelegate
//
//  Implement this interface when you want to receive notifications about the
//  state of an animation.
//
class AnimationDelegate {
 public:
  // Called when an animation has started.
  virtual void AnimationStarted(const Animation* animation) {
  }

  // Called when an animation has completed.
  virtual void AnimationEnded(const Animation* animation) {
  }

  // Called when an animation has progressed.
  virtual void AnimationProgressed(const Animation* animation) {
  }

  // Called when an animation has been canceled.
  virtual void AnimationCanceled(const Animation* animation) {
  }

 protected:
  virtual ~AnimationDelegate() {}
};

// Animation
//
//  This class provides a basic implementation of an object that uses a timer
//  to increment its state over the specified time and frame-rate. To
//  actually do something useful with this you need to subclass it and override
//  AnimateToState and optionally GetCurrentValue to update your state.
//
//  The Animation notifies a delegate when events of interest occur.
//
//  The practice is to instantiate a subclass and call Init and any other
//  initialization specific to the subclass, and then call |Start|. The
//  animation uses the current thread's message loop.
//
class Animation {
 public:
  // Initializes everything except the duration.
  //
  // Caller must make sure to call SetDuration() if they use this
  // constructor; it is preferable to use the full one, but sometimes
  // duration can change between calls to Start() and we need to
  // expose this interface.
  Animation(int frame_rate, AnimationDelegate* delegate);

  // Initializes all fields.
  Animation(int duration, int frame_rate, AnimationDelegate* delegate);
  virtual ~Animation();

  // Called when the animation progresses. Subclasses override this to
  // efficiently update their state.
  virtual void AnimateToState(double state) = 0;

  // Gets the value for the current state, according to the animation
  // curve in use. This class provides only for a linear relationship,
  // however subclasses can override this to provide others.
  virtual double GetCurrentValue() const;

  // Convenience for returning a value between |start| and |target| based on
  // the current value. This is (target - start) * GetCurrentValue() + start.
  double CurrentValueBetween(double start, double target) const;
  int CurrentValueBetween(int start, int target) const;
  gfx::Rect CurrentValueBetween(const gfx::Rect& start_bounds,
                                const gfx::Rect& target_bounds) const;

  // Start the animation.
  void Start();

  // Stop the animation.
  void Stop();

  // Skip to the end of the current animation.
  void End();

  // Return whether this animation is animating.
  bool IsAnimating() const;

  // Changes the length of the animation. This resets the current
  // state of the animation to the beginning.
  void SetDuration(int duration);

  // Returns true if rich animations should be rendered.
  // Looks at session type (e.g. remote desktop) and accessibility settings
  // to give guidance for heavy animations such as "start download" arrow.
  static bool ShouldRenderRichAnimation();

  // Sets the delegate.
  void set_delegate(AnimationDelegate* delegate) { delegate_ = delegate; }

  // Sets the container used to manage the timer. A value of NULL results in
  // creating a new AnimationContainer.
  void SetContainer(AnimationContainer* container);

  base::TimeDelta timer_interval() const { return timer_interval_; }

 protected:
  // Invoked by the AnimationContainer when the animation is running to advance
  // the animation. Use |time_now| rather than Time::Now to avoid multiple
  // animations running at the same time diverging.
  virtual void Step(base::TimeTicks time_now);

  // Calculates the timer interval from the constructor list.
  base::TimeDelta CalculateInterval(int frame_rate);

 private:
  friend class AnimationContainer;

  // Invoked from AnimationContainer when started.
  void set_start_time(base::TimeTicks start_time) { start_time_ = start_time; }

  // Whether or not we are currently animating.
  bool animating_;

  int frame_rate_;
  base::TimeDelta timer_interval_;
  base::TimeDelta duration_;

  // Current state, on a scale from 0.0 to 1.0.
  double state_;

  base::TimeTicks start_time_;

  AnimationDelegate* delegate_;

  scoped_refptr<AnimationContainer> container_;

  DISALLOW_COPY_AND_ASSIGN(Animation);
};

#endif  // APP_ANIMATION_H_
