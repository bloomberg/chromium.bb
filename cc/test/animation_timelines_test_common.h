// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ANIMATION_TIMELINES_TEST_COMMON_H_
#define CC_TEST_ANIMATION_TIMELINES_TEST_COMMON_H_

#include <memory>
#include <unordered_map>

#include "cc/animation/animation.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/animation_host.h"
#include "cc/trees/mutator_host_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace cc {

class TestLayer {
 public:
  static std::unique_ptr<TestLayer> Create();
  ~TestLayer();

  void ClearMutatedProperties();

  int transform_x() const;
  int transform_y() const;
  float brightness() const;

  const gfx::Transform& transform() const { return transform_; }
  void set_transform(const gfx::Transform& transform) {
    transform_ = transform;
    mutated_properties_[TargetProperty::TRANSFORM] = true;
  }

  float opacity() const { return opacity_; }
  void set_opacity(float opacity) {
    opacity_ = opacity;
    mutated_properties_[TargetProperty::OPACITY] = true;
  }

  FilterOperations filters() const { return filters_; }
  void set_filters(const FilterOperations& filters) {
    filters_ = filters;
    mutated_properties_[TargetProperty::FILTER] = true;
  }

  gfx::ScrollOffset scroll_offset() const { return scroll_offset_; }
  void set_scroll_offset(const gfx::ScrollOffset& scroll_offset) {
    scroll_offset_ = scroll_offset;
    mutated_properties_[TargetProperty::SCROLL_OFFSET] = true;
  }

  bool is_currently_animating(TargetProperty::Type property) const {
    return is_currently_animating_[property];
  }
  void set_is_currently_animating(TargetProperty::Type property,
                                  bool is_animating) {
    is_currently_animating_[property] = is_animating;
  }

  bool has_potential_animation(TargetProperty::Type property) const {
    return has_potential_animation_[property];
  }
  void set_has_potential_animation(TargetProperty::Type property,
                                   bool is_animating) {
    has_potential_animation_[property] = is_animating;
  }

  bool is_property_mutated(TargetProperty::Type property) const {
    return mutated_properties_[property];
  }

 private:
  TestLayer();

  gfx::Transform transform_;
  float opacity_;
  FilterOperations filters_;
  gfx::ScrollOffset scroll_offset_;

  TargetProperties has_potential_animation_;
  TargetProperties is_currently_animating_;
  TargetProperties mutated_properties_;
};

class TestHostClient : public MutatorHostClient {
 public:
  explicit TestHostClient(ThreadInstance thread_instance);
  ~TestHostClient();

  void ClearMutatedProperties();

  bool IsElementInList(ElementId element_id,
                       ElementListType list_type) const override;

  void SetMutatorsNeedCommit() override;
  void SetMutatorsNeedRebuildPropertyTrees() override;

  void SetElementFilterMutated(ElementId element_id,
                               ElementListType list_type,
                               const FilterOperations& filters) override;

  void SetElementOpacityMutated(ElementId element_id,
                                ElementListType list_type,
                                float opacity) override;

  void SetElementTransformMutated(ElementId element_id,
                                  ElementListType list_type,
                                  const gfx::Transform& transform) override;

  void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) override;

  void ElementIsAnimatingChanged(ElementId element_id,
                                 ElementListType list_type,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) override;

  void ScrollOffsetAnimationFinished() override {}

  void SetScrollOffsetForAnimation(const gfx::ScrollOffset& scroll_offset);
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  bool mutators_need_commit() const { return mutators_need_commit_; }
  void set_mutators_need_commit(bool need) { mutators_need_commit_ = need; }

  void RegisterElement(ElementId element_id, ElementListType list_type);
  void UnregisterElement(ElementId element_id, ElementListType list_type);

  AnimationHost* host() {
    DCHECK(host_);
    return host_.get();
  }

  bool IsPropertyMutated(ElementId element_id,
                         ElementListType list_type,
                         TargetProperty::Type property) const;

  FilterOperations GetFilters(ElementId element_id,
                              ElementListType list_type) const;
  float GetOpacity(ElementId element_id, ElementListType list_type) const;
  gfx::Transform GetTransform(ElementId element_id,
                              ElementListType list_type) const;
  gfx::ScrollOffset GetScrollOffset(ElementId element_id,
                                    ElementListType list_type) const;
  bool GetHasPotentialTransformAnimation(ElementId element_id,
                                         ElementListType list_type) const;
  bool GetTransformIsCurrentlyAnimating(ElementId element_id,
                                        ElementListType list_type) const;
  bool GetOpacityIsCurrentlyAnimating(ElementId element_id,
                                      ElementListType list_type) const;
  bool GetHasPotentialOpacityAnimation(ElementId element_id,
                                       ElementListType list_type) const;
  bool GetHasPotentialFilterAnimation(ElementId element_id,
                                      ElementListType list_type) const;
  bool GetFilterIsCurrentlyAnimating(ElementId element_id,
                                     ElementListType list_type) const;

  void ExpectFilterPropertyMutated(ElementId element_id,
                                   ElementListType list_type,
                                   float brightness) const;
  void ExpectOpacityPropertyMutated(ElementId element_id,
                                    ElementListType list_type,
                                    float opacity) const;
  void ExpectTransformPropertyMutated(ElementId element_id,
                                      ElementListType list_type,
                                      int transform_x,
                                      int transform_y) const;

  TestLayer* FindTestLayer(ElementId element_id,
                           ElementListType list_type) const;

 private:
  std::unique_ptr<AnimationHost> host_;

  using ElementIdToTestLayer =
      std::unordered_map<ElementId, std::unique_ptr<TestLayer>, ElementIdHash>;
  ElementIdToTestLayer layers_in_active_tree_;
  ElementIdToTestLayer layers_in_pending_tree_;

  gfx::ScrollOffset scroll_offset_;
  bool mutators_need_commit_;
};

class TestAnimationDelegate : public AnimationDelegate {
 public:
  TestAnimationDelegate();

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

  bool started() { return started_; }

  bool finished() { return finished_; }

  bool aborted() { return aborted_; }

  bool takeover() { return takeover_; }

  base::TimeTicks start_time() { return start_time_; }

 private:
  bool started_;
  bool finished_;
  bool aborted_;
  bool takeover_;
  base::TimeTicks start_time_;
};

class AnimationTimelinesTest : public testing::Test {
 public:
  AnimationTimelinesTest();
  ~AnimationTimelinesTest() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  void GetImplTimelineAndPlayerByID();

  void CreateTestLayer(bool needs_active_value_observations,
                       bool needs_pending_value_observations);
  void AttachTimelinePlayerLayer();
  void CreateImplTimelineAndPlayer();

  void CreateTestMainLayer();
  void DestroyTestMainLayer();
  void CreateTestImplLayer(ElementListType element_list_type);

  void ReleaseRefPtrs();

  void TickAnimationsTransferEvents(base::TimeTicks time,
                                    unsigned expect_events);

  AnimationPlayer* GetPlayerForElementId(ElementId element_id);
  AnimationPlayer* GetImplPlayerForLayerId(ElementId element_id);

  int NextTestLayerId();

  bool CheckPlayerTimelineNeedsPushProperties(bool needs_push_properties) const;

  void PushProperties();

  TestHostClient client_;
  TestHostClient client_impl_;

  AnimationHost* host_;
  AnimationHost* host_impl_;

  const int timeline_id_;
  const int player_id_;
  ElementId element_id_;

  int next_test_layer_id_;

  scoped_refptr<AnimationTimeline> timeline_;
  scoped_refptr<AnimationPlayer> player_;
  scoped_refptr<ElementAnimations> element_animations_;

  scoped_refptr<AnimationTimeline> timeline_impl_;
  scoped_refptr<AnimationPlayer> player_impl_;
  scoped_refptr<ElementAnimations> element_animations_impl_;
};

}  // namespace cc

#endif  // CC_TEST_ANIMATION_TIMELINES_TEST_COMMON_H_
