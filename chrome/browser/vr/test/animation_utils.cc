// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/animation_utils.h"

#include "chrome/browser/vr/target_property.h"

namespace vr {

std::unique_ptr<cc::Animation> CreateTransformAnimation(
    int id,
    int group,
    const cc::TransformOperations& from,
    const cc::TransformOperations& to,
    base::TimeDelta duration) {
  std::unique_ptr<cc::KeyframedTransformAnimationCurve> curve(
      cc::KeyframedTransformAnimationCurve::Create());
  curve->AddKeyframe(
      cc::TransformKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(cc::TransformKeyframe::Create(duration, to, nullptr));
  std::unique_ptr<cc::Animation> animation(cc::Animation::Create(
      std::move(curve), id, group, TargetProperty::TRANSFORM));
  return animation;
}

std::unique_ptr<cc::Animation> CreateBoundsAnimation(int id,
                                                     int group,
                                                     const gfx::SizeF& from,
                                                     const gfx::SizeF& to,
                                                     base::TimeDelta duration) {
  std::unique_ptr<cc::KeyframedSizeAnimationCurve> curve(
      cc::KeyframedSizeAnimationCurve::Create());
  curve->AddKeyframe(
      cc::SizeKeyframe::Create(base::TimeDelta(), from, nullptr));
  curve->AddKeyframe(cc::SizeKeyframe::Create(duration, to, nullptr));
  std::unique_ptr<cc::Animation> animation(cc::Animation::Create(
      std::move(curve), id, group, TargetProperty::BOUNDS));
  return animation;
}

base::TimeTicks MicrosecondsToTicks(uint64_t us) {
  base::TimeTicks to_return;
  return base::TimeDelta::FromMicroseconds(us) + to_return;
}

base::TimeDelta MicrosecondsToDelta(uint64_t us) {
  return base::TimeDelta::FromMicroseconds(us);
}

base::TimeTicks MsToTicks(uint64_t ms) {
  return MicrosecondsToTicks(1000 * ms);
}

base::TimeDelta MsToDelta(uint64_t ms) {
  return MicrosecondsToDelta(1000 * ms);
}

}  // namespace vr
