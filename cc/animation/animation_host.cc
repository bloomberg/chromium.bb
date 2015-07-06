// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_host.h"

#include <algorithm>

#include "cc/animation/animation_player.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

scoped_ptr<AnimationHost> AnimationHost::Create() {
  return make_scoped_ptr(new AnimationHost);
}

AnimationHost::AnimationHost()
    : animation_registrar_(AnimationRegistrar::Create()),
      mutator_host_client_(nullptr) {
}

AnimationHost::~AnimationHost() {
  ClearTimelines();
  DCHECK(!mutator_host_client());
  DCHECK(layer_to_element_animations_map_.empty());
}

AnimationTimeline* AnimationHost::GetTimelineById(int timeline_id) const {
  for (auto& timeline : timelines_)
    if (timeline->id() == timeline_id)
      return timeline.get();
  return nullptr;
}

void AnimationHost::ClearTimelines() {
  EraseTimelines(timelines_.begin(), timelines_.end());
}

void AnimationHost::EraseTimelines(AnimationTimelineList::iterator begin,
                                   AnimationTimelineList::iterator end) {
  for (auto i = begin; i != end; ++i) {
    auto& timeline = *i;
    timeline->ClearPlayers();
    timeline->SetAnimationHost(nullptr);
  }

  timelines_.erase(begin, end);
}

void AnimationHost::AddAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  timeline->SetAnimationHost(this);
  timelines_.push_back(timeline);
}

void AnimationHost::RemoveAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  for (auto iter = timelines_.begin(); iter != timelines_.end(); ++iter) {
    if (iter->get() != timeline)
      continue;

    EraseTimelines(iter, iter + 1);
    break;
  }
}

void AnimationHost::RegisterLayer(int layer_id, LayerTreeType tree_type) {
  ElementAnimations* element_animations =
      GetElementAnimationsForLayerId(layer_id);
  if (element_animations)
    element_animations->LayerRegistered(layer_id, tree_type);
}

void AnimationHost::UnregisterLayer(int layer_id, LayerTreeType tree_type) {
  ElementAnimations* element_animations =
      GetElementAnimationsForLayerId(layer_id);
  if (element_animations)
    element_animations->LayerUnregistered(layer_id, tree_type);
}

void AnimationHost::RegisterPlayerForLayer(int layer_id,
                                           AnimationPlayer* player) {
  DCHECK(layer_id);
  DCHECK(player);

  ElementAnimations* element_animations =
      GetElementAnimationsForLayerId(layer_id);
  if (!element_animations) {
    auto new_element_animations = ElementAnimations::Create(this);
    element_animations = new_element_animations.get();

    layer_to_element_animations_map_.add(layer_id,
                                         new_element_animations.Pass());
    element_animations->CreateLayerAnimationController(layer_id);
  }

  DCHECK(element_animations);
  element_animations->AddPlayer(player);
}

void AnimationHost::UnregisterPlayerForLayer(int layer_id,
                                             AnimationPlayer* player) {
  DCHECK(layer_id);
  DCHECK(player);

  ElementAnimations* element_animations =
      GetElementAnimationsForLayerId(layer_id);
  DCHECK(element_animations);
  element_animations->RemovePlayer(player);

  if (element_animations->IsEmpty()) {
    element_animations->DestroyLayerAnimationController();
    layer_to_element_animations_map_.erase(layer_id);
    element_animations = nullptr;
  }
}

void AnimationHost::SetMutatorHostClient(MutatorHostClient* client) {
  if (mutator_host_client_ == client)
    return;

  mutator_host_client_ = client;
}

void AnimationHost::SetNeedsCommit() {
  DCHECK(mutator_host_client_);
  mutator_host_client_->SetMutatorsNeedCommit();
}

void AnimationHost::PushPropertiesTo(AnimationHost* host_impl) {
  PushTimelinesToImplThread(host_impl);
  RemoveTimelinesFromImplThread(host_impl);
  PushPropertiesToImplThread(host_impl);
}

void AnimationHost::PushTimelinesToImplThread(AnimationHost* host_impl) const {
  for (auto& timeline : timelines_) {
    AnimationTimeline* timeline_impl =
        host_impl->GetTimelineById(timeline->id());
    if (timeline_impl)
      continue;

    scoped_refptr<AnimationTimeline> to_add = timeline->CreateImplInstance();
    host_impl->AddAnimationTimeline(to_add.get());
  }
}

void AnimationHost::RemoveTimelinesFromImplThread(
    AnimationHost* host_impl) const {
  AnimationTimelineList& timelines_impl = host_impl->timelines_;

  auto to_erase =
      std::partition(timelines_impl.begin(), timelines_impl.end(),
                     [this](AnimationTimelineList::value_type timeline_impl) {
                       return timeline_impl->is_impl_only() ||
                              GetTimelineById(timeline_impl->id());
                     });

  host_impl->EraseTimelines(to_erase, timelines_impl.end());
}

void AnimationHost::PushPropertiesToImplThread(AnimationHost* host_impl) {
  // Firstly, sync all players with impl thread to create ElementAnimations and
  // layer animation controllers.
  for (auto& timeline : timelines_) {
    AnimationTimeline* timeline_impl =
        host_impl->GetTimelineById(timeline->id());
    if (timeline_impl)
      timeline->PushPropertiesTo(timeline_impl);
  }

  // Secondly, sync properties for created layer animation controllers.
  for (auto& kv : layer_to_element_animations_map_) {
    ElementAnimations* element_animations = kv.second;
    ElementAnimations* element_animations_impl =
        host_impl->GetElementAnimationsForLayerId(kv.first);
    if (element_animations_impl)
      element_animations->PushPropertiesTo(element_animations_impl);
  }
}

LayerAnimationController* AnimationHost::GetControllerForLayerId(
    int layer_id) const {
  const ElementAnimations* element_animations =
      GetElementAnimationsForLayerId(layer_id);
  if (!element_animations)
    return nullptr;

  return element_animations->layer_animation_controller();
}

ElementAnimations* AnimationHost::GetElementAnimationsForLayerId(
    int layer_id) const {
  DCHECK(layer_id);
  auto iter = layer_to_element_animations_map_.find(layer_id);
  return iter == layer_to_element_animations_map_.end() ? nullptr
                                                        : iter->second;
}

void AnimationHost::SetSupportsScrollAnimations(
    bool supports_scroll_animations) {
  animation_registrar_->set_supports_scroll_animations(
      supports_scroll_animations);
}

bool AnimationHost::SupportsScrollAnimations() const {
  return animation_registrar_->supports_scroll_animations();
}

bool AnimationHost::NeedsAnimateLayers() const {
  return animation_registrar_->needs_animate_layers();
}

bool AnimationHost::ActivateAnimations() {
  return animation_registrar_->ActivateAnimations();
}

bool AnimationHost::AnimateLayers(base::TimeTicks monotonic_time) {
  return animation_registrar_->AnimateLayers(monotonic_time);
}

bool AnimationHost::UpdateAnimationState(bool start_ready_animations,
                                         AnimationEventsVector* events) {
  return animation_registrar_->UpdateAnimationState(start_ready_animations,
                                                    events);
}

scoped_ptr<AnimationEventsVector> AnimationHost::CreateEvents() {
  return animation_registrar_->CreateEvents();
}

void AnimationHost::SetAnimationEvents(
    scoped_ptr<AnimationEventsVector> events) {
  return animation_registrar_->SetAnimationEvents(events.Pass());
}

bool AnimationHost::ScrollOffsetAnimationWasInterrupted(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->scroll_offset_animation_was_interrupted()
                    : false;
}

bool AnimationHost::IsAnimatingFilterProperty(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->IsAnimatingProperty(Animation::FILTER)
                    : false;
}

bool AnimationHost::IsAnimatingOpacityProperty(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->IsAnimatingProperty(Animation::OPACITY)
                    : false;
}

bool AnimationHost::IsAnimatingTransformProperty(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->IsAnimatingProperty(Animation::TRANSFORM)
                    : false;
}

bool AnimationHost::HasPotentiallyRunningOpacityAnimation(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  if (!controller)
    return false;

  Animation* animation = controller->GetAnimation(Animation::OPACITY);
  return animation && !animation->is_finished();
}

bool AnimationHost::HasPotentiallyRunningTransformAnimation(
    int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  if (!controller)
    return false;

  Animation* animation = controller->GetAnimation(Animation::TRANSFORM);
  return animation && !animation->is_finished();
}

bool AnimationHost::FilterIsAnimatingOnImplOnly(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  if (!controller)
    return false;

  Animation* animation = controller->GetAnimation(Animation::FILTER);
  return animation && animation->is_impl_only();
}

bool AnimationHost::OpacityIsAnimatingOnImplOnly(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  if (!controller)
    return false;

  Animation* animation = controller->GetAnimation(Animation::OPACITY);
  return animation && animation->is_impl_only();
}

bool AnimationHost::TransformIsAnimatingOnImplOnly(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  if (!controller)
    return false;

  Animation* animation = controller->GetAnimation(Animation::TRANSFORM);
  return animation && animation->is_impl_only();
}

bool AnimationHost::HasFilterAnimationThatInflatesBounds(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->HasFilterAnimationThatInflatesBounds()
                    : false;
}

bool AnimationHost::HasTransformAnimationThatInflatesBounds(
    int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->HasTransformAnimationThatInflatesBounds()
                    : false;
}

bool AnimationHost::HasAnimationThatInflatesBounds(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->HasAnimationThatInflatesBounds() : false;
}

bool AnimationHost::FilterAnimationBoundsForBox(int layer_id,
                                                const gfx::BoxF& box,
                                                gfx::BoxF* bounds) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->FilterAnimationBoundsForBox(box, bounds)
                    : false;
}

bool AnimationHost::TransformAnimationBoundsForBox(int layer_id,
                                                   const gfx::BoxF& box,
                                                   gfx::BoxF* bounds) const {
  *bounds = gfx::BoxF();
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->TransformAnimationBoundsForBox(box, bounds)
                    : true;
}

bool AnimationHost::HasOnlyTranslationTransforms(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->HasOnlyTranslationTransforms() : true;
}

bool AnimationHost::AnimationsPreserveAxisAlignment(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->AnimationsPreserveAxisAlignment() : true;
}

bool AnimationHost::MaximumTargetScale(int layer_id, float* max_scale) const {
  *max_scale = 0.f;
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->MaximumTargetScale(max_scale) : true;
}

bool AnimationHost::AnimationStartScale(int layer_id,
                                        float* start_scale) const {
  *start_scale = 0.f;
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->AnimationStartScale(start_scale) : true;
}

bool AnimationHost::HasAnyAnimation(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->has_any_animation() : false;
}

bool AnimationHost::HasActiveAnimation(int layer_id) const {
  LayerAnimationController* controller = GetControllerForLayerId(layer_id);
  return controller ? controller->HasActiveAnimation() : false;
}

}  // namespace cc
