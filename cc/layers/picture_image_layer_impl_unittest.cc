// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_image_layer_impl.h"

#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/impl_side_painting_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestablePictureImageLayerImpl : public PictureImageLayerImpl {
 public:
  TestablePictureImageLayerImpl(LayerTreeImpl* tree_impl, int id)
      : PictureImageLayerImpl(tree_impl, id) {
  }
  friend class PictureImageLayerImplTest;
};

class PictureImageLayerImplTest : public testing::Test {
 public:
  PictureImageLayerImplTest()
      : host_impl_(ImplSidePaintingSettings(), &proxy_) {
    tiling_client_.SetTileSize(ImplSidePaintingSettings().default_tile_size);
    host_impl_.CreatePendingTree();
    host_impl_.InitializeRenderer(CreateFakeOutputSurface());
  }

  scoped_ptr<TestablePictureImageLayerImpl> CreateLayer(int id) {
    TestablePictureImageLayerImpl* layer =
        new TestablePictureImageLayerImpl(host_impl_.pending_tree(), id);
    layer->SetBounds(gfx::Size(100, 200));
    layer->tilings_.reset(new PictureLayerTilingSet(&tiling_client_));
    layer->tilings_->SetLayerBounds(layer->bounds());
    layer->pile_ = tiling_client_.pile();
    return make_scoped_ptr(layer);
  }

 private:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  FakePictureLayerTilingClient tiling_client_;
};

TEST_F(PictureImageLayerImplTest, CalculateContentsScale) {
  scoped_ptr<TestablePictureImageLayerImpl> layer(CreateLayer(1));
  layer->SetDrawsContent(true);

  float contents_scale_x;
  float contents_scale_y;
  gfx::Size content_bounds;
  layer->CalculateContentsScale(2.f, false,
                                &contents_scale_x, &contents_scale_y,
                                &content_bounds);
  EXPECT_FLOAT_EQ(1.f, contents_scale_x);
  EXPECT_FLOAT_EQ(1.f, contents_scale_y);
  EXPECT_EQ(layer->bounds(), content_bounds);
}

TEST_F(PictureImageLayerImplTest, AreVisibleResourcesReady) {
  scoped_ptr<TestablePictureImageLayerImpl> layer(CreateLayer(1));
  layer->SetBounds(gfx::Size(100, 200));
  layer->SetDrawsContent(true);

  float contents_scale_x;
  float contents_scale_y;
  gfx::Size content_bounds;
  layer->CalculateContentsScale(2.f, false,
                                &contents_scale_x, &contents_scale_y,
                                &content_bounds);

  EXPECT_TRUE(layer->AreVisibleResourcesReady());
}

}  // namespace
}  // namespace cc
