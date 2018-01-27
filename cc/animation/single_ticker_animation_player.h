// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SINGLE_TICKER_ANIMATION_PLAYER_H_
#define CC_ANIMATION_SINGLE_TICKER_ANIMATION_PLAYER_H_

#include <vector>

#include <memory>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/element_animations.h"
#include "cc/trees/element_id.h"

namespace cc {

class AnimationTicker;

// SingleTickerAnimationPlayer is a sub-class of AnimationPlayer. It serves as a
// bridge between the cc animation clients and cc because we previously only
// supported single effect(ticker) per player.
//
// There is a 1:1 relationship between SingleTickerAnimationPlayer and the
// AnimationTicker. In general, the base class AnimationPlayer is a 1:N
// relationship to allow for grouped animations.
//
// TODO(yigu): Deprecate SingleTickerAnimationPlayer once grouped animations are
// fully supported by all clients.
class CC_ANIMATION_EXPORT SingleTickerAnimationPlayer : public AnimationPlayer {
 public:
  static scoped_refptr<SingleTickerAnimationPlayer> Create(int id);
  scoped_refptr<AnimationPlayer> CreateImplInstance() const override;

  ElementId element_id() const;

  void AttachElement(ElementId element_id);

  AnimationTicker* animation_ticker() const;
  void AddAnimation(std::unique_ptr<Animation> animation);
  void PauseAnimation(int animation_id, double time_offset);
  void RemoveAnimation(int animation_id);
  void AbortAnimation(int animation_id);

  bool NotifyAnimationFinishedForTesting(TargetProperty::Type target_property,
                                         int group_id);
  Animation* GetAnimation(TargetProperty::Type target_property) const;

 private:
  friend class base::RefCounted<SingleTickerAnimationPlayer>;

  AnimationTicker* GetTicker() const;

 protected:
  explicit SingleTickerAnimationPlayer(int id);
  explicit SingleTickerAnimationPlayer(int id, size_t ticker_id);
  ~SingleTickerAnimationPlayer() override;

  DISALLOW_COPY_AND_ASSIGN(SingleTickerAnimationPlayer);
};

}  // namespace cc

#endif  // CC_ANIMATION_SINGLE_TICKER_ANIMATION_PLAYER_H_
