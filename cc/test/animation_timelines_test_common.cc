// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/animation_timelines_test_common.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "ui/gfx/transform.h"

namespace cc {

std::unique_ptr<TestLayer> TestLayer::Create() {
  return base::WrapUnique(new TestLayer());
}

TestLayer::TestLayer() {
  ClearMutatedProperties();
}

void TestLayer::ClearMutatedProperties() {
  transform_ = gfx::Transform();
  opacity_ = 0;
  filters_ = FilterOperations();
  scroll_offset_ = gfx::ScrollOffset();
  transform_is_animating_ = false;

  for (int i = 0; i <= TargetProperty::LAST_TARGET_PROPERTY; ++i)
    mutated_properties_[i] = false;
}

int TestLayer::transform_x() const {
  gfx::Vector2dF vec = transform_.To2dTranslation();
  return static_cast<int>(vec.x());
}

int TestLayer::transform_y() const {
  gfx::Vector2dF vec = transform_.To2dTranslation();
  return static_cast<int>(vec.y());
}

float TestLayer::brightness() const {
  for (unsigned i = 0; i < filters_.size(); ++i) {
    const FilterOperation& filter = filters_.at(i);
    if (filter.type() == FilterOperation::BRIGHTNESS)
      return filter.amount();
  }

  NOTREACHED();
  return 0;
}

TestHostClient::TestHostClient(ThreadInstance thread_instance)
    : host_(AnimationHost::Create(thread_instance)),
      mutators_need_commit_(false) {
  host_->SetMutatorHostClient(this);
  host_->SetSupportsScrollAnimations(true);
}

TestHostClient::~TestHostClient() {
  host_->SetMutatorHostClient(nullptr);
}

void TestHostClient::ClearMutatedProperties() {
  for (auto& kv : layers_in_pending_tree_)
    kv.second->ClearMutatedProperties();
  for (auto& kv : layers_in_active_tree_)
    kv.second->ClearMutatedProperties();
}

bool TestHostClient::IsLayerInTree(int layer_id,
                                   LayerTreeType tree_type) const {
  return tree_type == LayerTreeType::ACTIVE
             ? layers_in_active_tree_.count(layer_id)
             : layers_in_pending_tree_.count(layer_id);
}

void TestHostClient::SetMutatorsNeedCommit() {
  mutators_need_commit_ = true;
}

void TestHostClient::SetMutatorsNeedRebuildPropertyTrees() {}

void TestHostClient::SetLayerFilterMutated(int layer_id,
                                           LayerTreeType tree_type,
                                           const FilterOperations& filters) {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  if (layer)
    layer->set_filters(filters);
}

void TestHostClient::SetLayerOpacityMutated(int layer_id,
                                            LayerTreeType tree_type,
                                            float opacity) {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  if (layer)
    layer->set_opacity(opacity);
}

void TestHostClient::SetLayerTransformMutated(int layer_id,
                                              LayerTreeType tree_type,
                                              const gfx::Transform& transform) {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  if (layer)
    layer->set_transform(transform);
}

void TestHostClient::SetLayerScrollOffsetMutated(
    int layer_id,
    LayerTreeType tree_type,
    const gfx::ScrollOffset& scroll_offset) {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  if (layer)
    layer->set_scroll_offset(scroll_offset);
}

void TestHostClient::LayerTransformIsPotentiallyAnimatingChanged(
    int layer_id,
    LayerTreeType tree_type,
    bool is_animating) {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  if (layer)
    layer->set_transform_is_animating(is_animating);
}

void TestHostClient::SetScrollOffsetForAnimation(
    const gfx::ScrollOffset& scroll_offset) {
  scroll_offset_ = scroll_offset;
}

gfx::ScrollOffset TestHostClient::GetScrollOffsetForAnimation(
    int layer_id) const {
  return scroll_offset_;
}

void TestHostClient::RegisterLayer(int layer_id, LayerTreeType tree_type) {
  LayerIdToTestLayer& layers_in_tree = tree_type == LayerTreeType::ACTIVE
                                           ? layers_in_active_tree_
                                           : layers_in_pending_tree_;
  DCHECK(layers_in_tree.find(layer_id) == layers_in_tree.end());
  layers_in_tree[layer_id] = TestLayer::Create();

  DCHECK(host_);
  host_->RegisterLayer(layer_id, tree_type);
}

void TestHostClient::UnregisterLayer(int layer_id, LayerTreeType tree_type) {
  DCHECK(host_);
  host_->UnregisterLayer(layer_id, tree_type);

  LayerIdToTestLayer& layers_in_tree = tree_type == LayerTreeType::ACTIVE
                                           ? layers_in_active_tree_
                                           : layers_in_pending_tree_;
  auto kv = layers_in_tree.find(layer_id);
  DCHECK(kv != layers_in_tree.end());
  layers_in_tree.erase(kv);
}

bool TestHostClient::IsPropertyMutated(int layer_id,
                                       LayerTreeType tree_type,
                                       TargetProperty::Type property) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  return layer->is_property_mutated(property);
}

FilterOperations TestHostClient::GetFilters(int layer_id,
                                            LayerTreeType tree_type) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  return layer->filters();
}

float TestHostClient::GetOpacity(int layer_id, LayerTreeType tree_type) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  return layer->opacity();
}

gfx::Transform TestHostClient::GetTransform(int layer_id,
                                            LayerTreeType tree_type) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  return layer->transform();
}

gfx::ScrollOffset TestHostClient::GetScrollOffset(
    int layer_id,
    LayerTreeType tree_type) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  return layer->scroll_offset();
}

bool TestHostClient::GetTransformIsAnimating(int layer_id,
                                             LayerTreeType tree_type) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  return layer->transform_is_animating();
}

void TestHostClient::ExpectFilterPropertyMutated(int layer_id,
                                                 LayerTreeType tree_type,
                                                 float brightness) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  EXPECT_TRUE(layer->is_property_mutated(TargetProperty::FILTER));
  EXPECT_EQ(brightness, layer->brightness());
}

void TestHostClient::ExpectOpacityPropertyMutated(int layer_id,
                                                  LayerTreeType tree_type,
                                                  float opacity) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  EXPECT_TRUE(layer->is_property_mutated(TargetProperty::OPACITY));
  EXPECT_EQ(opacity, layer->opacity());
}

void TestHostClient::ExpectTransformPropertyMutated(int layer_id,
                                                    LayerTreeType tree_type,
                                                    int transform_x,
                                                    int transform_y) const {
  TestLayer* layer = FindTestLayer(layer_id, tree_type);
  EXPECT_TRUE(layer);
  EXPECT_TRUE(layer->is_property_mutated(TargetProperty::TRANSFORM));
  EXPECT_EQ(transform_x, layer->transform_x());
  EXPECT_EQ(transform_y, layer->transform_y());
}

TestLayer* TestHostClient::FindTestLayer(int layer_id,
                                         LayerTreeType tree_type) const {
  const LayerIdToTestLayer& layers_in_tree = tree_type == LayerTreeType::ACTIVE
                                                 ? layers_in_active_tree_
                                                 : layers_in_pending_tree_;
  auto kv = layers_in_tree.find(layer_id);
  if (kv == layers_in_tree.end())
    return nullptr;

  DCHECK(kv->second);
  return kv->second.get();
}

TestAnimationDelegate::TestAnimationDelegate()
    : started_(false),
      finished_(false),
      aborted_(false),
      takeover_(false),
      start_time_(base::TimeTicks()) {}

void TestAnimationDelegate::NotifyAnimationStarted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  started_ = true;
  start_time_ = monotonic_time;
}

void TestAnimationDelegate::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  finished_ = true;
}

void TestAnimationDelegate::NotifyAnimationAborted(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    int group) {
  aborted_ = true;
}

void TestAnimationDelegate::NotifyAnimationTakeover(
    base::TimeTicks monotonic_time,
    TargetProperty::Type target_property,
    double animation_start_time,
    std::unique_ptr<AnimationCurve> curve) {
  takeover_ = true;
}

AnimationTimelinesTest::AnimationTimelinesTest()
    : client_(ThreadInstance::MAIN),
      client_impl_(ThreadInstance::IMPL),
      host_(nullptr),
      host_impl_(nullptr),
      timeline_id_(AnimationIdProvider::NextTimelineId()),
      player_id_(AnimationIdProvider::NextPlayerId()),
      next_test_layer_id_(0) {
  host_ = client_.host();
  host_impl_ = client_impl_.host();

  layer_id_ = NextTestLayerId();
}

AnimationTimelinesTest::~AnimationTimelinesTest() {
}

void AnimationTimelinesTest::SetUp() {
  timeline_ = AnimationTimeline::Create(timeline_id_);
  player_ = AnimationPlayer::Create(player_id_);
}

void AnimationTimelinesTest::TearDown() {
  host_impl_->ClearTimelines();
  host_->ClearTimelines();
}

void AnimationTimelinesTest::CreateTestLayer(
    bool needs_active_value_observations,
    bool needs_pending_value_observations) {
  CreateTestMainLayer();

  if (needs_pending_value_observations)
    CreateTestImplLayer(LayerTreeType::PENDING);
  if (needs_active_value_observations)
    CreateTestImplLayer(LayerTreeType::ACTIVE);
}

void AnimationTimelinesTest::CreateTestMainLayer() {
  client_.RegisterLayer(layer_id_, LayerTreeType::ACTIVE);
}

void AnimationTimelinesTest::CreateTestImplLayer(
    LayerTreeType layer_tree_type) {
  client_impl_.RegisterLayer(layer_id_, layer_tree_type);
}

void AnimationTimelinesTest::AttachTimelinePlayerLayer() {
  host_->AddAnimationTimeline(timeline_);
  timeline_->AttachPlayer(player_);
  player_->AttachLayer(layer_id_);
}

void AnimationTimelinesTest::CreateImplTimelineAndPlayer() {
  host_->PushPropertiesTo(host_impl_);
  GetImplTimelineAndPlayerByID();
}

scoped_refptr<ElementAnimations> AnimationTimelinesTest::element_animations()
    const {
  DCHECK(player_);
  DCHECK(player_->element_animations());
  return player_->element_animations();
}

scoped_refptr<ElementAnimations>
AnimationTimelinesTest::element_animations_impl() const {
  DCHECK(player_impl_);
  DCHECK(player_impl_->element_animations());
  return player_impl_->element_animations();
}

void AnimationTimelinesTest::GetImplTimelineAndPlayerByID() {
  timeline_impl_ = host_impl_->GetTimelineById(timeline_id_);
  EXPECT_TRUE(timeline_impl_);
  player_impl_ = timeline_impl_->GetPlayerById(player_id_);
  EXPECT_TRUE(player_impl_);
}

void AnimationTimelinesTest::ReleaseRefPtrs() {
  player_ = nullptr;
  timeline_ = nullptr;
  player_impl_ = nullptr;
  timeline_impl_ = nullptr;
}

void AnimationTimelinesTest::AnimateLayersTransferEvents(
    base::TimeTicks time,
    unsigned expect_events) {
  std::unique_ptr<AnimationEvents> events = host_->CreateEvents();

  host_impl_->AnimateLayers(time);
  host_impl_->UpdateAnimationState(true, events.get());
  EXPECT_EQ(expect_events, events->events_.size());

  host_->AnimateLayers(time);
  host_->UpdateAnimationState(true, nullptr);
  host_->SetAnimationEvents(std::move(events));
}

AnimationPlayer* AnimationTimelinesTest::GetPlayerForLayerId(int layer_id) {
  const scoped_refptr<ElementAnimations> element_animations =
      host_->GetElementAnimationsForLayerId(layer_id);
  return element_animations ? element_animations->players_list().head()->value()
                            : nullptr;
}

AnimationPlayer* AnimationTimelinesTest::GetImplPlayerForLayerId(int layer_id) {
  const scoped_refptr<ElementAnimations> element_animations =
      host_impl_->GetElementAnimationsForLayerId(layer_id);
  return element_animations ? element_animations->players_list().head()->value()
                            : nullptr;
}

int AnimationTimelinesTest::NextTestLayerId() {
  next_test_layer_id_++;
  return next_test_layer_id_;
}

}  // namespace cc
