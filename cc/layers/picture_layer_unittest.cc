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
#include "cc/layers/empty_content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/proto/layer.pb.h"
#include "cc/test/fake_client_picture_cache.h"
#include "cc/test/fake_compositor_frame_sink.h"
#include "cc/test/fake_engine_picture_cache.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/layer_tree_settings_for_testing.h"
#include "cc/test/skia_common.h"
#include "cc/test/stub_layer_tree_host_single_thread_client.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {

class TestSerializationPictureLayer : public PictureLayer {
 public:
  static scoped_refptr<TestSerializationPictureLayer> Create(
      const gfx::Size& recording_source_viewport) {
    return make_scoped_refptr(new TestSerializationPictureLayer(
        EmptyContentLayerClient::GetInstance(),
        FakeRecordingSource::CreateFilledRecordingSource(
            recording_source_viewport),
        recording_source_viewport));
  }

  FakeRecordingSource* recording_source() {
    return static_cast<FakeRecordingSource*>(recording_source_.get());
  }

  void set_invalidation(const Region& invalidation) {
    last_updated_invalidation_ = invalidation;
  }

  void set_update_source_frame_number(int number) {
    update_source_frame_number_ = number;
  }

  void set_is_mask(bool is_mask) { is_mask_ = is_mask; }

  void set_nearest_neighbor(bool nearest_neighbor) {
    picture_layer_inputs_.nearest_neighbor = nearest_neighbor;
  }

  void ValidateSerialization(
      ImageSerializationProcessor* image_serialization_processor,
      LayerTreeHostInProcess* host) {
    std::vector<uint32_t> engine_picture_ids = GetPictureIds();
    proto::LayerProperties proto;
    LayerSpecificPropertiesToProto(&proto, false);

    FakeEnginePictureCache* engine_picture_cache =
        static_cast<FakeEnginePictureCache*>(host->engine_picture_cache());
    EXPECT_THAT(engine_picture_ids,
                testing::UnorderedElementsAreArray(
                    engine_picture_cache->GetAllUsedPictureIds()));

    scoped_refptr<TestSerializationPictureLayer> layer =
        TestSerializationPictureLayer::Create(recording_source_viewport_);
    host->GetLayerTree()->SetRootLayer(layer);

    layer->FromLayerSpecificPropertiesProto(proto);

    FakeClientPictureCache* client_picture_cache =
        static_cast<FakeClientPictureCache*>(host->client_picture_cache());
    EXPECT_THAT(engine_picture_ids,
                testing::UnorderedElementsAreArray(
                    client_picture_cache->GetAllUsedPictureIds()));

    // Validate that the PictureLayer specific fields are properly set.
    EXPECT_TRUE(recording_source()->EqualsTo(*layer->recording_source()));
    EXPECT_EQ(update_source_frame_number_, layer->update_source_frame_number_);
    EXPECT_EQ(is_mask_, layer->is_mask_);
    EXPECT_EQ(picture_layer_inputs_.nearest_neighbor,
              layer->picture_layer_inputs_.nearest_neighbor);
    EXPECT_EQ(picture_layer_inputs_.recorded_viewport,
              layer->picture_layer_inputs_.recorded_viewport);

    // The DisplayItemLists are equal if they are both null or they are both not
    // null and render to the same thing.
    bool display_lists_equal = !picture_layer_inputs_.display_list &&
                               !layer->picture_layer_inputs_.display_list;
    if (picture_layer_inputs_.display_list &&
        layer->picture_layer_inputs_.display_list) {
      display_lists_equal = AreDisplayListDrawingResultsSame(
          picture_layer_inputs_.recorded_viewport,
          picture_layer_inputs_.display_list.get(),
          layer->picture_layer_inputs_.display_list.get());
    }
    EXPECT_TRUE(display_lists_equal);
  }

  std::vector<uint32_t> GetPictureIds() {
    std::vector<uint32_t> ids;
    const DisplayItemList* display_list =
        picture_layer_inputs_.display_list.get();
    if (!display_list)
      return ids;

    for (auto it = display_list->begin(); it != display_list->end(); ++it) {
      sk_sp<const SkPicture> picture = it->GetPicture();
      if (!picture)
        continue;

      ids.push_back(picture->uniqueID());
    }
    return ids;
  }

 private:
  TestSerializationPictureLayer(ContentLayerClient* client,
                                std::unique_ptr<RecordingSource> source,
                                const gfx::Size& recording_source_viewport)
      : PictureLayer(client, std::move(source)),
        recording_source_viewport_(recording_source_viewport) {}
  ~TestSerializationPictureLayer() override {}

  gfx::Size recording_source_viewport_;

  DISALLOW_COPY_AND_ASSIGN(TestSerializationPictureLayer);
};

namespace {

TEST(PictureLayerTest, TestSetAllPropsSerializationDeserialization) {
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings = LayerTreeSettingsForTesting();
  std::unique_ptr<FakeImageSerializationProcessor>
      fake_image_serialization_processor =
          base::WrapUnique(new FakeImageSerializationProcessor);
  std::unique_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner, settings,
                                CompositorMode::SINGLE_THREADED,
                                fake_image_serialization_processor.get());
  host->InitializePictureCacheForTesting();

  gfx::Size recording_source_viewport(256, 256);
  scoped_refptr<TestSerializationPictureLayer> layer =
      TestSerializationPictureLayer::Create(recording_source_viewport);
  host->GetLayerTree()->SetRootLayer(layer);

  Region region(gfx::Rect(14, 15, 16, 17));
  layer->set_invalidation(region);
  layer->set_is_mask(true);
  layer->set_nearest_neighbor(true);

  layer->SetBounds(recording_source_viewport);
  layer->set_update_source_frame_number(0);
  layer->recording_source()->SetDisplayListUsesCachedPicture(false);
  layer->recording_source()->add_draw_rect(
      gfx::Rect(recording_source_viewport));
  layer->recording_source()->SetGenerateDiscardableImagesMetadata(true);
  layer->recording_source()->Rerecord();
  layer->ValidateSerialization(fake_image_serialization_processor.get(),
                               host.get());
}

TEST(PictureLayerTest, TestSerializationDeserialization) {
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<FakeImageSerializationProcessor>
      fake_image_serialization_processor =
          base::WrapUnique(new FakeImageSerializationProcessor);
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, LayerTreeSettings(),
      CompositorMode::SINGLE_THREADED,
      fake_image_serialization_processor.get());
  host->InitializePictureCacheForTesting();

  gfx::Size recording_source_viewport(256, 256);
  scoped_refptr<TestSerializationPictureLayer> layer =
      TestSerializationPictureLayer::Create(recording_source_viewport);
  host->GetLayerTree()->SetRootLayer(layer);

  layer->SetBounds(recording_source_viewport);
  layer->set_update_source_frame_number(0);
  layer->recording_source()->SetDisplayListUsesCachedPicture(false);
  layer->recording_source()->add_draw_rect(
      gfx::Rect(recording_source_viewport));
  layer->recording_source()->SetGenerateDiscardableImagesMetadata(true);
  layer->recording_source()->Rerecord();
  layer->ValidateSerialization(fake_image_serialization_processor.get(),
                               host.get());
}

TEST(PictureLayerTest, TestEmptySerializationDeserialization) {
  std::unique_ptr<FakeImageSerializationProcessor>
      fake_image_serialization_processor =
          base::WrapUnique(new FakeImageSerializationProcessor);
  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(
      &host_client, &task_graph_runner, LayerTreeSettings(),
      CompositorMode::SINGLE_THREADED,
      fake_image_serialization_processor.get());
  host->InitializePictureCacheForTesting();

  gfx::Size recording_source_viewport(256, 256);
  scoped_refptr<TestSerializationPictureLayer> layer =
      TestSerializationPictureLayer::Create(recording_source_viewport);
  host->GetLayerTree()->SetRootLayer(layer);
  layer->ValidateSerialization(fake_image_serialization_processor.get(),
                               host.get());
}

TEST(PictureLayerTest, NoTilesIfEmptyBounds) {
  ContentLayerClient* client = EmptyContentLayerClient::GetInstance();
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->GetLayerTree()->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();
  layer->Update();

  EXPECT_EQ(0, host->SourceFrameNumber());
  host->CommitComplete();
  EXPECT_EQ(1, host->SourceFrameNumber());

  layer->SetBounds(gfx::Size(0, 0));
  layer->SavePaintProperties();
  // Intentionally skipping Update since it would normally be skipped on
  // a layer with empty bounds.

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  std::unique_ptr<FakeCompositorFrameSink> compositor_frame_sink =
      FakeCompositorFrameSink::CreateSoftware();
  FakeLayerTreeHostImpl host_impl(
      LayerTreeSettings(), &impl_task_runner_provider, &task_graph_runner);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
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
  std::unique_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->GetLayerTree()->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();

  gfx::Rect invalidation_bounds(layer_size);

  // The important two lines are the following:
  layer->SetNeedsDisplayRect(invalidation_bounds);
  layer->Update();

  host->CommitComplete();
  FakeImplTaskRunnerProvider impl_task_runner_provider;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink(
      FakeCompositorFrameSink::Create3d());
  LayerTreeSettings layer_tree_settings = LayerTreeSettingsForTesting();
  layer_tree_settings.image_decode_tasks_enabled = true;
  FakeLayerTreeHostImpl host_impl(
      layer_tree_settings, &impl_task_runner_provider, &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
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
  std::unique_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->GetLayerTree()->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();

  gfx::Rect invalidation_bounds(layer_size);

  // The important line is the following (note that we do not call Update):
  layer->SetNeedsDisplayRect(invalidation_bounds);

  host->CommitComplete();
  FakeImplTaskRunnerProvider impl_task_runner_provider;
  std::unique_ptr<CompositorFrameSink> compositor_frame_sink(
      FakeCompositorFrameSink::Create3d());
  LayerTreeSettings layer_tree_settings = LayerTreeSettingsForTesting();
  layer_tree_settings.image_decode_tasks_enabled = true;
  FakeLayerTreeHostImpl host_impl(
      layer_tree_settings, &impl_task_runner_provider, &task_graph_runner);
  host_impl.SetVisible(true);
  host_impl.InitializeRenderer(compositor_frame_sink.get());
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
  client.add_draw_image(CreateDiscardableImage(layer_size), gfx::Point(),
                        SkPaint());
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->GetLayerTree()->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();
  layer->Update();

  EXPECT_EQ(0, host->SourceFrameNumber());
  host->CommitComplete();
  EXPECT_EQ(1, host->SourceFrameNumber());

  layer->SavePaintProperties();
  layer->Update();

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  std::unique_ptr<CompositorFrameSink> compositor_frame_sink(
      FakeCompositorFrameSink::Create3d());
  LayerTreeSettings layer_tree_settings = LayerTreeSettingsForTesting();
  layer_tree_settings.image_decode_tasks_enabled = true;
  FakeLayerTreeHostImpl host_impl(
      layer_tree_settings, &impl_task_runner_provider, &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(compositor_frame_sink.get()));

  host_impl.CreatePendingTree();
  host_impl.pending_tree()->SetRootLayerForTesting(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));
  host_impl.pending_tree()->BuildLayerListForTesting();

  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer_for_testing());

  layer->PushPropertiesTo(layer_impl);

  host->CommitComplete();
  EXPECT_EQ(2, host->SourceFrameNumber());

  host_impl.ActivateSyncTree();

  // By updating the draw proprties on the active tree, we will set the viewport
  // rect for tile priorities to something non-empty.
  const bool can_use_lcd_text = false;
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  host_impl.active_tree()->UpdateDrawProperties(can_use_lcd_text);

  layer->SetBounds(gfx::Size(11, 11));
  layer->SavePaintProperties();

  host_impl.CreatePendingTree();
  layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer_for_testing());

  // We should now have invalid contents and should therefore clear the
  // recording source.
  layer->PushPropertiesTo(layer_impl);

  host_impl.ActivateSyncTree();

  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer_for_testing()->WillDraw(
      DRAW_MODE_SOFTWARE, nullptr);
  host_impl.active_tree()->root_layer_for_testing()->AppendQuads(
      render_pass.get(), &data);
  host_impl.active_tree()->root_layer_for_testing()->DidDraw(nullptr);
}

TEST(PictureLayerTest, SuitableForGpuRasterization) {
  std::unique_ptr<FakeRecordingSource> recording_source_owned(
      new FakeRecordingSource);
  FakeRecordingSource* recording_source = recording_source_owned.get();

  ContentLayerClient* client = EmptyContentLayerClient::GetInstance();
  scoped_refptr<FakePictureLayer> layer =
      FakePictureLayer::CreateWithRecordingSource(
          client, std::move(recording_source_owned));

  FakeLayerTreeHostClient host_client;
  TestTaskGraphRunner task_graph_runner;
  std::unique_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->GetLayerTree()->SetRootLayer(layer);

  // Update layers to initialize the recording source.
  gfx::Size layer_bounds(200, 200);
  gfx::Rect layer_rect(layer_bounds);
  Region invalidation(layer_rect);

  gfx::Rect new_recorded_viewport = client->PaintableRegion();
  scoped_refptr<DisplayItemList> display_list =
      client->PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  size_t painter_reported_memory_usage =
      client->GetApproximateUnsharedMemoryUsage();
  recording_source->UpdateAndExpandInvalidation(&invalidation, layer_bounds,
                                                new_recorded_viewport);
  recording_source->UpdateDisplayItemList(display_list,
                                          painter_reported_memory_usage);

  // Layer is suitable for gpu rasterization by default.
  EXPECT_TRUE(layer->IsSuitableForGpuRasterization());

  // Veto gpu rasterization.
  layer->set_force_unsuitable_for_gpu_rasterization(true);
  EXPECT_FALSE(layer->IsSuitableForGpuRasterization());
}

// PicturePile uses the source frame number as a unit for measuring invalidation
// frequency. When a pile moves between compositors, the frame number increases
// non-monotonically. This executes that code path under this scenario allowing
// for the code to verify correctness with DCHECKs.
TEST(PictureLayerTest, NonMonotonicSourceFrameNumber) {
  LayerTreeSettings settings = LayerTreeSettingsForTesting();
  settings.single_thread_proxy_scheduler = false;
  settings.use_zero_copy = true;

  StubLayerTreeHostSingleThreadClient single_thread_client;
  FakeLayerTreeHostClient host_client1;
  FakeLayerTreeHostClient host_client2;
  TestTaskGraphRunner task_graph_runner;

  ContentLayerClient* client = EmptyContentLayerClient::GetInstance();
  scoped_refptr<FakePictureLayer> layer = FakePictureLayer::Create(client);

  LayerTreeHostInProcess::InitParams params;
  params.client = &host_client1;
  params.settings = &settings;
  params.task_graph_runner = &task_graph_runner;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> host1 =
      LayerTreeHostInProcess::CreateSingleThreaded(&single_thread_client,
                                                   &params);
  host1->SetVisible(true);
  host_client1.SetLayerTreeHost(host1.get());

  // TODO(sad): InitParams will be movable.
  LayerTreeHostInProcess::InitParams params2;
  params2.client = &host_client1;
  params2.settings = &settings;
  params2.task_graph_runner = &task_graph_runner;
  params2.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params2.client = &host_client2;
  params2.animation_host =
      AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<LayerTreeHost> host2 =
      LayerTreeHostInProcess::CreateSingleThreaded(&single_thread_client,
                                                   &params2);
  host2->SetVisible(true);
  host_client2.SetLayerTreeHost(host2.get());

  // The PictureLayer is put in one LayerTreeHost.
  host1->GetLayerTree()->SetRootLayer(layer);
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
  host1->GetLayerTree()->SetRootLayer(nullptr);
  host2->GetLayerTree()->SetRootLayer(layer);

  // Do a main frame, record the picture layers. The frame number has changed
  // non-monotonically.
  layer->SetNeedsDisplay();
  host2->Composite(base::TimeTicks::Now());
  EXPECT_EQ(3, layer->update_count());
  EXPECT_EQ(1, host2->SourceFrameNumber());
}

}  // namespace
}  // namespace cc
