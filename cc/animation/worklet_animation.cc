// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/worklet_animation.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/scroll_timeline.h"

namespace cc {

WorkletAnimation::WorkletAnimation(
    int id,
    const std::string& name,
    std::unique_ptr<ScrollTimeline> scroll_timeline)
    : SingleKeyframeEffectAnimation(id),
      name_(name),
      scroll_timeline_(std::move(scroll_timeline)),
      last_current_time_(base::nullopt) {}

WorkletAnimation::~WorkletAnimation() = default;

scoped_refptr<WorkletAnimation> WorkletAnimation::Create(
    int id,
    const std::string& name,
    std::unique_ptr<ScrollTimeline> scroll_timeline) {
  return WrapRefCounted(
      new WorkletAnimation(id, name, std::move(scroll_timeline)));
}

scoped_refptr<Animation> WorkletAnimation::CreateImplInstance() const {
  std::unique_ptr<ScrollTimeline> impl_timeline;
  if (scroll_timeline_)
    impl_timeline = scroll_timeline_->CreateImplInstance();

  return WrapRefCounted(
      new WorkletAnimation(id(), name(), std::move(impl_timeline)));
}

void WorkletAnimation::SetLocalTime(base::TimeDelta local_time) {
  local_time_ = local_time;
  SetNeedsPushProperties();
}

void WorkletAnimation::Tick(base::TimeTicks monotonic_time) {
  keyframe_effect()->Tick(monotonic_time, this);
}

// TODO(crbug.com/780151): The current time returned should be an offset against
// the animation's start time and based on the playback rate, not just the
// timeline time directly.
double WorkletAnimation::CurrentTime(base::TimeTicks monotonic_time,
                                     const ScrollTree& scroll_tree) {
  if (scroll_timeline_) {
    return scroll_timeline_->CurrentTime(scroll_tree);
  }

  // TODO(crbug.com/783333): Support DocumentTimeline's originTime concept.
  return (monotonic_time - base::TimeTicks()).InMillisecondsF();
}

bool WorkletAnimation::NeedsUpdate(base::TimeTicks monotonic_time,
                                   const ScrollTree& scroll_tree) {
  double current_time = CurrentTime(monotonic_time, scroll_tree);
  bool needs_update = last_current_time_ != current_time;
  last_current_time_ = current_time;
  return needs_update;
}

base::TimeTicks WorkletAnimation::GetTimeForKeyframeModel(
    const KeyframeModel& keyframe_model) const {
  // Animation local time is equivalent to animation active time. So we have to
  // convert it from active time to monotonic time.
  return keyframe_model.ConvertFromActiveTime(local_time_);
}

void WorkletAnimation::PushPropertiesTo(Animation* animation_impl) {
  SingleKeyframeEffectAnimation::PushPropertiesTo(animation_impl);
  static_cast<WorkletAnimation*>(animation_impl)->SetLocalTime(local_time_);
}

bool WorkletAnimation::IsWorkletAnimation() const {
  return true;
}

}  // namespace cc
