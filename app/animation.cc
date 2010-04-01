// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/animation.h"

#include "app/animation_container.h"
#include "gfx/rect.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#endif

using base::Time;
using base::TimeDelta;

Animation::Animation(int frame_rate,
                     AnimationDelegate* delegate)
  : animating_(false),
    frame_rate_(frame_rate),
    timer_interval_(CalculateInterval(frame_rate)),
    state_(0.0),
    delegate_(delegate) {
}

Animation::Animation(int duration,
                     int frame_rate,
                     AnimationDelegate* delegate)
  : animating_(false),
    frame_rate_(frame_rate),
    timer_interval_(CalculateInterval(frame_rate)),
    duration_(TimeDelta::FromMilliseconds(duration)),
    state_(0.0),
    delegate_(delegate) {

  SetDuration(duration);
}

Animation::~Animation() {
  if (animating_)
    container_->Stop(this);
}

double Animation::GetCurrentValue() const {
  // Default is linear relationship, subclass to adapt.
  return state_;
}

double Animation::CurrentValueBetween(double start, double target) const {
  return start + (target - start) * GetCurrentValue();
}

int Animation::CurrentValueBetween(int start, int target) const {
  return static_cast<int>(CurrentValueBetween(static_cast<double>(start),
                                              static_cast<double>(target)));
}

gfx::Rect Animation::CurrentValueBetween(const gfx::Rect& start_bounds,
                                         const gfx::Rect& target_bounds) const {
  return gfx::Rect(CurrentValueBetween(start_bounds.x(), target_bounds.x()),
                   CurrentValueBetween(start_bounds.y(), target_bounds.y()),
                   CurrentValueBetween(start_bounds.width(),
                                       target_bounds.width()),
                   CurrentValueBetween(start_bounds.height(),
                                       target_bounds.height()));
}

void Animation::Start() {
  if (!animating_) {
    if (!container_.get())
      container_ = new AnimationContainer();

    animating_ = true;

    container_->Start(this);

    if (delegate_)
      delegate_->AnimationStarted(this);
  }
}

void Animation::Stop() {
  if (animating_) {
    animating_ = false;

    // Notify the container first as the delegate may delete us.
    container_->Stop(this);

    if (delegate_) {
      if (state_ >= 1.0)
        delegate_->AnimationEnded(this);
      else
        delegate_->AnimationCanceled(this);
    }
  }
}

void Animation::End() {
  if (animating_) {
    animating_ = false;

    // Notify the container first as the delegate may delete us.
    container_->Stop(this);

    AnimateToState(1.0);
    if (delegate_)
      delegate_->AnimationEnded(this);
  }
}

bool Animation::IsAnimating() const {
  return animating_;
}

void Animation::SetDuration(int duration) {
  duration_ = TimeDelta::FromMilliseconds(duration);
  if (duration_ < timer_interval_)
    duration_ = timer_interval_;
  if (animating_)
    start_time_ = container_->last_tick_time();
}

// static
bool Animation::ShouldRenderRichAnimation() {
#if defined(OS_WIN)
  if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA) {
    BOOL result;
    // Get "Turn off all unnecessary animations" value.
    if (::SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &result, 0)) {
      // There seems to be a typo in the MSDN document (as of May 2009):
      //   http://msdn.microsoft.com/en-us/library/ms724947(VS.85).aspx
      // The document states that the result is TRUE when animations are
      // _disabled_, but in fact, it is TRUE when they are _enabled_.
      return !!result;
    }
  }
  return !::GetSystemMetrics(SM_REMOTESESSION);
#endif
  return true;
}

void Animation::SetContainer(AnimationContainer* container) {
  if (container == container_.get())
    return;

  if (animating_)
    container_->Stop(this);

  if (container)
    container_ = container;
  else
    container_ = new AnimationContainer();

  if (animating_)
    container_->Start(this);
}

void Animation::Step(base::TimeTicks time_now) {
  TimeDelta elapsed_time = time_now - start_time_;
  state_ = static_cast<double>(elapsed_time.InMicroseconds()) /
           static_cast<double>(duration_.InMicroseconds());
  if (state_ >= 1.0)
    state_ = 1.0;

  AnimateToState(state_);

  if (delegate_)
    delegate_->AnimationProgressed(this);

  if (state_ == 1.0)
    Stop();
}

TimeDelta Animation::CalculateInterval(int frame_rate) {
  int timer_interval = 1000000 / frame_rate;
  if (timer_interval < 10000)
    timer_interval = 10000;
  return TimeDelta::FromMicroseconds(timer_interval);
}
