// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tiled_layer_impl.h"

#include "cc/layers/append_quads_data.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/layer_tiling_data.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TiledLayerImplTest : public testing::Test {
 public:
  TiledLayerImplTest() : host_impl_(&proxy_) {}

  // Create a default tiled layer with textures for all tiles and a default
  // visibility of the entire layer size.
  scoped_ptr<TiledLayerImpl> CreateLayer(
      gfx::Size tile_size,
      gfx::Size layer_size,
      LayerTilingData::BorderTexelOption border_texels) {
    scoped_ptr<TiledLayerImpl> layer =
        TiledLayerImpl::Create(host_impl_.active_tree(), 1);
    scoped_ptr<LayerTilingData> tiler =
        LayerTilingData::Create(tile_size, border_texels);
    tiler->SetBounds(layer_size);
    layer->SetTilingData(*tiler);
    layer->set_skips_draw(false);
    layer->draw_properties().visible_content_rect =
        gfx::Rect(layer_size);
    layer->draw_properties().opacity = 1;
    layer->SetBounds(layer_size);
    layer->SetContentBounds(layer_size);
    layer->CreateRenderSurface();
    layer->draw_properties().render_target = layer.get();

    ResourceProvider::ResourceId resource_id = 1;
    for (int i = 0; i < tiler->num_tiles_x(); ++i) {
      for (int j = 0; j < tiler->num_tiles_y(); ++j) {
        layer->PushTileProperties(
            i, j, resource_id++, gfx::Rect(0, 0, 1, 1), false);
      }
    }

    return layer.Pass();
  }

  void GetQuads(QuadList* quads,
                SharedQuadStateList* shared_states,
                gfx::Size tile_size,
                gfx::Size layer_size,
                LayerTilingData::BorderTexelOption border_texel_option,
                gfx::Rect visible_content_rect) {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, border_texel_option);
    layer->draw_properties().visible_content_rect = visible_content_rect;
    layer->SetBounds(layer_size);

    MockQuadCuller quad_culler(quads, shared_states);
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
  }

 protected:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
};

TEST_F(TiledLayerImplTest, EmptyQuadList) {
  gfx::Size tile_size(90, 90);
  int num_tiles_x = 8;
  int num_tiles_y = 4;
  gfx::Size layer_size(tile_size.width() * num_tiles_x,
                       tile_size.height() * num_tiles_y);

  // Verify default layer does creates quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);
    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    unsigned num_tiles = num_tiles_x * num_tiles_y;
    EXPECT_EQ(quad_culler.quad_list().size(), num_tiles);
  }

  // Layer with empty visible layer rect produces no quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);
    layer->draw_properties().visible_content_rect = gfx::Rect();

    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_EQ(quad_culler.quad_list().size(), 0u);
  }

  // Layer with non-intersecting visible layer rect produces no quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);

    gfx::Rect outside_bounds(-100, -100, 50, 50);
    layer->draw_properties().visible_content_rect = outside_bounds;

    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_EQ(quad_culler.quad_list().size(), 0u);
  }

  // Layer with skips draw produces no quads
  {
    scoped_ptr<TiledLayerImpl> layer =
        CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);
    layer->set_skips_draw(true);

    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_EQ(quad_culler.quad_list().size(), 0u);
  }
}

TEST_F(TiledLayerImplTest, Checkerboarding) {
  gfx::Size tile_size(10, 10);
  int num_tiles_x = 2;
  int num_tiles_y = 2;
  gfx::Size layer_size(tile_size.width() * num_tiles_x,
                       tile_size.height() * num_tiles_y);

  scoped_ptr<TiledLayerImpl> layer =
      CreateLayer(tile_size, layer_size, LayerTilingData::NO_BORDER_TEXELS);

  // No checkerboarding
  {
    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_EQ(quad_culler.quad_list().size(), 4u);
    EXPECT_EQ(0u, data.numMissingTiles);

    for (size_t i = 0; i < quad_culler.quad_list().size(); ++i)
      EXPECT_EQ(quad_culler.quad_list()[i]->material, DrawQuad::TILED_CONTENT);
  }

  for (int i = 0; i < num_tiles_x; ++i)
    for (int j = 0; j < num_tiles_y; ++j)
      layer->PushTileProperties(i, j, 0, gfx::Rect(), false);

  // All checkerboarding
  {
    MockQuadCuller quad_culler;
    AppendQuadsData data;
    layer->AppendQuads(&quad_culler, &data);
    EXPECT_LT(0u, data.numMissingTiles);
    EXPECT_EQ(quad_culler.quad_list().size(), 4u);
    for (size_t i = 0; i < quad_culler.quad_list().size(); ++i)
      EXPECT_NE(quad_culler.quad_list()[i]->material, DrawQuad::TILED_CONTENT);
  }
}

// Test with both border texels and without.
#define WITH_AND_WITHOUT_BORDER_TEST(text_fixture_name)                        \
  TEST_F(TiledLayerImplBorderTest, text_fixture_name##NoBorders) {             \
    text_fixture_name(LayerTilingData::NO_BORDER_TEXELS);                      \
  }                                                                            \
  TEST_F(TiledLayerImplBorderTest, text_fixture_name##HasBorders) {            \
    text_fixture_name(LayerTilingData::HAS_BORDER_TEXELS);                     \
  }

class TiledLayerImplBorderTest : public TiledLayerImplTest {
 public:
  void CoverageVisibleRectOnTileBoundaries(
      LayerTilingData::BorderTexelOption borders) {
    gfx::Size layer_size(1000, 1000);
    QuadList quads;
    SharedQuadStateList shared_states;
    GetQuads(&quads,
             &shared_states,
             gfx::Size(100, 100),
             layer_size,
             borders,
             gfx::Rect(layer_size));
    LayerTestCommon::VerifyQuadsExactlyCoverRect(quads, gfx::Rect(layer_size));
  }

  void CoverageVisibleRectIntersectsTiles(
      LayerTilingData::BorderTexelOption borders) {
    // This rect intersects the middle 3x3 of the 5x5 tiles.
    gfx::Point top_left(65, 73);
    gfx::Point bottom_right(182, 198);
    gfx::Rect visible_content_rect = gfx::BoundingRect(top_left, bottom_right);

    gfx::Size layer_size(250, 250);
    QuadList quads;
    SharedQuadStateList shared_states;
    GetQuads(&quads,
             &shared_states,
             gfx::Size(50, 50),
             gfx::Size(250, 250),
             LayerTilingData::NO_BORDER_TEXELS,
             visible_content_rect);
    LayerTestCommon::VerifyQuadsExactlyCoverRect(quads, visible_content_rect);
  }

  void CoverageVisibleRectIntersectsBounds(
      LayerTilingData::BorderTexelOption borders) {
    gfx::Size layer_size(220, 210);
    gfx::Rect visible_content_rect(layer_size);
    QuadList quads;
    SharedQuadStateList shared_states;
    GetQuads(&quads,
             &shared_states,
             gfx::Size(100, 100),
             layer_size,
             LayerTilingData::NO_BORDER_TEXELS,
             visible_content_rect);
    LayerTestCommon::VerifyQuadsExactlyCoverRect(quads, visible_content_rect);
  }
};
WITH_AND_WITHOUT_BORDER_TEST(CoverageVisibleRectOnTileBoundaries);

WITH_AND_WITHOUT_BORDER_TEST(CoverageVisibleRectIntersectsTiles);

WITH_AND_WITHOUT_BORDER_TEST(CoverageVisibleRectIntersectsBounds);

TEST_F(TiledLayerImplTest, TextureInfoForLayerNoBorders) {
  gfx::Size tile_size(50, 50);
  gfx::Size layer_size(250, 250);
  QuadList quads;
  SharedQuadStateList shared_states;
  GetQuads(&quads,
           &shared_states,
           tile_size,
           layer_size,
           LayerTilingData::NO_BORDER_TEXELS,
           gfx::Rect(layer_size));

  for (size_t i = 0; i < quads.size(); ++i) {
    const TileDrawQuad* quad = TileDrawQuad::MaterialCast(quads[i]);

    EXPECT_NE(0u, quad->resource_id) << LayerTestCommon::quad_string << i;
    EXPECT_EQ(gfx::RectF(gfx::PointF(), tile_size), quad->tex_coord_rect)
        << LayerTestCommon::quad_string << i;
    EXPECT_EQ(tile_size, quad->texture_size) << LayerTestCommon::quad_string
                                             << i;
    EXPECT_EQ(gfx::Rect(0, 0, 1, 1), quad->opaque_rect)
        << LayerTestCommon::quad_string << i;
  }
}

}  // namespace
}  // namespace cc
