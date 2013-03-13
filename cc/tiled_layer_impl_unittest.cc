// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiled_layer_impl.h"

#include "cc/append_quads_data.h"
#include "cc/layer_tiling_data.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/tile_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class TiledLayerImplTest : public testing::Test
{
public:
    TiledLayerImplTest()
        : m_hostImpl(&m_proxy)
    {
    }

    // Create a default tiled layer with textures for all tiles and a default
    // visibility of the entire layer size.
    scoped_ptr<TiledLayerImpl> createLayer(const gfx::Size& tileSize, const gfx::Size& layerSize, LayerTilingData::BorderTexelOption borderTexels)
    {
        scoped_ptr<TiledLayerImpl> layer = TiledLayerImpl::Create(m_hostImpl.active_tree(), 1);
        scoped_ptr<LayerTilingData> tiler = LayerTilingData::create(tileSize, borderTexels);
        tiler->setBounds(layerSize);
        layer->setTilingData(*tiler);
        layer->setSkipsDraw(false);
        layer->draw_properties().visible_content_rect = gfx::Rect(gfx::Point(), layerSize);
        layer->draw_properties().opacity = 1;
        layer->SetBounds(layerSize);
        layer->SetContentBounds(layerSize);
        layer->CreateRenderSurface();
        layer->draw_properties().render_target = layer.get();

        ResourceProvider::ResourceId resourceId = 1;
        for (int i = 0; i < tiler->numTilesX(); ++i)
            for (int j = 0; j < tiler->numTilesY(); ++j)
                layer->pushTileProperties(i, j, resourceId++, gfx::Rect(0, 0, 1, 1), false);

        return layer.Pass();
    }

    void getQuads(QuadList& quads, SharedQuadStateList& sharedStates, gfx::Size tileSize, const gfx::Size& layerSize, LayerTilingData::BorderTexelOption borderTexelOption, const gfx::Rect& visibleContentRect)
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, borderTexelOption);
        layer->draw_properties().visible_content_rect = visibleContentRect;
        layer->SetBounds(layerSize);

        MockQuadCuller quadCuller(quads, sharedStates);
        AppendQuadsData data;
        layer->AppendQuads(&quadCuller, &data);
    }

protected:
    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;
};

TEST_F(TiledLayerImplTest, emptyQuadList)
{
    const gfx::Size tileSize(90, 90);
    const int numTilesX = 8;
    const int numTilesY = 4;
    const gfx::Size layerSize(tileSize.width() * numTilesX, tileSize.height() * numTilesY);

    // Verify default layer does creates quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->AppendQuads(&quadCuller, &data);
        const unsigned numTiles = numTilesX * numTilesY;
        EXPECT_EQ(quadCuller.quadList().size(), numTiles);
    }

    // Layer with empty visible layer rect produces no quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);
        layer->draw_properties().visible_content_rect = gfx::Rect();

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->AppendQuads(&quadCuller, &data);
        EXPECT_EQ(quadCuller.quadList().size(), 0u);
    }

    // Layer with non-intersecting visible layer rect produces no quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);

        gfx::Rect outsideBounds(gfx::Point(-100, -100), gfx::Size(50, 50));
        layer->draw_properties().visible_content_rect = outsideBounds;

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->AppendQuads(&quadCuller, &data);
        EXPECT_EQ(quadCuller.quadList().size(), 0u);
    }

    // Layer with skips draw produces no quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);
        layer->setSkipsDraw(true);

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->AppendQuads(&quadCuller, &data);
        EXPECT_EQ(quadCuller.quadList().size(), 0u);
    }
}

TEST_F(TiledLayerImplTest, checkerboarding)
{
    const gfx::Size tileSize(10, 10);
    const int numTilesX = 2;
    const int numTilesY = 2;
    const gfx::Size layerSize(tileSize.width() * numTilesX, tileSize.height() * numTilesY);

    scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);

    // No checkerboarding
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->AppendQuads(&quadCuller, &data);
        EXPECT_EQ(quadCuller.quadList().size(), 4u);
        EXPECT_EQ(0u, data.numMissingTiles);

        for (size_t i = 0; i < quadCuller.quadList().size(); ++i)
            EXPECT_EQ(quadCuller.quadList()[i]->material, DrawQuad::TILED_CONTENT);
    }

    for (int i = 0; i < numTilesX; ++i)
        for (int j = 0; j < numTilesY; ++j)
            layer->pushTileProperties(i, j, 0, gfx::Rect(), false);

    // All checkerboarding
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->AppendQuads(&quadCuller, &data);
        EXPECT_LT(0u, data.numMissingTiles);
        EXPECT_EQ(quadCuller.quadList().size(), 4u);
        for (size_t i = 0; i < quadCuller.quadList().size(); ++i)
            EXPECT_NE(quadCuller.quadList()[i]->material, DrawQuad::TILED_CONTENT);
    }
}

// Test with both border texels and without.
#define WITH_AND_WITHOUT_BORDER_TEST(testFixtureName)             \
    TEST_F(TiledLayerImplBorderTest, testFixtureName##NoBorders)  \
    {                                                             \
        testFixtureName(LayerTilingData::NoBorderTexels);         \
    }                                                             \
    TEST_F(TiledLayerImplBorderTest, testFixtureName##HasBorders) \
    {                                                             \
        testFixtureName(LayerTilingData::HasBorderTexels);        \
    }

class TiledLayerImplBorderTest : public TiledLayerImplTest
{
public:
    void coverageVisibleRectOnTileBoundaries(LayerTilingData::BorderTexelOption borders)
    {
        gfx::Size layerSize(1000, 1000);
        QuadList quads;
        SharedQuadStateList sharedStates;
        getQuads(quads, sharedStates, gfx::Size(100, 100), layerSize, borders, gfx::Rect(gfx::Point(), layerSize));
        LayerTestCommon::verifyQuadsExactlyCoverRect(quads, gfx::Rect(gfx::Point(), layerSize));
    }

    void coverageVisibleRectIntersectsTiles(LayerTilingData::BorderTexelOption borders)
    {
        // This rect intersects the middle 3x3 of the 5x5 tiles.
        gfx::Point topLeft(65, 73);
        gfx::Point bottomRight(182, 198);
        gfx::Rect visibleContentRect = gfx::BoundingRect(topLeft, bottomRight);

        gfx::Size layerSize(250, 250);
        QuadList quads;
        SharedQuadStateList sharedStates;
        getQuads(quads, sharedStates, gfx::Size(50, 50), gfx::Size(250, 250), LayerTilingData::NoBorderTexels, visibleContentRect);
        LayerTestCommon::verifyQuadsExactlyCoverRect(quads, visibleContentRect);
    }

    void coverageVisibleRectIntersectsBounds(LayerTilingData::BorderTexelOption borders)
    {
        gfx::Size layerSize(220, 210);
        gfx::Rect visibleContentRect(layerSize);
        QuadList quads;
        SharedQuadStateList sharedStates;
        getQuads(quads, sharedStates, gfx::Size(100, 100), layerSize, LayerTilingData::NoBorderTexels, visibleContentRect);
        LayerTestCommon::verifyQuadsExactlyCoverRect(quads, visibleContentRect);
    }
};
WITH_AND_WITHOUT_BORDER_TEST(coverageVisibleRectOnTileBoundaries);

WITH_AND_WITHOUT_BORDER_TEST(coverageVisibleRectIntersectsTiles);

WITH_AND_WITHOUT_BORDER_TEST(coverageVisibleRectIntersectsBounds);

TEST_F(TiledLayerImplTest, textureInfoForLayerNoBorders)
{
    gfx::Size tileSize(50, 50);
    gfx::Size layerSize(250, 250);
    QuadList quads;
    SharedQuadStateList sharedStates;
    getQuads(quads, sharedStates, tileSize, layerSize, LayerTilingData::NoBorderTexels, gfx::Rect(gfx::Point(), layerSize));

    for (size_t i = 0; i < quads.size(); ++i) {
        const TileDrawQuad* quad = TileDrawQuad::MaterialCast(quads[i]);

        EXPECT_NE(0u, quad->resource_id) << LayerTestCommon::quadString << i;
        EXPECT_EQ(gfx::RectF(gfx::PointF(), tileSize), quad->tex_coord_rect) << LayerTestCommon::quadString << i;
        EXPECT_EQ(tileSize, quad->texture_size) << LayerTestCommon::quadString << i;
        EXPECT_EQ(gfx::Rect(0, 0, 1, 1), quad->opaque_rect) << LayerTestCommon::quadString << i;
    }
}

}  // namespace
}  // namespace cc
