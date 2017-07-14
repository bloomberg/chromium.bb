// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ANIMATION_PLAYER_H_
#define CHROME_BROWSER_VR_ANIMATION_PLAYER_H_

#include <vector>

#include "base/macros.h"
#include "cc/animation/animation.h"
#include "cc/trees/target_property.h"

namespace cc {
class AnimationTarget;
}  // namespace cc

namespace vr {

// This is a simplified version of the cc::AnimationPlayer. Its sole purpose is
// the management of its collection of animations. Ticking them, updating their
// state, and deleting them as required.
//
// TODO(vollick): if cc::Animation and friends move into gfx/, then this class
// should follow suit. As such, it should not absorb any vr-specific
// functionality.
class AnimationPlayer final {
 public:
  AnimationPlayer();
  ~AnimationPlayer();

  cc::AnimationTarget* target() const { return target_; }
  void set_target(cc::AnimationTarget* target) { target_ = target; }

  // Animations for properties already being animated are enqueued. Animation
  // groups are not currently supported. crbug.com/742358
  void AddAnimation(std::unique_ptr<cc::Animation> animation);
  void RemoveAnimation(int animation_id);
  void RemoveAnimations(cc::TargetProperty::Type target_property);

  void Tick(base::TimeTicks monotonic_time);

  using Animations = std::vector<std::unique_ptr<cc::Animation>>;
  const Animations& animations() { return animations_; }

 private:
  void StartAnimations(base::TimeTicks monotonic_time);

  cc::AnimationTarget* target_ = nullptr;
  Animations animations_;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace vr

#endif  //  CHROME_BROWSER_VR_ANIMATION_PLAYER_H_
