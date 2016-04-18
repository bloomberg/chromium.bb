// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ELEMENT_ANIMATIONS_H_
#define CC_ANIMATION_ELEMENT_ANIMATIONS_H_

#include <memory>

#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/animation/layer_animation_value_provider.h"
#include "cc/base/cc_export.h"

namespace gfx {
class ScrollOffset;
class Transform;
}

namespace cc {

class AnimationHost;
class AnimationPlayer;
class FilterOperations;
class LayerAnimationController;
enum class LayerTreeType;

// An ElementAnimations owns a list of all AnimationPlayers, attached to
// the layer. Also, it owns LayerAnimationController instance (1:1
// relationship)
// ElementAnimations object redirects all events from LAC to the list
// of animation layers.
// This is a CC counterpart for blink::ElementAnimations (in 1:1 relationship).
// No pointer to/from respective blink::ElementAnimations object for now.
class CC_EXPORT ElementAnimations : public base::RefCounted<ElementAnimations>,
                                    public AnimationDelegate,
                                    public LayerAnimationValueObserver,
                                    public LayerAnimationValueProvider {
 public:
  static scoped_refptr<ElementAnimations> Create(AnimationHost* host);

  int layer_id() const {
    return layer_animation_controller_ ? layer_animation_controller_->id() : 0;
  }

  // Parent AnimationHost.
  AnimationHost* animation_host() { return animation_host_; }
  const AnimationHost* animation_host() const { return animation_host_; }

  void CreateLayerAnimationController(int layer_id);
  void DestroyLayerAnimationController();

  void LayerRegistered(int layer_id, LayerTreeType tree_type);
  void LayerUnregistered(int layer_id, LayerTreeType tree_type);

  bool needs_active_value_observations() const {
    return layer_animation_controller_->needs_active_value_observations();
  }
  bool needs_pending_value_observations() const {
    return layer_animation_controller_->needs_pending_value_observations();
  }

  void AddPlayer(AnimationPlayer* player);
  void RemovePlayer(AnimationPlayer* player);
  bool IsEmpty() const;

  typedef base::LinkedList<AnimationPlayer> PlayersList;
  typedef base::LinkNode<AnimationPlayer> PlayersListNode;
  const PlayersList& players_list() const { return *players_list_.get(); }

  void PushPropertiesTo(
      scoped_refptr<ElementAnimations> element_animations_impl);

  void AddAnimation(std::unique_ptr<Animation> animation);
  void PauseAnimation(int animation_id, base::TimeDelta time_offset);
  void RemoveAnimation(int animation_id);
  void AbortAnimation(int animation_id);
  void AbortAnimations(TargetProperty::Type target_property,
                       bool needs_completion = false);

  // Returns the active animation animating the given property that is either
  // running, or is next to run, if such an animation exists.
  Animation* GetAnimation(TargetProperty::Type target_property) const;

  // Returns the active animation for the given unique animation id.
  Animation* GetAnimationById(int animation_id) const;

 private:
  friend class base::RefCounted<ElementAnimations>;

  // TODO(loyso): Erase this when LAC merged into ElementAnimations.
  friend class AnimationHost;

  explicit ElementAnimations(AnimationHost* host);
  ~ElementAnimations() override;

  // LayerAnimationValueObserver implementation.
  void OnFilterAnimated(LayerTreeType tree_type,
                        const FilterOperations& filters) override;
  void OnOpacityAnimated(LayerTreeType tree_type, float opacity) override;
  void OnTransformAnimated(LayerTreeType tree_type,
                           const gfx::Transform& transform) override;
  void OnScrollOffsetAnimated(LayerTreeType tree_type,
                              const gfx::ScrollOffset& scroll_offset) override;
  void OnAnimationWaitingForDeletion() override;
  void OnTransformIsPotentiallyAnimatingChanged(LayerTreeType tree_type,
                                                bool is_animating) override;

  void CreateActiveValueObserver();
  void DestroyActiveValueObserver();

  void CreatePendingValueObserver();
  void DestroyPendingValueObserver();

  // AnimationDelegate implementation
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override;
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               int group) override;
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              TargetProperty::Type target_property,
                              int group) override;
  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               TargetProperty::Type target_property,
                               double animation_start_time,
                               std::unique_ptr<AnimationCurve> curve) override;

  // LayerAnimationValueProvider implementation.
  gfx::ScrollOffset ScrollOffsetForAnimation() const override;

  std::unique_ptr<PlayersList> players_list_;

  // LAC is owned by ElementAnimations (1:1 relationship).
  scoped_refptr<LayerAnimationController> layer_animation_controller_;
  AnimationHost* animation_host_;

  DISALLOW_COPY_AND_ASSIGN(ElementAnimations);
};

}  // namespace cc

#endif  // CC_ANIMATION_ELEMENT_ANIMATIONS_H_
