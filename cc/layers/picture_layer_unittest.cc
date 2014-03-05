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
#include "cc/test/impl_side_painting_settings.h"
#include "cc/trees/occlusion_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MockContentLayerClient : public ContentLayerClient {
 public:
  virtual void PaintContents(SkCanvas* canvas,
                             const gfx::Rect& clip,
                             gfx::RectF* opaque) OVERRIDE {}
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}
};

TEST(PictureLayerTest, NoTilesIfEmptyBounds) {
  MockContentLayerClient client;
  scoped_refptr<PictureLayer> layer = PictureLayer::Create(&client);
  layer->SetBounds(gfx::Size(10, 10));

  scoped_ptr<FakeLayerTreeHost> host = FakeLayerTreeHost::Create();
  host->SetRootLayer(layer);
  layer->SetIsDrawable(true);
  layer->SavePaintProperties();

  OcclusionTracker<Layer> occlusion(gfx::Rect(0, 0, 1000, 1000), false);
  scoped_ptr<ResourceUpdateQueue> queue(new ResourceUpdateQueue);
  layer->Update(queue.get(), &occlusion);

  layer->SetBounds(gfx::Size(0, 0));
  layer->SavePaintProperties();
  // Intentionally skipping Update since it would normally be skipped on
  // a layer with empty bounds.

  FakeProxy proxy;
#ifndef NDEBUG
  proxy.SetCurrentThreadIsImplThread(true);
#endif
  {
    FakeLayerTreeHostImpl host_impl(ImplSidePaintingSettings(), &proxy);
    host_impl.CreatePendingTree();
    scoped_ptr<FakePictureLayerImpl> layer_impl =
        FakePictureLayerImpl::Create(host_impl.pending_tree(), 1);

    layer->PushPropertiesTo(layer_impl.get());
    EXPECT_FALSE(layer_impl->CanHaveTilings());
    EXPECT_TRUE(layer_impl->bounds() == gfx::Size(0, 0));
    EXPECT_TRUE(layer_impl->pile()->size() == gfx::Size(0, 0));
    EXPECT_TRUE(layer_impl->pile()->recorded_region().IsEmpty());
  }
#ifndef NDEBUG
  proxy.SetCurrentThreadIsImplThread(false);
#endif
}

}  // namespace
}  // namespace cc
