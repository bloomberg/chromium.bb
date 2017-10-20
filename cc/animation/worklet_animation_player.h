// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_WORKLET_ANIMATION_PLAYER_H_
#define CC_ANIMATION_WORKLET_ANIMATION_PLAYER_H_

#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_ticker.h"

namespace cc {

// A WorkletAnimationPlayer is an animations player that allows its animation
// timing to be controlled by an animator instance that is running in a
// AnimationWorkletGlobalScope.
class CC_ANIMATION_EXPORT WorkletAnimationPlayer final
    : public AnimationPlayer,
      AnimationTicker::AnimationTimeProvider {
 public:
  WorkletAnimationPlayer(int id, const std::string& name);
  static scoped_refptr<WorkletAnimationPlayer> Create(int id,
                                                      const std::string& name);
  scoped_refptr<AnimationPlayer> CreateImplInstance() const override;

  const std::string& name() const { return name_; }
  void SetLocalTime(base::TimeDelta local_time);
  bool IsWorkletAnimationPlayer() const override;

  void Tick(base::TimeTicks monotonic_time) override;

  // AnimationTicker::AnimationTimeProvider:
  base::TimeTicks GetTimeForAnimation(
      const Animation& animation) const override;

  void PushPropertiesTo(AnimationPlayer* animation_player_impl) override;

 private:
  ~WorkletAnimationPlayer() override;

  std::string name_;
  base::TimeDelta local_time_;
};

}  // namespace cc

#endif  // CC_ANIMATION_WORKLET_ANIMATION_PLAYER_H_
