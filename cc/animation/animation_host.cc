// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_host.h"

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animations.h"
#include "cc/animation/scroll_offset_animations_impl.h"
#include "cc/animation/timing_function.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

std::unique_ptr<AnimationHost> AnimationHost::CreateMainInstance() {
  return base::WrapUnique(new AnimationHost(ThreadInstance::MAIN));
}

std::unique_ptr<AnimationHost> AnimationHost::CreateForTesting(
    ThreadInstance thread_instance) {
  auto animation_host = base::WrapUnique(new AnimationHost(thread_instance));

  if (thread_instance == ThreadInstance::IMPL)
    animation_host->SetSupportsScrollAnimations(true);

  return animation_host;
}

AnimationHost::AnimationHost(ThreadInstance thread_instance)
    : mutator_host_client_(nullptr),
      thread_instance_(thread_instance),
      supports_scroll_animations_(false),
      needs_push_properties_(false) {
  if (thread_instance_ == ThreadInstance::IMPL) {
    scroll_offset_animations_impl_ =
        base::MakeUnique<ScrollOffsetAnimationsImpl>(this);
  } else {
    scroll_offset_animations_ = base::MakeUnique<ScrollOffsetAnimations>(this);
  }
}

AnimationHost::~AnimationHost() {
  scroll_offset_animations_impl_ = nullptr;

  ClearMutators();
  DCHECK(!mutator_host_client());
  DCHECK(element_to_animations_map_.empty());
}

std::unique_ptr<MutatorHost> AnimationHost::CreateImplInstance(
    bool supports_impl_scrolling) const {
  DCHECK_EQ(thread_instance_, ThreadInstance::MAIN);

  auto mutator_host_impl =
      base::WrapUnique<MutatorHost>(new AnimationHost(ThreadInstance::IMPL));
  mutator_host_impl->SetSupportsScrollAnimations(supports_impl_scrolling);
  return mutator_host_impl;
}

AnimationTimeline* AnimationHost::GetTimelineById(int timeline_id) const {
  auto f = id_to_timeline_map_.find(timeline_id);
  return f == id_to_timeline_map_.end() ? nullptr : f->second.get();
}

void AnimationHost::ClearMutators() {
  for (auto& kv : id_to_timeline_map_)
    EraseTimeline(kv.second);
  id_to_timeline_map_.clear();
}

void AnimationHost::EraseTimeline(scoped_refptr<AnimationTimeline> timeline) {
  timeline->ClearPlayers();
  timeline->SetAnimationHost(nullptr);
}

void AnimationHost::AddAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  DCHECK(timeline->id());
  timeline->SetAnimationHost(this);
  id_to_timeline_map_.insert(
      std::make_pair(timeline->id(), std::move(timeline)));
  SetNeedsPushProperties();
}

void AnimationHost::RemoveAnimationTimeline(
    scoped_refptr<AnimationTimeline> timeline) {
  DCHECK(timeline->id());
  EraseTimeline(timeline);
  id_to_timeline_map_.erase(timeline->id());
  SetNeedsPushProperties();
}

void AnimationHost::RegisterElement(ElementId element_id,
                                    ElementListType list_type) {
  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (element_animations)
    element_animations->ElementRegistered(element_id, list_type);
}

void AnimationHost::UnregisterElement(ElementId element_id,
                                      ElementListType list_type) {
  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (element_animations)
    element_animations->ElementUnregistered(element_id, list_type);
}

void AnimationHost::RegisterPlayerForElement(ElementId element_id,
                                             AnimationPlayer* player) {
  DCHECK(element_id);
  DCHECK(player);

  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  if (!element_animations) {
    element_animations = ElementAnimations::Create();
    element_animations->SetElementId(element_id);
    element_to_animations_map_[element_animations->element_id()] =
        element_animations;
  }

  if (element_animations->animation_host() != this) {
    element_animations->SetAnimationHost(this);
    element_animations->InitAffectedElementTypes();
  }

  element_animations->AddPlayer(player);
}

void AnimationHost::UnregisterPlayerForElement(ElementId element_id,
                                               AnimationPlayer* player) {
  DCHECK(element_id);
  DCHECK(player);

  scoped_refptr<ElementAnimations> element_animations =
      GetElementAnimationsForElementId(element_id);
  DCHECK(element_animations);
  element_animations->RemovePlayer(player);

  if (element_animations->IsEmpty()) {
    element_animations->ClearAffectedElementTypes();
    element_to_animations_map_.erase(element_animations->element_id());
    element_animations->SetAnimationHost(nullptr);
  }

  RemoveFromTicking(player);
}

void AnimationHost::SetMutatorHostClient(MutatorHostClient* client) {
  if (mutator_host_client_ == client)
    return;

  mutator_host_client_ = client;

  if (needs_push_properties() && mutator_host_client())
    SetNeedsPushProperties();
}

void AnimationHost::SetNeedsCommit() {
  DCHECK(mutator_host_client_);
  mutator_host_client_->SetMutatorsNeedCommit();
  // TODO(loyso): Invalidate property trees only if really needed.
  mutator_host_client_->SetMutatorsNeedRebuildPropertyTrees();
}

void AnimationHost::SetNeedsPushProperties() {
  needs_push_properties_ = true;
}

void AnimationHost::PushPropertiesTo(MutatorHost* mutator_host_impl) {
  auto* host_impl = static_cast<AnimationHost*>(mutator_host_impl);

  if (needs_push_properties_) {
    needs_push_properties_ = false;
    PushTimelinesToImplThread(host_impl);
    RemoveTimelinesFromImplThread(host_impl);
    PushPropertiesToImplThread(host_impl);
    // This is redundant but used in tests.
    host_impl->needs_push_properties_ = false;
  }
}

void AnimationHost::PushTimelinesToImplThread(AnimationHost* host_impl) const {
  for (auto& kv : id_to_timeline_map_) {
    auto& timeline = kv.second;
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
  IdToTimelineMap& timelines_impl = host_impl->id_to_timeline_map_;

  // Erase all the impl timelines which |this| doesn't have.
  for (auto it = timelines_impl.begin(); it != timelines_impl.end();) {
    auto& timeline_impl = it->second;
    if (timeline_impl->is_impl_only() || GetTimelineById(timeline_impl->id())) {
      ++it;
    } else {
      host_impl->EraseTimeline(it->second);
      it = timelines_impl.erase(it);
    }
  }
}

void AnimationHost::PushPropertiesToImplThread(AnimationHost* host_impl) {
  // Sync all players with impl thread to create ElementAnimations. This needs
  // to happen before the element animations are synced below.
  for (auto& kv : id_to_timeline_map_) {
    AnimationTimeline* timeline = kv.second.get();
    if (timeline->needs_push_properties()) {
      AnimationTimeline* timeline_impl =
          host_impl->GetTimelineById(timeline->id());
      if (timeline_impl)
        timeline->PushPropertiesTo(timeline_impl);
    }
  }

  // Sync properties for created ElementAnimations.
  for (auto& kv : element_to_animations_map_) {
    const auto& element_animations = kv.second;
    if (element_animations->needs_push_properties()) {
      auto element_animations_impl =
          host_impl->GetElementAnimationsForElementId(kv.first);
      if (element_animations_impl)
        element_animations->PushPropertiesTo(
            std::move(element_animations_impl));
    }
  }

  // Update the impl-only scroll offset animations.
  scroll_offset_animations_->PushPropertiesTo(
      host_impl->scroll_offset_animations_impl_.get());
}

scoped_refptr<ElementAnimations>
AnimationHost::GetElementAnimationsForElementId(ElementId element_id) const {
  if (!element_id)
    return nullptr;
  auto iter = element_to_animations_map_.find(element_id);
  return iter == element_to_animations_map_.end() ? nullptr : iter->second;
}

void AnimationHost::SetSupportsScrollAnimations(
    bool supports_scroll_animations) {
  supports_scroll_animations_ = supports_scroll_animations;
}

bool AnimationHost::SupportsScrollAnimations() const {
  return supports_scroll_animations_;
}

bool AnimationHost::NeedsTickAnimations() const {
  return !ticking_players_.empty();
}

bool AnimationHost::ActivateAnimations() {
  if (!NeedsTickAnimations())
    return false;

  TRACE_EVENT0("cc", "AnimationHost::ActivateAnimations");
  PlayersList ticking_players_copy = ticking_players_;
  for (auto& it : ticking_players_copy)
    it->ActivateAnimations();

  return true;
}

bool AnimationHost::TickAnimations(base::TimeTicks monotonic_time) {
  if (!NeedsTickAnimations())
    return false;

  TRACE_EVENT0("cc", "AnimationHost::TickAnimations");
  PlayersList ticking_players_copy = ticking_players_;
  for (auto& it : ticking_players_copy)
    it->Tick(monotonic_time);

  return true;
}

bool AnimationHost::UpdateAnimationState(bool start_ready_animations,
                                         MutatorEvents* mutator_events) {
  if (!NeedsTickAnimations())
    return false;

  auto* animation_events = static_cast<AnimationEvents*>(mutator_events);

  TRACE_EVENT0("cc", "AnimationHost::UpdateAnimationState");
  PlayersList ticking_players_copy = ticking_players_;
  for (auto& it : ticking_players_copy)
    it->UpdateState(start_ready_animations, animation_events);

  return true;
}

std::unique_ptr<MutatorEvents> AnimationHost::CreateEvents() {
  return base::MakeUnique<AnimationEvents>();
}

void AnimationHost::SetAnimationEvents(
    std::unique_ptr<MutatorEvents> mutator_events) {
  auto events =
      base::WrapUnique(static_cast<AnimationEvents*>(mutator_events.release()));

  for (size_t event_index = 0; event_index < events->events_.size();
       ++event_index) {
    ElementId element_id = events->events_[event_index].element_id;

    // Use the map of all ElementAnimations, not just ticking players, since
    // non-ticking Players may still receive events for impl-only animations.
    const ElementToAnimationsMap& all_element_animations =
        element_to_animations_map_;
    auto iter = all_element_animations.find(element_id);
    if (iter != all_element_animations.end()) {
      switch (events->events_[event_index].type) {
        case AnimationEvent::STARTED:
          (*iter).second->NotifyAnimationStarted(events->events_[event_index]);
          break;

        case AnimationEvent::FINISHED:
          (*iter).second->NotifyAnimationFinished(events->events_[event_index]);
          break;

        case AnimationEvent::ABORTED:
          (*iter).second->NotifyAnimationAborted(events->events_[event_index]);
          break;

        case AnimationEvent::TAKEOVER:
          (*iter).second->NotifyAnimationTakeover(events->events_[event_index]);
          break;
      }
    }
  }
}

bool AnimationHost::ScrollOffsetAnimationWasInterrupted(
    ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->ScrollOffsetAnimationWasInterrupted()
             : false;
}

bool AnimationHost::IsAnimatingFilterProperty(ElementId element_id,
                                              ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsCurrentlyAnimatingProperty(
                   TargetProperty::FILTER, list_type)
             : false;
}

bool AnimationHost::IsAnimatingOpacityProperty(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsCurrentlyAnimatingProperty(
                   TargetProperty::OPACITY, list_type)
             : false;
}

bool AnimationHost::IsAnimatingTransformProperty(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsCurrentlyAnimatingProperty(
                   TargetProperty::TRANSFORM, list_type)
             : false;
}

bool AnimationHost::HasPotentiallyRunningFilterAnimation(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(
                   TargetProperty::FILTER, list_type)
             : false;
}

bool AnimationHost::HasPotentiallyRunningOpacityAnimation(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(
                   TargetProperty::OPACITY, list_type)
             : false;
}

bool AnimationHost::HasPotentiallyRunningTransformAnimation(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->IsPotentiallyAnimatingProperty(
                   TargetProperty::TRANSFORM, list_type)
             : false;
}

bool AnimationHost::HasAnyAnimationTargetingProperty(
    ElementId element_id,
    TargetProperty::Type property) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  if (!element_animations)
    return false;

  return element_animations->HasAnyAnimationTargetingProperty(property);
}

bool AnimationHost::HasFilterAnimationThatInflatesBounds(
    ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->HasFilterAnimationThatInflatesBounds()
             : false;
}

bool AnimationHost::HasTransformAnimationThatInflatesBounds(
    ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->HasTransformAnimationThatInflatesBounds()
             : false;
}

bool AnimationHost::HasAnimationThatInflatesBounds(ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->HasAnimationThatInflatesBounds()
             : false;
}

bool AnimationHost::FilterAnimationBoundsForBox(ElementId element_id,
                                                const gfx::BoxF& box,
                                                gfx::BoxF* bounds) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->FilterAnimationBoundsForBox(box, bounds)
             : false;
}

bool AnimationHost::TransformAnimationBoundsForBox(ElementId element_id,
                                                   const gfx::BoxF& box,
                                                   gfx::BoxF* bounds) const {
  *bounds = gfx::BoxF();
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->TransformAnimationBoundsForBox(box, bounds)
             : true;
}

bool AnimationHost::HasOnlyTranslationTransforms(
    ElementId element_id,
    ElementListType list_type) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->HasOnlyTranslationTransforms(list_type)
             : true;
}

bool AnimationHost::AnimationsPreserveAxisAlignment(
    ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->AnimationsPreserveAxisAlignment()
             : true;
}

bool AnimationHost::MaximumTargetScale(ElementId element_id,
                                       ElementListType list_type,
                                       float* max_scale) const {
  *max_scale = 0.f;
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->MaximumTargetScale(list_type, max_scale)
             : true;
}

bool AnimationHost::AnimationStartScale(ElementId element_id,
                                        ElementListType list_type,
                                        float* start_scale) const {
  *start_scale = 0.f;
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations
             ? element_animations->AnimationStartScale(list_type, start_scale)
             : true;
}

bool AnimationHost::HasAnyAnimation(ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->HasAnyAnimation() : false;
}

bool AnimationHost::HasTickingAnimationForTesting(ElementId element_id) const {
  auto element_animations = GetElementAnimationsForElementId(element_id);
  return element_animations ? element_animations->HasTickingAnimation() : false;
}

void AnimationHost::ImplOnlyScrollAnimationCreate(
    ElementId element_id,
    const gfx::ScrollOffset& target_offset,
    const gfx::ScrollOffset& current_offset,
    base::TimeDelta delayed_by,
    base::TimeDelta animation_start_offset) {
  DCHECK(scroll_offset_animations_impl_);
  scroll_offset_animations_impl_->ScrollAnimationCreate(
      element_id, target_offset, current_offset, delayed_by,
      animation_start_offset);
}

bool AnimationHost::ImplOnlyScrollAnimationUpdateTarget(
    ElementId element_id,
    const gfx::Vector2dF& scroll_delta,
    const gfx::ScrollOffset& max_scroll_offset,
    base::TimeTicks frame_monotonic_time,
    base::TimeDelta delayed_by) {
  DCHECK(scroll_offset_animations_impl_);
  return scroll_offset_animations_impl_->ScrollAnimationUpdateTarget(
      element_id, scroll_delta, max_scroll_offset, frame_monotonic_time,
      delayed_by);
}

ScrollOffsetAnimations& AnimationHost::scroll_offset_animations() const {
  DCHECK(scroll_offset_animations_);
  return *scroll_offset_animations_.get();
}

void AnimationHost::ScrollAnimationAbort() {
  DCHECK(scroll_offset_animations_impl_);
  scroll_offset_animations_impl_->ScrollAnimationAbort(
      false /* needs_completion */);
}

void AnimationHost::AddToTicking(scoped_refptr<AnimationPlayer> player) {
  DCHECK(std::find(ticking_players_.begin(), ticking_players_.end(), player) ==
         ticking_players_.end());
  ticking_players_.push_back(player);
}

void AnimationHost::RemoveFromTicking(scoped_refptr<AnimationPlayer> player) {
  auto to_erase =
      std::find(ticking_players_.begin(), ticking_players_.end(), player);
  if (to_erase != ticking_players_.end())
    ticking_players_.erase(to_erase);
}

const AnimationHost::PlayersList& AnimationHost::ticking_players_for_testing()
    const {
  return ticking_players_;
}

const AnimationHost::ElementToAnimationsMap&
AnimationHost::element_animations_for_testing() const {
  return element_to_animations_map_;
}

}  // namespace cc
