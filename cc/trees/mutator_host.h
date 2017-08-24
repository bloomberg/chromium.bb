// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_MUTATOR_HOST_H_
#define CC_TREES_MUTATOR_HOST_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "cc/trees/element_id.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/mutator_host_client.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class ScrollOffset;
}

namespace cc {

class MutatorEvents;
class MutatorHostClient;
class LayerTreeMutator;

// A MutatorHost owns all the animation and mutation effects.
// There is just one MutatorHost for LayerTreeHost on main renderer thread
// and just one MutatorHost for LayerTreeHostImpl on the impl thread.
// We synchronize them during the commit in a one-way data-flow process
// (PushPropertiesTo).
// A MutatorHost talks to its correspondent LayerTreeHost via
// MutatorHostClient interface.
class MutatorHost {
 public:
  virtual ~MutatorHost() {}

  virtual std::unique_ptr<MutatorHost> CreateImplInstance(
      bool supports_impl_scrolling) const = 0;

  virtual void ClearMutators() = 0;

  virtual void RegisterElement(ElementId element_id,
                               ElementListType list_type) = 0;
  virtual void UnregisterElement(ElementId element_id,
                                 ElementListType list_type) = 0;

  virtual void SetMutatorHostClient(MutatorHostClient* client) = 0;

  virtual void SetLayerTreeMutator(
      std::unique_ptr<LayerTreeMutator> mutator) = 0;

  virtual void PushPropertiesTo(MutatorHost* host_impl) = 0;

  virtual void SetSupportsScrollAnimations(bool supports_scroll_animations) = 0;
  virtual bool NeedsTickAnimations() const = 0;

  virtual bool ActivateAnimations() = 0;
  virtual bool TickAnimations(base::TimeTicks monotonic_time) = 0;
  // Tick animations that depends on scroll offset.
  virtual void TickScrollAnimations(base::TimeTicks monotonic_time) = 0;
  virtual bool UpdateAnimationState(bool start_ready_animations,
                                    MutatorEvents* events) = 0;

  // Returns a callback which is responsible for applying layer tree mutations
  // to DOM elements. The callback are transfered to main thread when we begin
  // main frame and should only be invoked on the main thread.
  //
  // TODO(majidvp): http://crbug.com/756539 Eliminate this. Once we implement
  // syncing animation local times from animation worklet to cc then we should
  // not directly send mutations to main thread and eliminate of this method.
  // Instead the animation local times should be send to main thread via
  // existing cc->main plumbing for animations which is |SetAnimationEvents|.
  virtual base::Closure TakeMutations() = 0;

  virtual std::unique_ptr<MutatorEvents> CreateEvents() = 0;
  virtual void SetAnimationEvents(std::unique_ptr<MutatorEvents> events) = 0;

  virtual bool ScrollOffsetAnimationWasInterrupted(
      ElementId element_id) const = 0;

  virtual bool IsAnimatingFilterProperty(ElementId element_id,
                                         ElementListType list_type) const = 0;
  virtual bool IsAnimatingOpacityProperty(ElementId element_id,
                                          ElementListType list_type) const = 0;
  virtual bool IsAnimatingTransformProperty(
      ElementId element_id,
      ElementListType list_type) const = 0;

  virtual bool HasPotentiallyRunningFilterAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool HasPotentiallyRunningOpacityAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool HasPotentiallyRunningTransformAnimation(
      ElementId element_id,
      ElementListType list_type) const = 0;

  virtual bool HasAnyAnimationTargetingProperty(
      ElementId element_id,
      TargetProperty::Type property) const = 0;

  virtual bool HasTransformAnimationThatInflatesBounds(
      ElementId element_id) const = 0;

  virtual bool TransformAnimationBoundsForBox(ElementId element_id,
                                              const gfx::BoxF& box,
                                              gfx::BoxF* bounds) const = 0;

  virtual bool HasOnlyTranslationTransforms(
      ElementId element_id,
      ElementListType list_type) const = 0;
  virtual bool AnimationsPreserveAxisAlignment(ElementId element_id) const = 0;

  virtual bool MaximumTargetScale(ElementId element_id,
                                  ElementListType list_type,
                                  float* max_scale) const = 0;
  virtual bool AnimationStartScale(ElementId element_id,
                                   ElementListType list_type,
                                   float* start_scale) const = 0;

  virtual bool HasAnyAnimation(ElementId element_id) const = 0;
  virtual bool HasTickingAnimationForTesting(ElementId element_id) const = 0;

  virtual void ImplOnlyScrollAnimationCreate(
      ElementId element_id,
      const gfx::ScrollOffset& target_offset,
      const gfx::ScrollOffset& current_offset,
      base::TimeDelta delayed_by,
      base::TimeDelta animation_start_offset) = 0;
  virtual bool ImplOnlyScrollAnimationUpdateTarget(
      ElementId element_id,
      const gfx::Vector2dF& scroll_delta,
      const gfx::ScrollOffset& max_scroll_offset,
      base::TimeTicks frame_monotonic_time,
      base::TimeDelta delayed_by) = 0;

  virtual void ScrollAnimationAbort() = 0;
};

class MutatorEvents {
 public:
  virtual ~MutatorEvents() {}
  virtual bool IsEmpty() const = 0;
};

}  // namespace cc

#endif  // CC_TREES_MUTATOR_HOST_H_
