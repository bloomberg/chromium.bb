// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/element_animations.h"

#include "base/macros.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/trees/mutator_host_client.h"

namespace cc {

class ElementAnimations::ValueObserver : public LayerAnimationValueObserver {
 public:
  ValueObserver(ElementAnimations* element_animation, LayerListType list_type)
      : element_animations_(element_animation), list_type_(list_type) {
    DCHECK(element_animations_);
  }

  // LayerAnimationValueObserver implementation.
  void OnFilterAnimated(const FilterOperations& filters) override {
    element_animations_->SetFilterMutated(list_type_, filters);
  }

  void OnOpacityAnimated(float opacity) override {
    element_animations_->SetOpacityMutated(list_type_, opacity);
  }

  void OnTransformAnimated(const gfx::Transform& transform) override {
    element_animations_->SetTransformMutated(list_type_, transform);
  }

  void OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset) override {
    element_animations_->SetScrollOffsetMutated(list_type_, scroll_offset);
  }

  void OnAnimationWaitingForDeletion() override {
    // TODO(loyso): See Layer::OnAnimationWaitingForDeletion. But we always do
    // PushProperties for AnimationTimelines for now.
  }

  void OnTransformIsPotentiallyAnimatingChanged(bool is_animating) override {
    element_animations_->SetTransformIsPotentiallyAnimatingChanged(
        list_type_, is_animating);
  }

  bool IsActive() const override { return list_type_ == LayerListType::ACTIVE; }

 private:
  ElementAnimations* element_animations_;
  const LayerListType list_type_;

  DISALLOW_COPY_AND_ASSIGN(ValueObserver);
};

scoped_ptr<ElementAnimations> ElementAnimations::Create(AnimationHost* host) {
  return make_scoped_ptr(new ElementAnimations(host));
}

ElementAnimations::ElementAnimations(AnimationHost* host)
    : players_list_(make_scoped_ptr(new PlayersList())), animation_host_(host) {
  DCHECK(animation_host_);
}

ElementAnimations::~ElementAnimations() {
  DCHECK(!layer_animation_controller_);
}

void ElementAnimations::CreateLayerAnimationController(int layer_id) {
  DCHECK(layer_id);
  DCHECK(!layer_animation_controller_);
  DCHECK(animation_host_);

  AnimationRegistrar* registrar = animation_host_->animation_registrar();
  DCHECK(registrar);

  layer_animation_controller_ =
      registrar->GetAnimationControllerForId(layer_id);
  layer_animation_controller_->SetAnimationRegistrar(registrar);
  layer_animation_controller_->set_layer_animation_delegate(this);
  layer_animation_controller_->set_value_provider(this);

  DCHECK(animation_host_->mutator_host_client());
  if (animation_host_->mutator_host_client()->IsLayerInTree(
          layer_id, LayerListType::ACTIVE))
    CreateActiveValueObserver();
  if (animation_host_->mutator_host_client()->IsLayerInTree(
          layer_id, LayerListType::PENDING))
    CreatePendingValueObserver();
}

void ElementAnimations::DestroyLayerAnimationController() {
  DCHECK(animation_host_);

  if (active_value_observer_)
    SetTransformIsPotentiallyAnimatingChanged(LayerListType::ACTIVE, false);
  if (pending_value_observer_)
    SetTransformIsPotentiallyAnimatingChanged(LayerListType::PENDING, false);

  DestroyPendingValueObserver();
  DestroyActiveValueObserver();

  if (layer_animation_controller_) {
    layer_animation_controller_->remove_value_provider(this);
    layer_animation_controller_->remove_layer_animation_delegate(this);
    layer_animation_controller_->SetAnimationRegistrar(nullptr);
    layer_animation_controller_ = nullptr;
  }
}

void ElementAnimations::LayerRegistered(int layer_id, LayerListType list_type) {
  DCHECK(layer_animation_controller_);
  DCHECK_EQ(layer_animation_controller_->id(), layer_id);

  if (list_type == LayerListType::ACTIVE) {
    if (!active_value_observer_)
      CreateActiveValueObserver();
  } else {
    if (!pending_value_observer_)
      CreatePendingValueObserver();
  }
}

void ElementAnimations::LayerUnregistered(int layer_id,
                                          LayerListType list_type) {
  DCHECK_EQ(this->layer_id(), layer_id);
  list_type == LayerListType::ACTIVE ? DestroyActiveValueObserver()
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
  DCHECK(element_animations_impl->layer_animation_controller());

  layer_animation_controller_->PushAnimationUpdatesTo(
      element_animations_impl->layer_animation_controller());
}

void ElementAnimations::SetFilterMutated(LayerListType list_type,
                                         const FilterOperations& filters) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerFilterMutated(
      layer_id(), list_type, filters);
}

void ElementAnimations::SetOpacityMutated(LayerListType list_type,
                                          float opacity) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerOpacityMutated(
      layer_id(), list_type, opacity);
}

void ElementAnimations::SetTransformMutated(LayerListType list_type,
                                            const gfx::Transform& transform) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerTransformMutated(
      layer_id(), list_type, transform);
}

void ElementAnimations::SetScrollOffsetMutated(
    LayerListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()->mutator_host_client()->SetLayerScrollOffsetMutated(
      layer_id(), list_type, scroll_offset);
}

void ElementAnimations::SetTransformIsPotentiallyAnimatingChanged(
    LayerListType list_type,
    bool is_animating) {
  DCHECK(layer_id());
  DCHECK(animation_host());
  DCHECK(animation_host()->mutator_host_client());
  animation_host()
      ->mutator_host_client()
      ->LayerTransformIsPotentiallyAnimatingChanged(layer_id(), list_type,
                                                    is_animating);
}

void ElementAnimations::CreateActiveValueObserver() {
  DCHECK(layer_animation_controller_);
  DCHECK(!active_value_observer_);
  active_value_observer_ =
      make_scoped_ptr(new ValueObserver(this, LayerListType::ACTIVE));
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
      make_scoped_ptr(new ValueObserver(this, LayerListType::PENDING));
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
