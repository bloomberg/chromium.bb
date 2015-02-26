// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MockContentLayerClient : public ContentLayerClient {
 public:
  void PaintContents(SkCanvas* canvas,
                     const gfx::Rect& clip,
                     PaintingControlSetting picture_control) override {}
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      const gfx::Rect& clip,
      PaintingControlSetting picture_control) override {
    NOTIMPLEMENTED();
    return DisplayItemList::Create();
  }
  bool FillsBoundsCompletely() const override { return false; };
};

TEST(PictureLayerTest, NoTilesIfEmptyBounds) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(&host_client);
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();

  OcclusionTracker<Layer> occlusion(gfx::Rect(0, 0, 1000, 1000));
  scoped_ptr<ResourceUpdateQueue> queue(new ResourceUpdateQueue);
  layer->Update(queue.get(), &occlusion);

  EXPECT_EQ(0, host->source_frame_number());
  host->CommitComplete();
  EXPECT_EQ(1, host->source_frame_number());

  layer->SetBounds(gfx::Size(0, 0));
  layer->SavePaintProperties();
  // Intentionally skipping Update since it would normally be skipped on
  // a layer with empty bounds.

  FakeProxy proxy;
  {
    DebugScopedSetImplThread impl_thread(&proxy);

    TestSharedBitmapManager shared_bitmap_manager;
    FakeLayerTreeHostImpl host_impl(
        ImplSidePaintingSettings(), &proxy, &shared_bitmap_manager);
    host_impl.CreatePendingTree();
    scoped_ptr<FakePictureLayerImpl> layer_impl =
        FakePictureLayerImpl::Create(host_impl.pending_tree(), 1);

    layer->PushPropertiesTo(layer_impl.get());
    EXPECT_FALSE(layer_impl->CanHaveTilings());
    EXPECT_TRUE(layer_impl->bounds() == gfx::Size(0, 0));
    EXPECT_EQ(gfx::Size(), layer_impl->raster_source()->GetSize());
    EXPECT_FALSE(layer_impl->raster_source()->HasRecordings());
  }
}

TEST(PictureLayerTest, SuitableForGpuRasterization) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create(&host_client);
  host->SetRootLayer(layer);
  RecordingSource* recording_source = layer->GetRecordingSourceForTesting();

  // Layer is suitable for gpu rasterization by default.
  EXPECT_TRUE(recording_source->IsSuitableForGpuRasterization());
  EXPECT_TRUE(layer->IsSuitableForGpuRasterization());

  // Veto gpu rasterization.
  recording_source->SetUnsuitableForGpuRasterizationForTesting();
  EXPECT_FALSE(recording_source->IsSuitableForGpuRasterization());
  EXPECT_FALSE(layer->IsSuitableForGpuRasterization());
}

TEST(PictureLayerTest, UseTileGridSize) {
  LayerTreeSettings settings;
  settings.default_tile_grid_size = gfx::Size(123, 123);

  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  FakeLayerTreeHostClient host_client(FakeLayerTreeHostClient::DIRECT_3D);
  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(&host_client, settings);
  host->SetRootLayer(layer);

  // Tile-grid is set according to its setting.
  gfx::Size size =
      layer->GetRecordingSourceForTesting()->GetTileGridSizeForTesting();
  EXPECT_EQ(size.width(), 123);
  EXPECT_EQ(size.height(), 123);
}

// PicturePile uses the source frame number as a unit for measuring invalidation
// frequency. When a pile moves between compositors, the frame number increases
// non-monotonically. This executes that code path under this scenario allowing
// for the code to verify correctness with DCHECKs.
TEST(PictureLayerTest, NonMonotonicSourceFrameNumber) {
  LayerTreeSettings settings;
  settings.single_thread_proxy_scheduler = false;

  FakeLayerTreeHostClient host_client1(FakeLayerTreeHostClient::DIRECT_3D);
  FakeLayerTreeHostClient host_client2(FakeLayerTreeHostClient::DIRECT_3D);
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager(
      new TestSharedBitmapManager());

  MockContentLayerClient client;
  scoped_refptr<FakePictureLayer> layer = FakePictureLayer::Create(&client);

  scoped_ptr<LayerTreeHost> host1 = LayerTreeHost::CreateSingleThreaded(
      &host_client1, &host_client1, shared_bitmap_manager.get(), nullptr,
      settings, base::MessageLoopProxy::current(), nullptr);
  host_client1.SetLayerTreeHost(host1.get());

  scoped_ptr<LayerTreeHost> host2 = LayerTreeHost::CreateSingleThreaded(
      &host_client2, &host_client2, shared_bitmap_manager.get(), nullptr,
      settings, base::MessageLoopProxy::current(), nullptr);
  host_client2.SetLayerTreeHost(host2.get());

  // The PictureLayer is put in one LayerTreeHost.
  host1->SetRootLayer(layer);
  // Do a main frame, record the picture layers.
  EXPECT_EQ(0u, layer->update_count());
  layer->SetNeedsDisplay();
  host1->Composite(base::TimeTicks::Now());
  EXPECT_EQ(1u, layer->update_count());
  EXPECT_EQ(1, host1->source_frame_number());

  // The source frame number in |host1| is now higher than host2.
  layer->SetNeedsDisplay();
  host1->Composite(base::TimeTicks::Now());
  EXPECT_EQ(2u, layer->update_count());
  EXPECT_EQ(2, host1->source_frame_number());

  // Then moved to another LayerTreeHost.
  host1->SetRootLayer(nullptr);
  host2->SetRootLayer(layer);

  // Do a main frame, record the picture layers. The frame number has changed
  // non-monotonically.
  layer->SetNeedsDisplay();
  host2->Composite(base::TimeTicks::Now());
  EXPECT_EQ(3u, layer->update_count());
  EXPECT_EQ(1, host2->source_frame_number());
}

}  // namespace
}  // namespace cc
