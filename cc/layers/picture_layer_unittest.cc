// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer.h"

#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/gpu_rasterization_settings.h"
#include "cc/test/hybrid_rasterization_settings.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MockContentLayerClient : public ContentLayerClient {
 public:
  virtual void PaintContents(SkCanvas* canvas,
                             const gfx::Rect& clip,
                             gfx::RectF* opaque) OVERRIDE {}
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}
  virtual bool FillsBoundsCompletely() const OVERRIDE {
    return false;
  };
};

TEST(PictureLayerTest, NoTilesIfEmptyBounds) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();

  OcclusionTracker<Layer> occlusion(gfx::Rect(0, 0, 1000, 1000));
  scoped_ptr<ResourceUpdateQueue> queue(new ResourceUpdateQueue);
  layer->Update(queue.get(), &occlusion);

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
    EXPECT_TRUE(layer_impl->pile()->tiling_rect() == gfx::Rect());
    EXPECT_FALSE(layer_impl->pile()->HasRecordings());
  }
}

TEST(PictureLayerTest, ForcedCpuRaster) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(layer);

  // The default value is false.
  EXPECT_FALSE(layer->ShouldUseGpuRasterization());

  // Gpu rasterization cannot be enabled even with raster hint.
  layer->SetHasGpuRasterizationHint(true);
  EXPECT_FALSE(layer->ShouldUseGpuRasterization());
}

TEST(PictureLayerTest, ForcedGpuRaster) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);

  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(GpuRasterizationSettings());
  host->SetRootLayer(layer);

  // The default value is true.
  EXPECT_TRUE(layer->ShouldUseGpuRasterization());

  // Gpu rasterization cannot be disabled even with raster hint.
  layer->SetHasGpuRasterizationHint(false);
  EXPECT_TRUE(layer->ShouldUseGpuRasterization());

  // Gpu rasterization cannot be disabled even with skia veto.
  PicturePile* pile = layer->GetPicturePileForTesting();
  EXPECT_TRUE(pile->is_suitable_for_gpu_rasterization());
  pile->SetUnsuitableForGpuRasterizationForTesting();
  EXPECT_TRUE(layer->ShouldUseGpuRasterization());
}

TEST(PictureLayerTest, HybridRaster) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);

  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(HybridRasterizationSettings());
  host->SetRootLayer(layer);

  // The default value is false.
  EXPECT_FALSE(layer->ShouldUseGpuRasterization());

  // Gpu rasterization can be enabled first time.
  layer->SetHasGpuRasterizationHint(true);
  EXPECT_TRUE(layer->ShouldUseGpuRasterization());

  // Gpu rasterization can always be disabled.
  layer->SetHasGpuRasterizationHint(false);
  EXPECT_FALSE(layer->ShouldUseGpuRasterization());

  // Gpu rasterization cannot be enabled once disabled.
  layer->SetHasGpuRasterizationHint(true);
  EXPECT_FALSE(layer->ShouldUseGpuRasterization());
}

TEST(PictureLayerTest, VetoGpuRaster) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);

  scoped_ptr<FakeLayerTreeHost> host =
      FakeLayerTreeHost::Create(HybridRasterizationSettings());
  host->SetRootLayer(layer);

  EXPECT_FALSE(layer->ShouldUseGpuRasterization());

  layer->SetHasGpuRasterizationHint(true);
  EXPECT_TRUE(layer->ShouldUseGpuRasterization());

  // Veto gpu rasterization.
  PicturePile* pile = layer->GetPicturePileForTesting();
  EXPECT_TRUE(pile->is_suitable_for_gpu_rasterization());
  pile->SetUnsuitableForGpuRasterizationForTesting();
  EXPECT_FALSE(layer->ShouldUseGpuRasterization());
}

}  // namespace
}  // namespace cc
