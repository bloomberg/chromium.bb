// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation_player.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/scroll_timeline.h"

namespace cc {

WorkletAnimationPlayer::WorkletAnimationPlayer(
    int id,
    const std::string& name,
    std::unique_ptr<ScrollTimeline> scroll_timeline)
    : AnimationPlayer(id),
      name_(name),
      scroll_timeline_(std::move(scroll_timeline)) {}

WorkletAnimationPlayer::~WorkletAnimationPlayer() {}

scoped_refptr<WorkletAnimationPlayer> WorkletAnimationPlayer::Create(
    int id,
    const std::string& name,
    std::unique_ptr<ScrollTimeline> scroll_timeline) {
  return WrapRefCounted(
      new WorkletAnimationPlayer(id, name, std::move(scroll_timeline)));
}

scoped_refptr<AnimationPlayer> WorkletAnimationPlayer::CreateImplInstance()
    const {
  std::unique_ptr<ScrollTimeline> impl_timeline;
  if (scroll_timeline_)
    impl_timeline = scroll_timeline_->CreateImplInstance();

  return WrapRefCounted(
      new WorkletAnimationPlayer(id(), name(), std::move(impl_timeline)));
}

void WorkletAnimationPlayer::SetLocalTime(base::TimeDelta local_time) {
  local_time_ = local_time;
  SetNeedsPushProperties();
}

void WorkletAnimationPlayer::Tick(base::TimeTicks monotonic_time) {
  animation_ticker_->Tick(monotonic_time, this);
}

// TODO(crbug.com/780151): The current time returned should be an offset against
// the animation's start time and based on the playback rate, not just the
// timeline time directly.
double WorkletAnimationPlayer::CurrentTime(base::TimeTicks monotonic_time,
                                           const ScrollTree& scroll_tree) {
  if (scroll_timeline_) {
    return scroll_timeline_->CurrentTime(scroll_tree);
  }

  // TODO(crbug.com/783333): Support DocumentTimeline's originTime concept.
  return (monotonic_time - base::TimeTicks()).InMillisecondsF();
}

base::TimeTicks WorkletAnimationPlayer::GetTimeForAnimation(
    const Animation& animation) const {
  // Animation player local time is equivalent to animation active time. So
  // we have to convert it from active time to monotonic time.
  return animation.ConvertFromActiveTime(local_time_);
}

void WorkletAnimationPlayer::PushPropertiesTo(
    AnimationPlayer* animation_player_impl) {
  AnimationPlayer::PushPropertiesTo(animation_player_impl);
  static_cast<WorkletAnimationPlayer*>(animation_player_impl)
      ->SetLocalTime(local_time_);
}

bool WorkletAnimationPlayer::IsWorkletAnimationPlayer() const {
  return true;
}

}  // namespace cc
