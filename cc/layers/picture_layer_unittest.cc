// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

namespace {

TEST(PictureLayerTest, NoTilesIfEmptyBounds) {
  FakeContentLayerClient client;
  client.set_bounds(gfx::Size());
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->Update();

  EXPECT_EQ(0, host->SourceFrameNumber());
  host->CommitComplete();
  EXPECT_EQ(1, host->SourceFrameNumber());

  layer->SetBounds(gfx::Size(0, 0));
  // Intentionally skipping Update since it would normally be skipped on
  // a layer with empty bounds.

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::CreateSoftware();
  FakeLayerTreeHostImpl host_impl(
      LayerTreeSettings(), &impl_task_runner_provider, &task_graph_runner);
  host_impl.InitializeRenderer(layer_tree_frame_sink.get());
  host_impl.CreatePendingTree();
  std::unique_ptr<FakePictureLayerImpl> layer_impl =
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1);

  layer->PushPropertiesTo(layer_impl.get());
  EXPECT_FALSE(layer_impl->CanHaveTilings());
  EXPECT_TRUE(layer_impl->bounds() == gfx::Size(0, 0));
  EXPECT_EQ(gfx::Size(), layer_impl->raster_source()->GetSize());
  EXPECT_FALSE(layer_impl->raster_source()->HasRecordings());
}

TEST(PictureLayerTest, InvalidateRasterAfterUpdate) {
  gfx::Size layer_size(50, 50);
  FakeContentLayerClient client;
  client.set_bounds(layer_size);
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(50, 50));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);

  gfx::Rect invalidation_bounds(layer_size);

  // The important two lines are the following:
  layer->SetNeedsDisplayRect(invalidation_bounds);
  layer->Update();

  host->CommitComplete();
  FakeImplTaskRunnerProvider impl_task_runner_provider;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d());
  LayerTreeSettings layer_tree_settings = LayerTreeSettings();
  FakeLayerTreeHostImpl host_impl(
      layer_tree_settings, &impl_task_runner_provider, &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(layer_tree_frame_sink.get());
  host_impl.CreatePendingTree();
  host_impl.pending_tree()->SetRootLayerForTesting(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));
  host_impl.pending_tree()->BuildLayerListForTesting();
  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer_for_testing());
  layer->PushPropertiesTo(layer_impl);

  EXPECT_EQ(invalidation_bounds,
            layer_impl->GetPendingInvalidation()->bounds());
}

TEST(PictureLayerTest, InvalidateRasterWithoutUpdate) {
  gfx::Size layer_size(50, 50);
  FakeContentLayerClient client;
  client.set_bounds(layer_size);
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(50, 50));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);

  gfx::Rect invalidation_bounds(layer_size);

  // The important line is the following (note that we do not call Update):
  layer->SetNeedsDisplayRect(invalidation_bounds);

  host->CommitComplete();
  FakeImplTaskRunnerProvider impl_task_runner_provider;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d());
  LayerTreeSettings layer_tree_settings = LayerTreeSettings();
  FakeLayerTreeHostImpl host_impl(
      layer_tree_settings, &impl_task_runner_provider, &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(layer_tree_frame_sink.get());
  host_impl.CreatePendingTree();
  host_impl.pending_tree()->SetRootLayerForTesting(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));
  host_impl.pending_tree()->BuildLayerListForTesting();
  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer_for_testing());
  layer->PushPropertiesTo(layer_impl);

  EXPECT_EQ(gfx::Rect(), layer_impl->GetPendingInvalidation()->bounds());
}

TEST(PictureLayerTest, ClearVisibleRectWhenNoTiling) {
  gfx::Size layer_size(50, 50);
  FakeContentLayerClient client;
  client.set_bounds(layer_size);
  client.add_draw_image(CreateDiscardablePaintImage(layer_size), gfx::Point(),
                        PaintFlags());
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->Update();

  EXPECT_EQ(0, host->SourceFrameNumber());
  host->CommitComplete();
  EXPECT_EQ(1, host->SourceFrameNumber());

  layer->Update();
  host->BuildPropertyTreesForTesting();

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d());
  LayerTreeSettings layer_tree_settings = LayerTreeSettings();
  FakeLayerTreeHostImpl host_impl(
      layer_tree_settings, &impl_task_runner_provider, &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(layer_tree_frame_sink.get()));

  host_impl.CreatePendingTree();
  host_impl.pending_tree()->SetRootLayerForTesting(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));
  host_impl.pending_tree()->BuildLayerListAndPropertyTreesForTesting();

  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer_for_testing());

  layer->PushPropertiesTo(layer_impl);

  host->CommitComplete();
  EXPECT_EQ(2, host->SourceFrameNumber());

  host_impl.ActivateSyncTree();

  // By updating the draw proprties on the active tree, we will set the viewport
  // rect for tile priorities to something non-empty.
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  host_impl.active_tree()->UpdateDrawProperties();

  layer->SetBounds(gfx::Size(11, 11));

  host_impl.CreatePendingTree();
  layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer_for_testing());

  // We should now have invalid contents and should therefore clear the
  // recording source.
  layer->PushPropertiesTo(layer_impl);
  host_impl.pending_tree()->BuildPropertyTreesForTesting();

  host_impl.ActivateSyncTree();

  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer_for_testing()->WillDraw(
      DRAW_MODE_SOFTWARE, nullptr);
  host_impl.active_tree()->root_layer_for_testing()->AppendQuads(
      render_pass.get(), &data);
  host_impl.active_tree()->root_layer_for_testing()->DidDraw(nullptr);
}

TEST(PictureLayerTest, HasSlowPaths) {
  std::unique_ptr<FakeRecordingSource> recording_source_owned(
      new FakeRecordingSource);
  FakeRecordingSource* recording_source = recording_source_owned.get();

  gfx::Size layer_bounds(200, 200);
  gfx::Rect layer_rect(layer_bounds);
  Region invalidation(layer_rect);

  FakeContentLayerClient client;
  client.set_bounds(layer_bounds);
  scoped_refptr<FakePictureLayer> layer =
      FakePictureLayer::CreateWithRecordingSource(
          &client, std::move(recording_source_owned));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);

  recording_source->SetNeedsDisplayRect(layer_rect);
  layer->Update();

  // Layer does not have slow paths by default.
  EXPECT_FALSE(layer->HasSlowPaths());

  // Add slow-path content to the client.
  client.set_contains_slow_paths(true);
  recording_source->SetNeedsDisplayRect(layer_rect);
  layer->Update();
  EXPECT_TRUE(layer->HasSlowPaths());
}

TEST(PictureLayerTest, HasNonAAPaint) {
  std::unique_ptr<FakeRecordingSource> recording_source_owned(
      new FakeRecordingSource);
  FakeRecordingSource* recording_source = recording_source_owned.get();

  gfx::Size layer_bounds(200, 200);
  gfx::Rect layer_rect(layer_bounds);
  Region invalidation(layer_rect);

  FakeContentLayerClient client;
  client.set_bounds(layer_bounds);
  scoped_refptr<FakePictureLayer> layer =
      FakePictureLayer::CreateWithRecordingSource(
          &client, std::move(recording_source_owned));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, animation_host.get());
  host->SetRootLayer(layer);

  recording_source->SetNeedsDisplayRect(layer_rect);
  layer->Update();

  // Layer does not have non-aa paint by default.
  EXPECT_FALSE(layer->HasNonAAPaint());

  // Add non-aa content to the client.
  client.add_draw_rect(layer_rect, PaintFlags());
  recording_source->SetNeedsDisplayRect(layer_rect);
  layer->Update();
  EXPECT_TRUE(layer->HasNonAAPaint());
}

// PicturePile uses the source frame number as a unit for measuring invalidation
// frequency. When a pile moves between compositors, the frame number increases
// non-monotonically. This executes that code path under this scenario allowing
// for the code to verify correctness with DCHECKs.
TEST(PictureLayerTest, NonMonotonicSourceFrameNumber) {
  LayerTreeSettings settings = LayerTreeSettings();
  settings.single_thread_proxy_scheduler = false;
  settings.use_zero_copy = true;

  StubLayerTreeHostSingleThreadClient single_thread_client;
  FakeLayerTreeHostClient host_client1;
  FakeLayerTreeHostClient host_client2;
  TestTaskGraphRunner task_graph_runner;

  FakeContentLayerClient client;
  client.set_bounds(gfx::Size());
  scoped_refptr<FakePictureLayer> layer = FakePictureLayer::Create(&client);

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

  LayerTreeHost::InitParams params;
  params.client = &host_client1;
  params.settings = &settings;
  params.task_graph_runner = &task_graph_runner;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.mutator_host = animation_host.get();
  std::unique_ptr<LayerTreeHost> host1 =
      LayerTreeHost::CreateSingleThreaded(&single_thread_client, &params);
  host1->SetVisible(true);
  host_client1.SetLayerTreeHost(host1.get());

  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

  LayerTreeHost::InitParams params2;
  params2.client = &host_client1;
  params2.settings = &settings;
  params2.task_graph_runner = &task_graph_runner;
  params2.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params2.client = &host_client2;
  params2.mutator_host = animation_host2.get();
  std::unique_ptr<LayerTreeHost> host2 =
      LayerTreeHost::CreateSingleThreaded(&single_thread_client, &params2);
  host2->SetVisible(true);
  host_client2.SetLayerTreeHost(host2.get());

  // The PictureLayer is put in one LayerTreeHost.
  host1->SetRootLayer(layer);
  // Do a main frame, record the picture layers.
  EXPECT_EQ(0, layer->update_count());
  layer->SetNeedsDisplay();
  host1->Composite(base::TimeTicks::Now());
  EXPECT_EQ(1, layer->update_count());
  EXPECT_EQ(1, host1->SourceFrameNumber());

  // The source frame number in |host1| is now higher than host2.
  layer->SetNeedsDisplay();
  host1->Composite(base::TimeTicks::Now());
  EXPECT_EQ(2, layer->update_count());
  EXPECT_EQ(2, host1->SourceFrameNumber());

  // Then moved to another LayerTreeHost.
  host1->SetRootLayer(nullptr);
  host2->SetRootLayer(layer);

  // Do a main frame, record the picture layers. The frame number has changed
  // non-monotonically.
  layer->SetNeedsDisplay();
  host2->Composite(base::TimeTicks::Now());
  EXPECT_EQ(3, layer->update_count());
  EXPECT_EQ(1, host2->SourceFrameNumber());

  animation_host->SetMutatorHostClient(nullptr);
  animation_host2->SetMutatorHostClient(nullptr);
}

// Verify that PictureLayer::DropRecordingSourceContentIfInvalid does not
// assert when changing frames.
TEST(PictureLayerTest, ChangingHostsWithCollidingFrames) {
  LayerTreeSettings settings = LayerTreeSettings();
  settings.single_thread_proxy_scheduler = false;

  StubLayerTreeHostSingleThreadClient single_thread_client;
  FakeLayerTreeHostClient host_client1;
  FakeLayerTreeHostClient host_client2;
  TestTaskGraphRunner task_graph_runner;

  FakeContentLayerClient client;
  client.set_bounds(gfx::Size());
  scoped_refptr<FakePictureLayer> layer = FakePictureLayer::Create(&client);

  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

  LayerTreeHost::InitParams params;
  params.client = &host_client1;
  params.settings = &settings;
  params.task_graph_runner = &task_graph_runner;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.mutator_host = animation_host.get();
  std::unique_ptr<LayerTreeHost> host1 =
      LayerTreeHost::CreateSingleThreaded(&single_thread_client, &params);
  host1->SetVisible(true);
  host_client1.SetLayerTreeHost(host1.get());

  auto animation_host2 = AnimationHost::CreateForTesting(ThreadInstance::MAIN);

  LayerTreeHost::InitParams params2;
  params2.client = &host_client1;
  params2.settings = &settings;
  params2.task_graph_runner = &task_graph_runner;
  params2.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params2.client = &host_client2;
  params2.mutator_host = animation_host2.get();
  std::unique_ptr<LayerTreeHost> host2 =
      LayerTreeHost::CreateSingleThreaded(&single_thread_client, &params2);
  host2->SetVisible(true);
  host_client2.SetLayerTreeHost(host2.get());

  // The PictureLayer is put in one LayerTreeHost.
  host1->SetRootLayer(layer);
  // Do a main frame, record the picture layers.
  EXPECT_EQ(0, layer->update_count());
  layer->SetBounds(gfx::Size(500, 500));
  host1->Composite(base::TimeTicks::Now());
  EXPECT_EQ(1, layer->update_count());
  EXPECT_EQ(1, host1->SourceFrameNumber());
  EXPECT_EQ(gfx::Size(500, 500), layer->bounds());

  // Then moved to another LayerTreeHost.
  host1->SetRootLayer(nullptr);
  scoped_refptr<Layer> root = Layer::Create();
  host2->SetRootLayer(root);
  root->AddChild(layer);

  // Make the layer not update.
  layer->SetHideLayerAndSubtree(true);
  EXPECT_EQ(gfx::Size(500, 500),
            layer->GetRecordingSourceForTesting()->GetSize());

  // Change its bounds while it's in a state that can't update.
  layer->SetBounds(gfx::Size(600, 600));
  host2->Composite(base::TimeTicks::Now());

  // This layer should not have been updated because it is invisible.
  EXPECT_EQ(1, layer->update_count());
  EXPECT_EQ(1, host2->SourceFrameNumber());

  // This layer should also drop its recording source because it was resized
  // and not recorded.
  EXPECT_EQ(gfx::Size(), layer->GetRecordingSourceForTesting()->GetSize());
}

}  // namespace
}  // namespace cc
