// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/tiled_layer_impl.h"

#include "cc/append_quads_data.h"
#include "cc/layer_tiling_data.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/tile_draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace LayerTestCommon;

namespace {

// Create a default tiled layer with textures for all tiles and a default
// visibility of the entire layer size.
static scoped_ptr<TiledLayerImpl> createLayer(const IntSize& tileSize, const IntSize& layerSize, LayerTilingData::BorderTexelOption borderTexels)
{
    scoped_ptr<TiledLayerImpl> layer = TiledLayerImpl::create(1);
    scoped_ptr<LayerTilingData> tiler = LayerTilingData::create(tileSize, borderTexels);
    tiler->setBounds(layerSize);
    layer->setTilingData(*tiler);
    layer->setSkipsDraw(false);
    layer->setVisibleContentRect(IntRect(IntPoint(), layerSize));
    layer->setDrawOpacity(1);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    ResourceProvider::ResourceId resourceId = 1;
    for (int i = 0; i < tiler->numTilesX(); ++i)
        for (int j = 0; j < tiler->numTilesY(); ++j)
            layer->pushTileProperties(i, j, resourceId++, IntRect(0, 0, 1, 1), false);

    return layer.Pass();
}

TEST(TiledLayerImplTest, emptyQuadList)
{
    DebugScopedSetImplThread scopedImplThread;

    const IntSize tileSize(90, 90);
    const int numTilesX = 8;
    const int numTilesY = 4;
    const IntSize layerSize(tileSize.width() * numTilesX, tileSize.height() * numTilesY);

    // Verify default layer does creates quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->appendQuads(quadCuller, data);
        const unsigned numTiles = numTilesX * numTilesY;
        EXPECT_EQ(quadCuller.quadList().size(), numTiles);
    }

    // Layer with empty visible layer rect produces no quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);
        layer->setVisibleContentRect(IntRect());

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->appendQuads(quadCuller, data);
        EXPECT_EQ(quadCuller.quadList().size(), 0u);
    }

    // Layer with non-intersecting visible layer rect produces no quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);

        IntRect outsideBounds(IntPoint(-100, -100), IntSize(50, 50));
        layer->setVisibleContentRect(outsideBounds);

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->appendQuads(quadCuller, data);
        EXPECT_EQ(quadCuller.quadList().size(), 0u);
    }

    // Layer with skips draw produces no quads
    {
        scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);
        layer->setSkipsDraw(true);

        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->appendQuads(quadCuller, data);
        EXPECT_EQ(quadCuller.quadList().size(), 0u);
    }
}

TEST(TiledLayerImplTest, checkerboarding)
{
    DebugScopedSetImplThread scopedImplThread;

    const IntSize tileSize(10, 10);
    const int numTilesX = 2;
    const int numTilesY = 2;
    const IntSize layerSize(tileSize.width() * numTilesX, tileSize.height() * numTilesY);

    scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, LayerTilingData::NoBorderTexels);

    // No checkerboarding
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->appendQuads(quadCuller, data);
        EXPECT_EQ(quadCuller.quadList().size(), 4u);
        EXPECT_FALSE(data.hadMissingTiles);

        for (size_t i = 0; i < quadCuller.quadList().size(); ++i)
            EXPECT_EQ(quadCuller.quadList()[i]->material(), DrawQuad::TiledContent);
    }

    for (int i = 0; i < numTilesX; ++i)
        for (int j = 0; j < numTilesY; ++j)
            layer->pushTileProperties(i, j, 0, IntRect(), false);

    // All checkerboarding
    {
        MockQuadCuller quadCuller;
        AppendQuadsData data;
        layer->appendQuads(quadCuller, data);
        EXPECT_TRUE(data.hadMissingTiles);
        EXPECT_EQ(quadCuller.quadList().size(), 4u);
        for (size_t i = 0; i < quadCuller.quadList().size(); ++i)
            EXPECT_NE(quadCuller.quadList()[i]->material(), DrawQuad::TiledContent);
    }
}

static void getQuads(QuadList& quads, SharedQuadStateList& sharedStates, IntSize tileSize, const IntSize& layerSize, LayerTilingData::BorderTexelOption borderTexelOption, const IntRect& visibleContentRect)
{
    scoped_ptr<TiledLayerImpl> layer = createLayer(tileSize, layerSize, borderTexelOption);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);

    MockQuadCuller quadCuller(quads, sharedStates);
    AppendQuadsData data;
    layer->appendQuads(quadCuller, data);
}

// Test with both border texels and without.
#define WITH_AND_WITHOUT_BORDER_TEST(testFixtureName)       \
    TEST(TiledLayerImplTest, testFixtureName##NoBorders)  \
    {                                                       \
        testFixtureName(LayerTilingData::NoBorderTexels); \
    }                                                       \
    TEST(TiledLayerImplTest, testFixtureName##HasBorders) \
    {                                                       \
        testFixtureName(LayerTilingData::HasBorderTexels);\
    }

static void coverageVisibleRectOnTileBoundaries(LayerTilingData::BorderTexelOption borders)
{
    DebugScopedSetImplThread scopedImplThread;

    IntSize layerSize(1000, 1000);
    QuadList quads;
    SharedQuadStateList sharedStates;
    getQuads(quads, sharedStates, IntSize(100, 100), layerSize, borders, IntRect(IntPoint(), layerSize));
    verifyQuadsExactlyCoverRect(quads, IntRect(IntPoint(), layerSize));
}
WITH_AND_WITHOUT_BORDER_TEST(coverageVisibleRectOnTileBoundaries);

static void coverageVisibleRectIntersectsTiles(LayerTilingData::BorderTexelOption borders)
{
    DebugScopedSetImplThread scopedImplThread;

    // This rect intersects the middle 3x3 of the 5x5 tiles.
    IntPoint topLeft(65, 73);
    IntPoint bottomRight(182, 198);
    IntRect visibleContentRect(topLeft, bottomRight - topLeft);

    IntSize layerSize(250, 250);
    QuadList quads;
    SharedQuadStateList sharedStates;
    getQuads(quads, sharedStates, IntSize(50, 50), IntSize(250, 250), LayerTilingData::NoBorderTexels, visibleContentRect);
    verifyQuadsExactlyCoverRect(quads, visibleContentRect);
}
WITH_AND_WITHOUT_BORDER_TEST(coverageVisibleRectIntersectsTiles);

static void coverageVisibleRectIntersectsBounds(LayerTilingData::BorderTexelOption borders)
{
    DebugScopedSetImplThread scopedImplThread;

    IntSize layerSize(220, 210);
    IntRect visibleContentRect(IntPoint(), layerSize);
    QuadList quads;
    SharedQuadStateList sharedStates;
    getQuads(quads, sharedStates, IntSize(100, 100), layerSize, LayerTilingData::NoBorderTexels, visibleContentRect);
    verifyQuadsExactlyCoverRect(quads, visibleContentRect);
}
WITH_AND_WITHOUT_BORDER_TEST(coverageVisibleRectIntersectsBounds);

TEST(TiledLayerImplTest, textureInfoForLayerNoBorders)
{
    DebugScopedSetImplThread scopedImplThread;

    IntSize tileSize(50, 50);
    IntSize layerSize(250, 250);
    QuadList quads;
    SharedQuadStateList sharedStates;
    getQuads(quads, sharedStates, tileSize, layerSize, LayerTilingData::NoBorderTexels, IntRect(IntPoint(), layerSize));

    for (size_t i = 0; i < quads.size(); ++i) {
        ASSERT_EQ(quads[i]->material(), DrawQuad::TiledContent) << quadString << i;
        TileDrawQuad* quad = static_cast<TileDrawQuad*>(quads[i]);

        EXPECT_NE(quad->resourceId(), 0u) << quadString << i;
        EXPECT_EQ(quad->textureOffset(), IntPoint()) << quadString << i;
        EXPECT_EQ(quad->textureSize(), tileSize) << quadString << i;
        EXPECT_EQ(gfx::Rect(0, 0, 1, 1), quad->opaqueRect()) << quadString << i;
    }
}

TEST(TiledLayerImplTest, tileOpaqueRectForLayerNoBorders)
{
    DebugScopedSetImplThread scopedImplThread;

    IntSize tileSize(50, 50);
    IntSize layerSize(250, 250);
    QuadList quads;
    SharedQuadStateList sharedStates;
    getQuads(quads, sharedStates, tileSize, layerSize, LayerTilingData::NoBorderTexels, IntRect(IntPoint(), layerSize));

    for (size_t i = 0; i < quads.size(); ++i) {
        ASSERT_EQ(quads[i]->material(), DrawQuad::TiledContent) << quadString << i;
        TileDrawQuad* quad = static_cast<TileDrawQuad*>(quads[i]);

        EXPECT_EQ(gfx::Rect(0, 0, 1, 1), quad->opaqueRect()) << quadString << i;
    }
}

} // namespace
