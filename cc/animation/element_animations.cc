// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/trees/mutator_host_client.h"

namespace cc {

class ElementAnimations::ValueObserver : public LayerAnimationValueObserver {
 public:
  ValueObserver(ElementAnimations* element_animation, LayerTreeType tree_type)
      : element_animations_(element_animation), tree_type_(tree_type) {
    DCHECK(element_animations_);
  }

  // LayerAnimationValueObserver implementation.
  void OnFilterAnimated(const FilterOperations& filters) override {
    element_animations_->SetFilterMutated(tree_type_, filters);
  }

  void OnOpacityAnimated(float opacity) override {
    element_animations_->SetOpacityMutated(tree_type_, opacity);
  }

  void OnTransformAnimated(const gfx::Transform& transform) override {
    element_animations_->SetTransformMutated(tree_type_, transform);
  }

  void OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset) override {
    element_animations_->SetScrollOffsetMutated(tree_type_, scroll_offset);
  }

  void OnAnimationWaitingForDeletion() override {
    // TODO(loyso): See Layer::OnAnimationWaitingForDeletion. But we always do
    // PushProperties for AnimationTimelines for now.
  }

  void OnTransformIsPotentiallyAnimatingChanged(bool is_animating) override {
    element_animations_->SetTransformIsPotentiallyAnimatingChanged(
        tree_type_, is_animating);
  }

  bool IsActive() const override { return tree_type_ == LayerTreeType::ACTIVE; }

 private:
  ElementAnimations* element_animations_;
  const LayerTreeType tree_type_;

  DISALLOW_COPY_AND_ASSIGN(ValueObserver);
};

std::unique_ptr<ElementAnimations> ElementAnimations::Create(
    AnimationHost* host) {
  return base::WrapUnique(new ElementAnimations(host));
}

ElementAnimations::ElementAnimations(AnimationHost* host)
    : players_list_(new PlayersList()), animation_host_(host) {
  DCHECK(animation_host_);
}

ElementAnimations::~ElementAnimations() {
  DCHECK(!layer_animation_controller_);
}

void ElementAnimations::CreateLayerAnimationController(int layer_id) {
  DCHECK(layer_id);
  DCHECK(!layer_animation_controller_);
  DCHECK(animation_host_);

  layer_animation_controller_ =
      animation_host_->GetAnimationControllerForId(layer_id);
  layer_animation_controller_->SetAnimationHost(animation_host_);
  layer_animation_controller_->set_layer_animation_delegate(this);
  layer_animation_controller_->set_value_provider(this);

  DCHECK(animation_host_->mutator_host_client());
  if (animation_host_->mutator_host_client()->IsLayerInTree(
          layer_id, LayerTreeType::ACTIVE))
    CreateActiveValueObserver();
  if (animation_host_->mutator_host_client()->IsLayerInTree(
          layer_id, LayerTreeType::PENDING))
    CreatePendingValueObserver();
}

void ElementAnimations::DestroyLayerAnimationController() {
  DCHECK(animation_host_);

  if (active_value_observer_)
    SetTransformIsPotentiallyAnimatingChanged(LayerTreeType::ACTIVE, false);
  if (pending_value_observer_)
    SetTransformIsPotentiallyAnimatingChanged(LayerTreeType::PENDING, false);

  DestroyPendingValueObserver();
  DestroyActiveValueObserver();

  if (layer_animation_controller_) {
    layer_animation_controller_->remove_value_provider(this);
    layer_animation_controller_->remove_layer_animation_delegate(this);
    layer_animation_controller_->SetAnimationHost(nullptr);
    layer_animation_controller_ = nullptr;
  }
}

void ElementAnimations::LayerRegistered(int layer_id, LayerTreeType tree_type) {
  DCHECK(layer_animation_controller_);
  DCHECK_EQ(layer_animation_controller_->id(), layer_id);

  if (tree_type == LayerTreeType::ACTIVE) {
    if (!active_value_observer_)
      CreateActiveValueObserver();
  } else {
    if (!pending_value_observer_)
      CreatePendingValueObserver();
  }
}

void ElementAnimations::LayerUnregistered(int layer_id,
                                          LayerTreeType tree_type) {
  DCHECK_EQ(this->layer_id(), layer_id);
  tree_type == LayerTreeType::ACTIVE ? DestroyActiveValueObserver()
                                     : DestroyPendingValueObserver();
}

void ElementAnimations::AddPlayer(AnimationPlayer* player) {
  players_list_->Append(player);
}

void ElementAnimations::RemovePlayer(AnimationPlayer* player) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    if (node->value() == player) {
      node->RemoveFromList();
      return;
    }
  }
}

bool ElementAnimations::IsEmpty() const {
  return players_list_->empty();
}

void ElementAnimations::PushPropertiesTo(
    ElementAnimations* element_animations_impl) {
  DCHECK(layer_animation_controller_);
  DCHECK(element_animations_impl->layer_animation_controller_);

  layer_animation_controller_->PushAnimationUpdatesTo(
      element_animations_impl->layer_animation_controller_.get());
}

void ElementAnimations::AddAnimation(std::unique_ptr<Animation> animation) {
  layer_animation_controller_->AddAnimation(std::move(animation));
}

void ElementAnimations::PauseAnimation(int animation_id,
                                       base::TimeDelta time_offset) {
  layer_animation_controller_->PauseAnimation(animation_id, time_offset);
}

void ElementAnimations::RemoveAnimation(int animation_id) {
  layer_animation_controller_->RemoveAnimation(animation_id);
}

void ElementAnimations::AbortAnimation(int animation_id) {
  layer_animation_controller_->AbortAnimation(animation_id);
}

void ElementAnimations::AbortAnimations(TargetProperty::Type target_property,
                                        bool needs_completion) {
  layer_animation_controller_->AbortAnimations(target_property,
                                               needs_completion);
}

Animation* ElementAnimations::GetAnimation(
    TargetProperty::Type target_property) const {
  return layer_animation_controller_->GetAnimation(target_property);
}

Animation* ElementAnimations::GetAnimationById(int animation_id) const {
  return layer_animation_controller_->GetAnimationById(animation_id);
}

void ElementAnimations::AddEventObserver(
    LayerAnimationEventObserver* observer) {
  layer_animation_controller_->AddEventObserver(observer);
}

void ElementAnimations::RemoveEventObserver(
    LayerAnimationEventObserver* observer) {
  layer_animation_controller_->RemoveEventObserver(observer);
}

void ElementAnimations::SetFilterMutated(LayerTreeType tree_type,
                                         const FilterOperations& filters) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerFilterMutated(
      layer_id(), tree_type, filters);
}

void ElementAnimations::SetOpacityMutated(LayerTreeType tree_type,
                                          float opacity) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerOpacityMutated(
      layer_id(), tree_type, opacity);
}

void ElementAnimations::SetTransformMutated(LayerTreeType tree_type,
                                            const gfx::Transform& transform) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerTransformMutated(
      layer_id(), tree_type, transform);
}

void ElementAnimations::SetScrollOffsetMutated(
    LayerTreeType tree_type,
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerScrollOffsetMutated(
      layer_id(), tree_type, scroll_offset);
}

void ElementAnimations::SetTransformIsPotentiallyAnimatingChanged(
    LayerTreeType tree_type,
    bool is_animating) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()
      ->mutator_host_client()
      ->LayerTransformIsPotentiallyAnimatingChanged(layer_id(), tree_type,
                                                    is_animating);
}

void ElementAnimations::CreateActiveValueObserver() {
  DCHECK(layer_animation_controller_);
  DCHECK(!active_value_observer_);
  active_value_observer_ =
      base::WrapUnique(new ValueObserver(this, LayerTreeType::ACTIVE));
  layer_animation_controller_->AddValueObserver(active_value_observer_.get());
}

void ElementAnimations::DestroyActiveValueObserver() {
  if (layer_animation_controller_ && active_value_observer_)
    layer_animation_controller_->RemoveValueObserver(
        active_value_observer_.get());
  active_value_observer_ = nullptr;
}

void ElementAnimations::CreatePendingValueObserver() {
  DCHECK(layer_animation_controller_);
  DCHECK(!pending_value_observer_);
  pending_value_observer_ =
      base::WrapUnique(new ValueObserver(this, LayerTreeType::PENDING));
  layer_animation_controller_->AddValueObserver(pending_value_observer_.get());
}

void ElementAnimations::DestroyPendingValueObserver() {
  if (layer_animation_controller_ && pending_value_observer_)
    layer_animation_controller_->RemoveValueObserver(
        pending_value_observer_.get());
  pending_value_observer_ = nullptr;
}

void ElementAnimations::NotifyAnimationStarted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationStarted(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationFinished(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyAnimationAborted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    AnimationPlayer* player = node->value();
    player->NotifyAnimationAborted(monotonic_time, target_property, group);
  }
}

void ElementAnimations::NotifyAnimationTakeover(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    double animation_start_time,
    std::unique_ptr<AnimationCurve> curve) {
  DCHECK(curve);
  for (PlayersListNode* node = players_list_->head();
       node != players_list_->end(); node = node->next()) {
    std::unique_ptr<AnimationCurve> animation_curve = curve->Clone();
    AnimationPlayer* player = node->value();
    player->NotifyAnimationTakeover(monotonic_time, target_property,
                                    animation_start_time,
                                    std::move(animation_curve));
  }
}

gfx::ScrollOffset ElementAnimations::ScrollOffsetForAnimation() const {
  DCHECK(layer_animation_controller_);
  if (animation_host()) {
    DCHECK(animation_host()->mutator_host_client());
    return animation_host()->mutator_host_client()->GetScrollOffsetForAnimation(
        layer_id());
  }

  return gfx::ScrollOffset();
}

}  // namespace cc
