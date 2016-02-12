// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include <stddef.h>

#include "base/thread_task_runner_handle.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/empty_content_layer_client.h"
#include "cc/layers/layer_settings.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/proto/layer.pb.h"
#include "cc/test/fake_display_list_recording_source.h"
#include "cc/test/fake_image_serialization_processor.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/layer_tree_settings_for_testing.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TestSerializationPictureLayer : public PictureLayer {
 public:
  static scoped_refptr<TestSerializationPictureLayer> Create(
      const gfx::Size& recording_source_viewport) {
    return make_scoped_refptr(new TestSerializationPictureLayer(
        LayerSettings(), EmptyContentLayerClient::GetInstance(),
        FakeDisplayListRecordingSource::CreateFilledRecordingSource(
            recording_source_viewport),
        recording_source_viewport));
  }

  FakeDisplayListRecordingSource* recording_source() {
    return static_cast<FakeDisplayListRecordingSource*>(
        recording_source_.get());
  }

  void set_invalidation(const Region& invalidation) {
    *invalidation_.region() = invalidation;
  }

  void set_last_updated_visible_layer_rect(const gfx::Rect& rect) {
    last_updated_visible_layer_rect_ = rect;
  }

  void set_update_source_frame_number(int number) {
    update_source_frame_number_ = number;
  }

  void set_is_mask(bool is_mask) { is_mask_ = is_mask; }

  void set_nearest_neighbor(bool nearest_neighbor) {
    nearest_neighbor_ = nearest_neighbor;
  }

  void ValidateSerialization() {
    proto::LayerProperties proto;
    LayerSpecificPropertiesToProto(&proto);

    FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
    TestTaskGraphRunner task_graph_runner;
    LayerTreeSettings settings;
    scoped_ptr<FakeImageSerializationProcessor>
        fake_image_serialization_processor =
            make_scoped_ptr(new FakeImageSerializationProcessor);
    scoped_ptr<FakeLayerTreeHost> host =
        FakeLayerTreeHost::Create(&host_client, &task_graph_runner, settings,
                                  CompositorMode::SINGLE_THREADED,
                                  fake_image_serialization_processor.get());
    scoped_refptr<TestSerializationPictureLayer> layer =
        TestSerializationPictureLayer::Create(recording_source_viewport_);
    host->SetRootLayer(layer);
    layer->FromLayerSpecificPropertiesProto(proto);

    // Validate that the PictureLayer specific fields are properly set.
    EXPECT_TRUE(recording_source()->EqualsTo(*layer->recording_source()));
    EXPECT_EQ(last_updated_visible_layer_rect_,
              layer->last_updated_visible_layer_rect_);
    EXPECT_EQ(update_source_frame_number_, layer->update_source_frame_number_);
    EXPECT_EQ(is_mask_, layer->is_mask_);
    EXPECT_EQ(nearest_neighbor_, layer->nearest_neighbor_);
  }

 private:
  TestSerializationPictureLayer(const LayerSettings& settings,
                                ContentLayerClient* client,
                                scoped_ptr<DisplayListRecordingSource> source,
                                const gfx::Size& recording_source_viewport)
      : PictureLayer(settings, client, std::move(source)),
        recording_source_viewport_(recording_source_viewport) {}
  ~TestSerializationPictureLayer() override {}

  gfx::Size recording_source_viewport_;

  DISALLOW_COPY_AND_ASSIGN(TestSerializationPictureLayer);
};

namespace {

TEST(PictureLayerTest, TestSetAllPropsSerializationDeserialization) {
  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings;
  scoped_ptr<FakeImageSerializationProcessor>
      fake_image_serialization_processor =
          make_scoped_ptr(new FakeImageSerializationProcessor);
  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner, settings,
                                CompositorMode::SINGLE_THREADED,
                                fake_image_serialization_processor.get());

  gfx::Size recording_source_viewport(256, 256);
  scoped_refptr<TestSerializationPictureLayer> layer =
      TestSerializationPictureLayer::Create(recording_source_viewport);
  host->SetRootLayer(layer);

  Region region(gfx::Rect(14, 15, 16, 17));
  layer->set_invalidation(region);
  layer->set_last_updated_visible_layer_rect(gfx::Rect(5, 6, 7, 8));
  layer->set_is_mask(true);
  layer->set_nearest_neighbor(true);

  layer->SetBounds(recording_source_viewport);
  layer->set_update_source_frame_number(0);
  layer->recording_source()->SetDisplayListUsesCachedPicture(false);
  layer->recording_source()->add_draw_rect(
      gfx::Rect(recording_source_viewport));
  layer->recording_source()->SetGenerateDiscardableImagesMetadata(true);
  layer->recording_source()->Rerecord();
  layer->ValidateSerialization();
}

TEST(PictureLayerTest, TestSerializationDeserialization) {
  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  TestTaskGraphRunner task_graph_runner;
  LayerTreeSettings settings;
  settings.verify_property_trees = true;
  settings.use_compositor_animation_timelines = true;
  scoped_ptr<FakeImageSerializationProcessor>
      fake_image_serialization_processor =
          make_scoped_ptr(new FakeImageSerializationProcessor);
  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner, settings,
                                CompositorMode::SINGLE_THREADED,
                                fake_image_serialization_processor.get());
  gfx::Size recording_source_viewport(256, 256);
  scoped_refptr<TestSerializationPictureLayer> layer =
      TestSerializationPictureLayer::Create(recording_source_viewport);
  host->SetRootLayer(layer);

  layer->SetBounds(recording_source_viewport);
  layer->set_update_source_frame_number(0);
  layer->recording_source()->SetDisplayListUsesCachedPicture(false);
  layer->recording_source()->add_draw_rect(
      gfx::Rect(recording_source_viewport));
  layer->recording_source()->SetGenerateDiscardableImagesMetadata(true);
  layer->recording_source()->Rerecord();
  layer->ValidateSerialization();
}

TEST(PictureLayerTest, TestEmptySerializationDeserialization) {
  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);

  gfx::Size recording_source_viewport(256, 256);
  scoped_refptr<TestSerializationPictureLayer> layer =
      TestSerializationPictureLayer::Create(recording_source_viewport);
  host->SetRootLayer(layer);
  layer->ValidateSerialization();
}

TEST(PictureLayerTest, NoTilesIfEmptyBounds) {
  ContentLayerClient* client = EmptyContentLayerClient::GetInstance();
  scoped_refptr<PictureLayer> layer =
      PictureLayer::Create(LayerSettings(), client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();
  layer->Update();

  EXPECT_EQ(0, host->source_frame_number());
  host->CommitComplete();
  EXPECT_EQ(1, host->source_frame_number());

  layer->SetBounds(gfx::Size(0, 0));
  layer->SavePaintProperties();
  // Intentionally skipping Update since it would normally be skipped on
  // a layer with empty bounds.

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  TestSharedBitmapManager shared_bitmap_manager;
  FakeLayerTreeHostImpl host_impl(LayerTreeSettingsForTesting(),
                                  &impl_task_runner_provider,
                                  &shared_bitmap_manager, &task_graph_runner);
  host_impl.CreatePendingTree();
  scoped_ptr<FakePictureLayerImpl> layer_impl =
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1);

  layer->PushPropertiesTo(layer_impl.get());
  EXPECT_FALSE(layer_impl->CanHaveTilings());
  EXPECT_TRUE(layer_impl->bounds() == gfx::Size(0, 0));
  EXPECT_EQ(gfx::Size(), layer_impl->raster_source()->GetSize());
  EXPECT_FALSE(layer_impl->raster_source()->HasRecordings());
}

TEST(PictureLayerTest, ClearVisibleRectWhenNoTiling) {
  gfx::Size layer_size(50, 50);
  FakeContentLayerClient client;
  client.set_bounds(layer_size);
  skia::RefPtr<SkImage> image = CreateDiscardableImage(layer_size);
  client.add_draw_image(image.get(), gfx::Point(), SkPaint());
  scoped_refptr<PictureLayer> layer =
      PictureLayer::Create(LayerSettings(), &client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();
  layer->Update();

  EXPECT_EQ(0, host->source_frame_number());
  host->CommitComplete();
  EXPECT_EQ(1, host->source_frame_number());

  layer->SavePaintProperties();
  layer->Update();

  FakeImplTaskRunnerProvider impl_task_runner_provider;

  TestSharedBitmapManager shared_bitmap_manager;
  scoped_ptr<OutputSurface> output_surface(FakeOutputSurface::Create3d());
  LayerTreeSettings layer_tree_settings = LayerTreeSettingsForTesting();
  layer_tree_settings.image_decode_tasks_enabled = true;
  FakeLayerTreeHostImpl host_impl(layer_tree_settings,
                                  &impl_task_runner_provider,
                                  &shared_bitmap_manager, &task_graph_runner);
  host_impl.SetVisible(true);
  EXPECT_TRUE(host_impl.InitializeRenderer(output_surface.get()));

  host_impl.CreatePendingTree();
  host_impl.pending_tree()->SetRootLayer(
      FakePictureLayerImpl::Create(host_impl.pending_tree(), 1));

  FakePictureLayerImpl* layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer());

  layer->PushPropertiesTo(layer_impl);

  host->CommitComplete();
  EXPECT_EQ(2, host->source_frame_number());

  host_impl.ActivateSyncTree();

  // By updating the draw proprties on the active tree, we will set the viewport
  // rect for tile priorities to something non-empty.
  const bool can_use_lcd_text = false;
  host_impl.active_tree()->property_trees()->needs_rebuild = true;
  host_impl.active_tree()->BuildPropertyTreesForTesting();
  host_impl.active_tree()->UpdateDrawProperties(can_use_lcd_text);

  layer->SetBounds(gfx::Size(11, 11));
  layer->SavePaintProperties();

  host_impl.CreatePendingTree();
  layer_impl = static_cast<FakePictureLayerImpl*>(
      host_impl.pending_tree()->root_layer());

  // We should now have invalid contents and should therefore clear the
  // recording source.
  layer->PushPropertiesTo(layer_impl);

  host_impl.ActivateSyncTree();

  scoped_ptr<RenderPass> render_pass = RenderPass::Create();
  AppendQuadsData data;
  host_impl.active_tree()->root_layer()->WillDraw(DRAW_MODE_SOFTWARE, nullptr);
  host_impl.active_tree()->root_layer()->AppendQuads(render_pass.get(), &data);
  host_impl.active_tree()->root_layer()->DidDraw(nullptr);
}

TEST(PictureLayerTest, SuitableForGpuRasterization) {
  scoped_ptr<FakeDisplayListRecordingSource> recording_source_owned(
      new FakeDisplayListRecordingSource);
  FakeDisplayListRecordingSource* recording_source =
      recording_source_owned.get();

  ContentLayerClient* client = EmptyContentLayerClient::GetInstance();
  scoped_refptr<FakePictureLayer> layer =
      FakePictureLayer::CreateWithRecordingSource(
          LayerSettings(), client, std::move(recording_source_owned));

  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  TestTaskGraphRunner task_graph_runner;
  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, &task_graph_runner);
  host->SetRootLayer(layer);

  // Update layers to initialize the recording source.
  gfx::Size layer_bounds(200, 200);
  gfx::Rect layer_rect(layer_bounds);
  Region invalidation(layer_rect);
  recording_source->UpdateAndExpandInvalidation(
      client, &invalidation, layer_bounds, layer_rect, 1,
      DisplayListRecordingSource::RECORD_NORMALLY);

  // Layer is suitable for gpu rasterization by default.
  EXPECT_TRUE(recording_source->IsSuitableForGpuRasterization());
  EXPECT_TRUE(layer->IsSuitableForGpuRasterization());

  // Veto gpu rasterization.
  recording_source->SetUnsuitableForGpuRasterization();
  EXPECT_FALSE(recording_source->IsSuitableForGpuRasterization());
  EXPECT_FALSE(layer->IsSuitableForGpuRasterization());
}

// PicturePile uses the source frame number as a unit for measuring invalidation
// frequency. When a pile moves between compositors, the frame number increases
// non-monotonically. This executes that code path under this scenario allowing
// for the code to verify correctness with DCHECKs.
TEST(PictureLayerTest, NonMonotonicSourceFrameNumber) {
  LayerTreeSettingsForTesting settings;
  settings.single_thread_proxy_scheduler = false;
  settings.use_zero_copy = true;

  FakeLayerTreeHostClient host_client1(FakeLayerTreeHostClient::DIRECT_3D);
  FakeLayerTreeHostClient host_client2(FakeLayerTreeHostClient::DIRECT_3D);
  TestSharedBitmapManager shared_bitmap_manager;
  TestTaskGraphRunner task_graph_runner;

  ContentLayerClient* client = EmptyContentLayerClient::GetInstance();
  scoped_refptr<FakePictureLayer> layer =
      FakePictureLayer::Create(LayerSettings(), client);

  LayerTreeHost::InitParams params;
  params.client = &host_client1;
  params.shared_bitmap_manager = &shared_bitmap_manager;
  params.settings = &settings;
  params.task_graph_runner = &task_graph_runner;
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  scoped_ptr<LayerTreeHost> host1 =
      LayerTreeHost::CreateSingleThreaded(&host_client1, &params);
  host1->SetVisible(true);
  host_client1.SetLayerTreeHost(host1.get());

  params.client = &host_client2;
  scoped_ptr<LayerTreeHost> host2 =
      LayerTreeHost::CreateSingleThreaded(&host_client2, &params);
  host2->SetVisible(true);
  host_client2.SetLayerTreeHost(host2.get());

  // The PictureLayer is put in one LayerTreeHost.
  host1->SetRootLayer(layer);
  // Do a main frame, record the picture layers.
  EXPECT_EQ(0, layer->update_count());
  layer->SetNeedsDisplay();
  host1->Composite(base::TimeTicks::Now());
  EXPECT_EQ(1, layer->update_count());
  EXPECT_EQ(1, host1->source_frame_number());

  // The source frame number in |host1| is now higher than host2.
  layer->SetNeedsDisplay();
  host1->Composite(base::TimeTicks::Now());
  EXPECT_EQ(2, layer->update_count());
  EXPECT_EQ(2, host1->source_frame_number());

  // Then moved to another LayerTreeHost.
  host1->SetRootLayer(nullptr);
  host2->SetRootLayer(layer);

  // Do a main frame, record the picture layers. The frame number has changed
  // non-monotonically.
  layer->SetNeedsDisplay();
  host2->Composite(base::TimeTicks::Now());
  EXPECT_EQ(3, layer->update_count());
  EXPECT_EQ(1, host2->source_frame_number());
}

}  // namespace
}  // namespace cc
