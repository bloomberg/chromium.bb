// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_PLAYER_H_
#define CC_ANIMATION_ANIMATION_PLAYER_H_

#include <vector>

#include <memory>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_ticker.h"
#include "cc/animation/element_animations.h"
#include "cc/trees/element_id.h"

namespace cc {

class AnimationDelegate;
class AnimationEvents;
class AnimationHost;
class AnimationTimeline;
struct AnimationEvent;

// An AnimationPlayer manages grouped sets of animations (each set of which are
// stored in an AnimationTicker), and handles the interaction with the
// AnimationHost and AnimationTimeline.
//
// This class is a CC counterpart for blink::Animation, currently in a 1:1
// relationship. Currently the blink logic is responsible for handling of
// conflicting same-property animations.
//
// Each cc AnimationPlayer has a copy on the impl thread, and will take care of
// synchronizing properties to/from the impl thread when requested.
//
// There is a 1:n relationship between AnimationPlayer and AnimationTicker.
class CC_ANIMATION_EXPORT AnimationPlayer
    : public base::RefCounted<AnimationPlayer> {
 public:
  static scoped_refptr<AnimationPlayer> Create(int id);
  virtual scoped_refptr<AnimationPlayer> CreateImplInstance() const;

  int id() const { return id_; }
  // An AnimationPlayer is capable of handling group effects in which case each
  // of the effects is owned by an AnimationTicker. The tickers are stored in
  // tickers_ and indexed by TickerId.
  typedef size_t TickerId;
  ElementId element_id_of_ticker(TickerId ticker_id) const;
  bool IsElementAttached(ElementId id) const;

  // Parent AnimationHost. AnimationPlayer can be detached from
  // AnimationTimeline.
  AnimationHost* animation_host() { return animation_host_; }
  const AnimationHost* animation_host() const { return animation_host_; }
  void SetAnimationHost(AnimationHost* animation_host);
  bool has_animation_host() const { return !!animation_host_; }

  // Parent AnimationTimeline.
  AnimationTimeline* animation_timeline() { return animation_timeline_; }
  const AnimationTimeline* animation_timeline() const {
    return animation_timeline_;
  }
  virtual void SetAnimationTimeline(AnimationTimeline* timeline);

  bool has_element_animations() const;
  scoped_refptr<ElementAnimations> element_animations(TickerId ticker_id) const;

  void set_animation_delegate(AnimationDelegate* delegate) {
    animation_delegate_ = delegate;
  }

  void AttachElementForTicker(ElementId element_id, TickerId ticker_id);
  void DetachElementForTicker(ElementId element_id, TickerId ticker_id);
  virtual void DetachElement();

  void AddAnimationForTicker(std::unique_ptr<Animation> animation,
                             TickerId ticker_id);
  void PauseAnimationForTicker(int animation_id,
                               double time_offset,
                               TickerId ticker_id);
  void RemoveAnimationForTicker(int animation_id, TickerId ticker_id);
  void AbortAnimationForTicker(int animation_id, TickerId ticker_id);
  void AbortAnimations(TargetProperty::Type target_property,
                       bool needs_completion);

  virtual void PushPropertiesTo(AnimationPlayer* player_impl);

  void UpdateState(bool start_ready_animations, AnimationEvents* events);
  virtual void Tick(base::TimeTicks monotonic_time);

  void AddToTicking();
  void AnimationRemovedFromTicking();

  // AnimationDelegate routing.
  void NotifyAnimationStarted(const AnimationEvent& event);
  void NotifyAnimationFinished(const AnimationEvent& event);
  void NotifyAnimationAborted(const AnimationEvent& event);
  void NotifyAnimationTakeover(const AnimationEvent& event);
  size_t TickingAnimationsCount() const;

  void SetNeedsPushProperties();

  // Make animations affect active elements if and only if they affect
  // pending elements. Any animations that no longer affect any elements
  // are deleted.
  void ActivateAnimations();

  // Returns the animation animating the given property that is either
  // running, or is next to run, if such an animation exists.
  Animation* GetAnimationForTicker(TargetProperty::Type target_property,
                                   TickerId ticker_id) const;

  std::string ToString() const;

  void SetNeedsCommit();

  virtual bool IsWorkletAnimationPlayer() const;
  void AddTicker(std::unique_ptr<AnimationTicker>);

  AnimationTicker* GetTickerById(TickerId ticker_id) const;
  TickerId NextTickerId() { return tickers_.size(); }

 private:
  friend class base::RefCounted<AnimationPlayer>;

  void RegisterTicker(ElementId element_id, TickerId ticker_id);
  void UnregisterTicker(ElementId element_id, TickerId ticker_id);
  void RegisterTickers();
  void UnregisterTickers();

  void PushAttachedTickersToImplThread(AnimationPlayer* player) const;
  void PushPropertiesToImplThread(AnimationPlayer* player);

 protected:
  explicit AnimationPlayer(int id);
  virtual ~AnimationPlayer();

  AnimationHost* animation_host_;
  AnimationTimeline* animation_timeline_;
  AnimationDelegate* animation_delegate_;

  int id_;

  using ElementToTickerIdMap = std::
      unordered_map<ElementId, std::unordered_set<TickerId>, ElementIdHash>;
  using Tickers = std::vector<std::unique_ptr<AnimationTicker>>;

  // It is possible for a ticker to be in tickers_ but not in
  // element_to_ticker_id_map_ but the reverse is not possible.
  ElementToTickerIdMap element_to_ticker_id_map_;
  Tickers tickers_;

  int ticking_tickers_count;

  DISALLOW_COPY_AND_ASSIGN(AnimationPlayer);
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_PLAYER_H_
