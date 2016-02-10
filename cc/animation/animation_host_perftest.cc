// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/animation_host.h"

#include "base/thread_task_runner_handle.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_player.h"
#include "cc/animation/animation_timeline.h"
#include "cc/debug/lap_timer.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_settings.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace cc {

static const int kNumberOfAnimationPlayers = 1000;

class AnimationHostPerfTest : public testing::Test {
 public:
  AnimationHostPerfTest() : fake_client_(FakeLayerTreeHostClient::DIRECT_3D) {}

 protected:
  void SetUp() override {
    LayerTreeSettings settings;
    settings.use_compositor_animation_timelines = true;

    layer_tree_host_ =
        FakeLayerTreeHost::Create(&fake_client_, &task_graph_runner_, settings);
    layer_tree_host_->InitializeSingleThreaded(
        &fake_client_, base::ThreadTaskRunnerHandle::Get(), nullptr);
  }

  void TearDown() override {
    layer_tree_host_->SetRootLayer(nullptr);
    layer_tree_host_ = nullptr;
  }

  LayerSettings GetLayerSettings() const {
    DCHECK(layer_tree_host_);

    LayerSettings settings;
    settings.use_compositor_animation_timelines =
        layer_tree_host_->settings().use_compositor_animation_timelines;
    return settings;
  }

  scoped_ptr<FakeLayerTreeHost> layer_tree_host_;
  LapTimer timer_;

  TestTaskGraphRunner task_graph_runner_;
  FakeLayerTreeHostClient fake_client_;
};

TEST_F(AnimationHostPerfTest, PushPropertiesTo) {
  if (!layer_tree_host_->settings().use_compositor_animation_timelines)
    return;

  AnimationHost* host = layer_tree_host_->animation_host();
  AnimationHost* host_impl = layer_tree_host_->host_impl()->animation_host();

  scoped_refptr<Layer> root_layer = Layer::Create(GetLayerSettings());
  layer_tree_host_->SetRootLayer(root_layer);

  scoped_ptr<LayerImpl> root_layer_impl = LayerImpl::Create(
      layer_tree_host_->host_impl()->active_tree(), root_layer->id());

  scoped_refptr<AnimationTimeline> timeline =
      AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  host->AddAnimationTimeline(timeline);

  scoped_refptr<AnimationTimeline> timeline_impl =
      timeline->CreateImplInstance();
  host_impl->AddAnimationTimeline(timeline_impl);

  for (int i = 0; i < kNumberOfAnimationPlayers; ++i) {
    scoped_refptr<Layer> layer = Layer::Create(GetLayerSettings());
    root_layer->AddChild(layer);

    const int layer_id = layer->id();

    scoped_ptr<LayerImpl> layer_impl = LayerImpl::Create(
        layer_tree_host_->host_impl()->active_tree(), layer_id);
    root_layer_impl->AddChild(std::move(layer_impl));

    scoped_refptr<AnimationPlayer> player =
        AnimationPlayer::Create(AnimationIdProvider::NextPlayerId());
    timeline->AttachPlayer(player);
    player->AttachLayer(layer_id);
    EXPECT_TRUE(player->element_animations());

    scoped_refptr<AnimationPlayer> impl_player = player->CreateImplInstance();
    timeline_impl->AttachPlayer(impl_player);
    impl_player->AttachLayer(layer_id);
    EXPECT_TRUE(impl_player->element_animations());
  }

  timer_.Reset();
  do {
    host->PushPropertiesTo(host_impl);
    timer_.NextLap();
  } while (!timer_.HasTimeLimitExpired());

  perf_test::PrintResult("push_properties_to", "", "", timer_.LapsPerSecond(),
                         "runs/s", true);
}

}  // namespace cc
