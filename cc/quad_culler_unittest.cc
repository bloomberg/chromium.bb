// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quad_culler.h"

#include "cc/append_quads_data.h"
#include "cc/layer_tiling_data.h"
#include "cc/math_util.h"
#include "cc/occlusion_tracker.h"
#include "cc/overdraw_metrics.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/tile_draw_quad.h"
#include "cc/tiled_layer_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestOcclusionTrackerImpl : public OcclusionTrackerImpl {
public:
    TestOcclusionTrackerImpl(const gfx::Rect& scissorRectInScreen, bool recordMetricsForFrame = true)
        : OcclusionTrackerImpl(scissorRectInScreen, recordMetricsForFrame)
        , m_scissorRectInScreen(scissorRectInScreen)
    {
    }

protected:
    virtual gfx::Rect layerScissorRectInTargetSurface(const LayerImpl* layer) const { return m_scissorRectInScreen; }

private:
    gfx::Rect m_scissorRectInScreen;
};

typedef LayerIterator<LayerImpl, std::vector<LayerImpl*>, RenderSurfaceImpl, LayerIteratorActions::FrontToBack> LayerIteratorType;

class QuadCullerTest : public testing::Test
{
public:
    QuadCullerTest()
        : m_hostImpl(&m_proxy)
        , m_layerId(1)
    {
    }

    scoped_ptr<TiledLayerImpl> makeLayer(TiledLayerImpl* parent, const gfx::Transform& drawTransform, const gfx::Rect& layerRect, float opacity, bool opaque, const gfx::Rect& layerOpaqueRect, std::vector<LayerImpl*>& surfaceLayerList)
    {
        scoped_ptr<TiledLayerImpl> layer = TiledLayerImpl::create(m_hostImpl.activeTree(), m_layerId++);
        scoped_ptr<LayerTilingData> tiler = LayerTilingData::create(gfx::Size(100, 100), LayerTilingData::NoBorderTexels);
        tiler->setBounds(layerRect.size());
        layer->setTilingData(*tiler);
        layer->setSkipsDraw(false);
        layer->drawProperties().target_space_transform = drawTransform;
        layer->drawProperties().screen_space_transform = drawTransform;
        layer->drawProperties().visible_content_rect = layerRect;
        layer->drawProperties().opacity = opacity;
        layer->setContentsOpaque(opaque);
        layer->setBounds(layerRect.size());
        layer->setContentBounds(layerRect.size());

        ResourceProvider::ResourceId resourceId = 1;
        for (int i = 0; i < tiler->numTilesX(); ++i)
            for (int j = 0; j < tiler->numTilesY(); ++j) {
              gfx::Rect tileOpaqueRect = opaque ? tiler->tileBounds(i, j) : gfx::IntersectRects(tiler->tileBounds(i, j), layerOpaqueRect);
                layer->pushTileProperties(i, j, resourceId++, tileOpaqueRect, false);
            }

        gfx::Rect rectInTarget = MathUtil::mapClippedRect(layer->drawTransform(), layer->visibleContentRect());
        if (!parent) {
            layer->createRenderSurface();
            layer->renderSurface()->setContentRect(rectInTarget);
            surfaceLayerList.push_back(layer.get());
            layer->renderSurface()->layerList().push_back(layer.get());
        } else {
            layer->drawProperties().render_target = parent->renderTarget();
            parent->renderSurface()->layerList().push_back(layer.get());
            rectInTarget.Union(MathUtil::mapClippedRect(parent->drawTransform(), parent->visibleContentRect()));
            parent->renderSurface()->setContentRect(rectInTarget);
        }
        layer->drawProperties().drawable_content_rect = rectInTarget;

        return layer.Pass();
    }

    void appendQuads(QuadList& quadList, SharedQuadStateList& sharedStateList, TiledLayerImpl* layer, LayerIteratorType& it, OcclusionTrackerImpl& occlusionTracker)
    {
        occlusionTracker.enterLayer(it);
        QuadCuller quadCuller(quadList, sharedStateList, layer, occlusionTracker, false, false);
        AppendQuadsData data;
        layer->appendQuads(quadCuller, data);
        occlusionTracker.leaveLayer(it);
        ++it;
    }

protected:
    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;
    int m_layerId;
};

#define DECLARE_AND_INITIALIZE_TEST_QUADS               \
    QuadList quadList;                                  \
    SharedQuadStateList sharedStateList;                \
    std::vector<LayerImpl*> renderSurfaceLayerList;     \
    gfx::Transform childTransform;                      \
    gfx::Size rootSize = gfx::Size(300, 300);           \
    gfx::Rect rootRect = gfx::Rect(rootSize);           \
    gfx::Size childSize = gfx::Size(200, 200);          \
    gfx::Rect childRect = gfx::Rect(childSize);

TEST_F(QuadCullerTest, verifyNoCulling)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, false, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST_F(QuadCullerTest, verifyCullChildLinesUpTopLeft)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 40000, 1);
}

TEST_F(QuadCullerTest, verifyCullWhenChildOpacityNotOne)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 0.9f, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST_F(QuadCullerTest, verifyCullWhenChildOpaqueFlagFalse)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, false, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST_F(QuadCullerTest, verifyCullCenterTileOnly)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.Translate(50, 50);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    ASSERT_EQ(quadList.size(), 12u);

    gfx::Rect quadVisibleRect1 = quadList[5]->visible_rect;
    EXPECT_EQ(quadVisibleRect1.height(), 50);

    gfx::Rect quadVisibleRect3 = quadList[7]->visible_rect;
    EXPECT_EQ(quadVisibleRect3.width(), 50);

    // Next index is 8, not 9, since centre quad culled.
    gfx::Rect quadVisibleRect4 = quadList[8]->visible_rect;
    EXPECT_EQ(quadVisibleRect4.width(), 50);
    EXPECT_EQ(quadVisibleRect4.x(), 250);

    gfx::Rect quadVisibleRect6 = quadList[10]->visible_rect;
    EXPECT_EQ(quadVisibleRect6.height(), 50);
    EXPECT_EQ(quadVisibleRect6.y(), 250);

    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 100000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 30000, 1);
}

TEST_F(QuadCullerTest, verifyCullCenterTileNonIntegralSize1)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.Translate(100, 100);

    // Make the root layer's quad have extent (99.1, 99.1) -> (200.9, 200.9) to make
    // sure it doesn't get culled due to transform rounding.
    gfx::Transform rootTransform;
    rootTransform.Translate(99.1, 99.1);
    rootTransform.Scale(1.018, 1.018);

    rootRect = childRect = gfx::Rect(0, 0, 100, 100);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, rootTransform, rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 2u);

    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 20363, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST_F(QuadCullerTest, verifyCullCenterTileNonIntegralSize2)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Make the child's quad slightly smaller than, and centred over, the root layer tile.
    // Verify the child does not cause the quad below to be culled due to rounding.
    childTransform.Translate(100.1, 100.1);
    childTransform.Scale(0.982, 0.982);

    gfx::Transform rootTransform;
    rootTransform.Translate(100, 100);

    rootRect = childRect = gfx::Rect(0, 0, 100, 100);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, rootTransform, rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 2u);

    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 19643, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST_F(QuadCullerTest, verifyCullChildLinesUpBottomRight)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.Translate(100, 100);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 40000, 1);
}

TEST_F(QuadCullerTest, verifyCullSubRegion)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.Translate(50, 50);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    gfx::Rect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() / 2);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, false, childOpaqueRect, renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 12u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 30000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 10000, 1);
}

TEST_F(QuadCullerTest, verifyCullSubRegion2)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.Translate(50, 10);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    gfx::Rect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() * 3 / 4);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, false, childOpaqueRect, renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 12u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 25000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 15000, 1);
}

TEST_F(QuadCullerTest, verifyCullSubRegionCheckOvercull)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.Translate(50, 49);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    gfx::Rect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() / 2);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, false, childOpaqueRect, renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 30000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 10000, 1);
}

TEST_F(QuadCullerTest, verifyNonAxisAlignedQuadsDontOcclude)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Use a small rotation so as to not disturb the geometry significantly.
    childTransform.Rotate(1);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 130000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

// This test requires some explanation: here we are rotating the quads to be culled.
// The 2x2 tile child layer remains in the top-left corner, unrotated, but the 3x3
// tile parent layer is rotated by 1 degree. Of the four tiles the child would
// normally occlude, three will move (slightly) out from under the child layer, and
// one moves further under the child. Only this last tile should be culled.
TEST_F(QuadCullerTest, verifyNonAxisAlignedQuadsSafelyCulled)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Use a small rotation so as to not disturb the geometry significantly.
    gfx::Transform parentTransform;
    parentTransform.Rotate(1);

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, parentTransform, rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(-100, -100, 1000, 1000));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 12u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 100600, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 29400, 1);
}

TEST_F(QuadCullerTest, verifyCullOutsideScissorOverTile)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(200, 100, 100, 100));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 1u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 10000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 120000, 1);
}

TEST_F(QuadCullerTest, verifyCullOutsideScissorOverCulledTile)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(100, 100, 100, 100));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 1u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 10000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 120000, 1);
}

TEST_F(QuadCullerTest, verifyCullOutsideScissorOverPartialTiles)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(50, 50, 200, 200));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 90000, 1);
}

TEST_F(QuadCullerTest, verifyCullOutsideScissorOverNoTiles)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(500, 500, 100, 100));
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 0u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 130000, 1);
}

TEST_F(QuadCullerTest, verifyWithoutMetrics)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    scoped_ptr<TiledLayerImpl> rootLayer = makeLayer(0, gfx::Transform(), rootRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    scoped_ptr<TiledLayerImpl> childLayer = makeLayer(rootLayer.get(), gfx::Transform(), childRect, 1, true, gfx::Rect(), renderSurfaceLayerList);
    TestOcclusionTrackerImpl occlusionTracker(gfx::Rect(50, 50, 200, 200), false);
    LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

}  // namespace
}  // namespace cc
