// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_image_layer_impl.h"

#include "cc/layers/append_quads_data.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer_tiling_client.h"
#include "cc/test/impl_side_painting_settings.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TestablePictureImageLayerImpl : public PictureImageLayerImpl {
 public:
  TestablePictureImageLayerImpl(LayerTreeImpl* tree_impl, int id)
      : PictureImageLayerImpl(tree_impl, id) {
  }

  PictureLayerTilingSet* tilings() { return tilings_.get(); }

  friend class PictureImageLayerImplTest;
};

class PictureImageLayerImplTest : public testing::Test {
 public:
  PictureImageLayerImplTest()
      : proxy_(base::MessageLoopProxy::current()),
        host_impl_(ImplSidePaintingSettings(),
                   &proxy_,
                   &shared_bitmap_manager_) {
    tiling_client_.SetTileSize(ImplSidePaintingSettings().default_tile_size);
    host_impl_.CreatePendingTree();
    host_impl_.InitializeRenderer(
        FakeOutputSurface::Create3d().PassAs<OutputSurface>());
  }

  scoped_ptr<TestablePictureImageLayerImpl> CreateLayer(int id,
                                                        WhichTree which_tree) {
    LayerTreeImpl* tree = NULL;
    switch (which_tree) {
      case ACTIVE_TREE:
        tree = host_impl_.active_tree();
        break;
      case PENDING_TREE:
        tree = host_impl_.pending_tree();
        break;
      case NUM_TREES:
        NOTREACHED();
        break;
    }
    TestablePictureImageLayerImpl* layer =
        new TestablePictureImageLayerImpl(tree, id);
    layer->SetBounds(gfx::Size(100, 200));
    layer->tilings_.reset(new PictureLayerTilingSet(&tiling_client_,
                                                    layer->bounds()));
    layer->pile_ = tiling_client_.pile();
    return make_scoped_ptr(layer);
  }

  void UpdateDrawProperties() {
    host_impl_.pending_tree()->UpdateDrawProperties();
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  TestSharedBitmapManager shared_bitmap_manager_;
  FakePictureLayerTilingClient tiling_client_;
};

TEST_F(PictureImageLayerImplTest, CalculateContentsScale) {
  scoped_ptr<TestablePictureImageLayerImpl> layer(CreateLayer(1, PENDING_TREE));
  layer->SetDrawsContent(true);

  float contents_scale_x;
  float contents_scale_y;
  gfx::Size content_bounds;
  layer->CalculateContentsScale(2.f, 3.f, 4.f, false,
                                &contents_scale_x, &contents_scale_y,
                                &content_bounds);
  EXPECT_FLOAT_EQ(1.f, contents_scale_x);
  EXPECT_FLOAT_EQ(1.f, contents_scale_y);
  EXPECT_EQ(layer->bounds(), content_bounds);
}

TEST_F(PictureImageLayerImplTest, AreVisibleResourcesReady) {
  scoped_ptr<TestablePictureImageLayerImpl> layer(CreateLayer(1, PENDING_TREE));
  layer->SetBounds(gfx::Size(100, 200));
  layer->SetDrawsContent(true);

  UpdateDrawProperties();

  float contents_scale_x;
  float contents_scale_y;
  gfx::Size content_bounds;
  layer->CalculateContentsScale(2.f, 3.f, 4.f, false,
                                &contents_scale_x, &contents_scale_y,
                                &content_bounds);
  layer->UpdateTilePriorities();

  EXPECT_TRUE(layer->AreVisibleResourcesReady());
}

TEST_F(PictureImageLayerImplTest, IgnoreIdealContentScale) {
  scoped_ptr<TestablePictureImageLayerImpl> pending_layer(
      CreateLayer(1, PENDING_TREE));
  pending_layer->SetDrawsContent(true);

  // Set PictureLayerImpl::ideal_contents_scale_ to 2.f which is not equal
  // to the content scale used by PictureImageLayerImpl.
  const float suggested_ideal_contents_scale = 2.f;
  const float device_scale_factor = 3.f;
  const float page_scale_factor = 4.f;
  const bool animating_transform_to_screen = false;
  float contents_scale_x;
  float contents_scale_y;
  gfx::Size content_bounds;
  pending_layer->CalculateContentsScale(suggested_ideal_contents_scale,
                                        device_scale_factor,
                                        page_scale_factor,
                                        animating_transform_to_screen,
                                        &contents_scale_x,
                                        &contents_scale_y,
                                        &content_bounds);

  // Push to active layer.
  host_impl_.ActivatePendingTree();
  scoped_ptr<TestablePictureImageLayerImpl> active_layer(
      CreateLayer(1, ACTIVE_TREE));
  pending_layer->PushPropertiesTo(active_layer.get());
  active_layer->CalculateContentsScale(suggested_ideal_contents_scale,
                                       device_scale_factor,
                                       page_scale_factor,
                                       animating_transform_to_screen,
                                       &contents_scale_x,
                                       &contents_scale_y,
                                       &content_bounds);

  // Create tile and resource.
  active_layer->tilings()->tiling_at(0)->CreateAllTilesForTesting();
  host_impl_.tile_manager()->InitializeTilesWithResourcesForTesting(
      active_layer->tilings()->tiling_at(0)->AllTilesForTesting());

  // Draw.
  active_layer->draw_properties().visible_content_rect =
      gfx::Rect(active_layer->bounds());
  MockQuadCuller quad_culler;
  AppendQuadsData data;
  active_layer->WillDraw(DRAW_MODE_SOFTWARE, NULL);
  active_layer->AppendQuads(&quad_culler, &data);
  active_layer->DidDraw(NULL);

  EXPECT_EQ(DrawQuad::TILED_CONTENT, quad_culler.quad_list()[0]->material);

  // Tiles are ready at correct scale, so should not set had_incomplete_tile.
  EXPECT_FALSE(data.had_incomplete_tile);
}

}  // namespace
}  // namespace cc
