// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiled_layer.h"

#include "cc/bitmap_content_layer_updater.h"
#include "cc/layer_painter.h"
#include "cc/overdraw_metrics.h"
#include "cc/prioritized_resource_manager.h"
#include "cc/resource_update_controller.h"
#include "cc/single_thread_proxy.h" // For DebugScopedSetImplThread
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/tiled_layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestOcclusionTracker : public OcclusionTracker {
public:
    TestOcclusionTracker()
        : OcclusionTracker(gfx::Rect(0, 0, 1000, 1000), true)
    {
        m_stack.push_back(StackObject());
    }

    void setRenderTarget(Layer* renderTarget)
    {
        m_stack.back().target = renderTarget;
    }

    void setOcclusion(const Region& occlusion)
    {
        m_stack.back().occlusionFromInsideTarget = occlusion;
    }
};

class TiledLayerTest : public testing::Test {
public:
    TiledLayerTest()
        : m_proxy(NULL)
        , m_outputSurface(createFakeOutputSurface())
        , m_queue(make_scoped_ptr(new ResourceUpdateQueue))
        , m_occlusion(0)
    {
    }

    virtual void SetUp()
    {
        m_layerTreeHost = LayerTreeHost::create(&m_fakeLayerImplTreeHostClient, m_settings, scoped_ptr<Thread>(NULL));
        m_proxy = m_layerTreeHost->proxy();
        m_resourceManager = PrioritizedResourceManager::create(m_proxy);
        m_layerTreeHost->initializeRendererIfNeeded();
        m_layerTreeHost->setRootLayer(Layer::create());

        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(m_proxy);
        m_resourceProvider = ResourceProvider::create(m_outputSurface.get());
        m_hostImpl = make_scoped_ptr(new FakeLayerTreeHostImpl(m_proxy));
    }

    virtual ~TiledLayerTest()
    {
        resourceManagerClearAllMemory(m_resourceManager.get(), m_resourceProvider.get());

        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(m_proxy);
        m_resourceProvider.reset();
        m_hostImpl.reset();
    }

    void resourceManagerClearAllMemory(PrioritizedResourceManager* resourceManager, ResourceProvider* resourceProvider)
    {
        {
            DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(m_proxy);
            resourceManager->clearAllMemory(resourceProvider);
            resourceManager->reduceMemory(resourceProvider);
        }
        resourceManager->unlinkAndClearEvictedBackings();
    }
    void updateTextures()
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(m_proxy);
        DCHECK(m_queue);
        scoped_ptr<ResourceUpdateController> updateController =
            ResourceUpdateController::create(
                NULL,
                m_proxy->implThread(),
                m_queue.Pass(),
                m_resourceProvider.get());
        updateController->finalize();
        m_queue = make_scoped_ptr(new ResourceUpdateQueue);
    }
    void layerPushPropertiesTo(FakeTiledLayer* layer, FakeTiledLayerImpl* layerImpl)
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(m_proxy);
        layer->pushPropertiesTo(layerImpl);
    }
    void layerUpdate(FakeTiledLayer* layer, TestOcclusionTracker* occluded)
    {
        DebugScopedSetMainThread mainThread(m_proxy);
        layer->update(*m_queue.get(), occluded, NULL);
    }

    void calcDrawProps(const scoped_refptr<FakeTiledLayer>& layer1)
    {
        scoped_refptr<FakeTiledLayer> layer2;
        calcDrawProps(layer1, layer2);
    }

    void calcDrawProps(const scoped_refptr<FakeTiledLayer>& layer1,
                       const scoped_refptr<FakeTiledLayer>& layer2)
    {
        if (layer1 && !layer1->parent())
            m_layerTreeHost->rootLayer()->addChild(layer1);
        if (layer2 && !layer1->parent())
            m_layerTreeHost->rootLayer()->addChild(layer2);
        if (m_occlusion)
            m_occlusion->setRenderTarget(m_layerTreeHost->rootLayer());

        std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
        LayerTreeHostCommon::calculateDrawProperties(
            m_layerTreeHost->rootLayer(),
            m_layerTreeHost->deviceViewportSize(),
            m_layerTreeHost->deviceScaleFactor(),
            1, // page_scale_factor
            m_layerTreeHost->rendererCapabilities().maxTextureSize,
            false, // can_use_lcd_text
            renderSurfaceLayerList);
    }

    bool updateAndPush(const scoped_refptr<FakeTiledLayer>& layer1,
                       const scoped_ptr<FakeTiledLayerImpl>& layerImpl1)
    {
        scoped_refptr<FakeTiledLayer> layer2;
        scoped_ptr<FakeTiledLayerImpl> layerImpl2;
        return updateAndPush(layer1, layerImpl1, layer2, layerImpl2);
    }

    bool updateAndPush(const scoped_refptr<FakeTiledLayer>& layer1,
                       const scoped_ptr<FakeTiledLayerImpl>& layerImpl1,
                       const scoped_refptr<FakeTiledLayer>& layer2,
                       const scoped_ptr<FakeTiledLayerImpl>& layerImpl2)
    {
        // Get textures
        m_resourceManager->clearPriorities();
        if (layer1)
            layer1->setTexturePriorities(m_priorityCalculator);
        if (layer2)
            layer2->setTexturePriorities(m_priorityCalculator);
        m_resourceManager->prioritizeTextures();

        // Update content
        if (layer1)
            layer1->update(*m_queue.get(), m_occlusion, NULL);
        if (layer2)
            layer2->update(*m_queue.get(), m_occlusion, NULL);

        bool needsUpdate = false;
        if (layer1)
            needsUpdate |= layer1->needsIdlePaint();
        if (layer2)
            needsUpdate |= layer2->needsIdlePaint();

        // Update textures and push.
        updateTextures();
        if (layer1)
            layerPushPropertiesTo(layer1.get(), layerImpl1.get());
        if (layer2)
            layerPushPropertiesTo(layer2.get(), layerImpl2.get());

        return needsUpdate;
    }

public:
    Proxy* m_proxy;
    LayerTreeSettings m_settings;
    scoped_ptr<OutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> m_resourceProvider;
    scoped_ptr<ResourceUpdateQueue> m_queue;
    PriorityCalculator m_priorityCalculator;
    FakeLayerImplTreeHostClient m_fakeLayerImplTreeHostClient;
    scoped_ptr<LayerTreeHost> m_layerTreeHost;
    scoped_ptr<FakeLayerTreeHostImpl> m_hostImpl;
    scoped_ptr<PrioritizedResourceManager> m_resourceManager;
    TestOcclusionTracker* m_occlusion;
};

TEST_F(TiledLayerTest, pushDirtyTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(gfx::Size(100, 200));
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer->invalidateContentRect(gfx::Rect(0, 0, 100, 200));
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 1));

    // Invalidates both tiles, but then only update one of them.
    layer->setBounds(gfx::Size(100, 200));
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    layer->invalidateContentRect(gfx::Rect(0, 0, 100, 200));
    updateAndPush(layer, layerImpl);

    // We should only have the first tile since the other tile was invalidated but not painted.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushOccludedDirtyTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(gfx::Size(100, 200));
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer->drawProperties().drawable_content_rect = gfx::Rect(0, 0, 100, 200);
    layer->invalidateContentRect(gfx::Rect(0, 0, 100, 200));
    updateAndPush(layer, layerImpl);

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 1));

    // Invalidates part of the top tile...
    layer->invalidateContentRect(gfx::Rect(0, 0, 50, 50));
    // ....but the area is occluded.
    occluded.setOcclusion(gfx::Rect(0, 0, 50, 50));
    updateAndPush(layer, layerImpl);

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 2500, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // We should still have both tiles, as part of the top tile is still unoccluded.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushDeletedTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(gfx::Size(100, 200));
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer->invalidateContentRect(gfx::Rect(0, 0, 100, 200));
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 1));

    m_resourceManager->clearPriorities();
    resourceManagerClearAllMemory(m_resourceManager.get(), m_resourceProvider.get());
    m_resourceManager->setMaxMemoryLimitBytes(4*1024*1024);

    // This should drop the tiles on the impl thread.
    layerPushPropertiesTo(layer.get(), layerImpl.get());

    // We should now have no textures on the impl thread.
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 1));

    // This should recreate and update one of the deleted textures.
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    updateAndPush(layer, layerImpl);

    // We should have one tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushIdlePaintTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // The tile size is 100x100. Setup 5x5 tiles with one visible tile in the center.
    // This paints 1 visible of the 25 invalid tiles.
    layer->setBounds(gfx::Size(500, 500));
    layer->drawProperties().visible_content_rect = gfx::Rect(200, 200, 100, 100);
    layer->invalidateContentRect(gfx::Rect(0, 0, 500, 500));
    bool needsUpdate = updateAndPush(layer, layerImpl);
    // We should need idle-painting for surrounding tiles.
    EXPECT_TRUE(needsUpdate);

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(2, 2));

    // For the next four updates, we should detect we still need idle painting.
    for (int i = 0; i < 4; i++) {
        needsUpdate = updateAndPush(layer, layerImpl);
        EXPECT_TRUE(needsUpdate);
    }

    // We should always finish painting eventually.
    for (int i = 0; i < 20; i++)
        needsUpdate = updateAndPush(layer, layerImpl);

    // We should have pre-painted all of the surrounding tiles.
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++)
            EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(i, j));
    }

    EXPECT_FALSE(needsUpdate);
}

TEST_F(TiledLayerTest, predictivePainting)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // Prepainting should occur in the scroll direction first, and the
    // visible rect should be extruded only along the dominant axis.
    gfx::Vector2d directions[6] = { gfx::Vector2d(-10, 0),
                                    gfx::Vector2d(10, 0),
                                    gfx::Vector2d(0, -10),
                                    gfx::Vector2d(0, 10),
                                    gfx::Vector2d(10, 20),
                              gfx::Vector2d(-20, 10) };
    // We should push all tiles that touch the extruded visible rect.
    gfx::Rect pushedVisibleTiles[6] = { gfx::Rect(2, 2, 2, 1),
                                        gfx::Rect(1, 2, 2, 1),
                                        gfx::Rect(2, 2, 1, 2),
                                        gfx::Rect(2, 1, 1, 2),
                                        gfx::Rect(2, 1, 1, 2),
                                        gfx::Rect(2, 2, 2, 1) };
    // The first pre-paint should also paint first in the scroll
    // direction so we should find one additional tile in the scroll direction.
    gfx::Rect pushedPrepaintTiles[6] = { gfx::Rect(2, 2, 3, 1),
                                         gfx::Rect(0, 2, 3, 1),
                                         gfx::Rect(2, 2, 1, 3),
                                         gfx::Rect(2, 0, 1, 3),
                                         gfx::Rect(2, 0, 1, 3),
                                         gfx::Rect(2, 2, 3, 1) };
    for(int k = 0; k < 6; k++) {
        // The tile size is 100x100. Setup 5x5 tiles with one visible tile
        // in the center.
        gfx::Size contentBounds = gfx::Size(500, 500);
        gfx::Rect contentRect = gfx::Rect(0, 0, 500, 500);
        gfx::Rect visibleRect = gfx::Rect(200, 200, 100, 100);
        gfx::Rect previousVisibleRect = gfx::Rect(visibleRect.origin() + directions[k], visibleRect.size());
        gfx::Rect nextVisibleRect = gfx::Rect(visibleRect.origin() - directions[k], visibleRect.size());

        // Setup. Use the previousVisibleRect to setup the prediction for next frame.
        layer->setBounds(contentBounds);
        layer->drawProperties().visible_content_rect = previousVisibleRect;
        layer->invalidateContentRect(contentRect);
        bool needsUpdate = updateAndPush(layer, layerImpl);

        // Invalidate and move the visibleRect in the scroll direction.
        // Check that the correct tiles have been painted in the visible pass.
        layer->invalidateContentRect(contentRect);
        layer->drawProperties().visible_content_rect = visibleRect;
        needsUpdate = updateAndPush(layer, layerImpl);
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++)
                EXPECT_EQ(layerImpl->hasResourceIdForTileAt(i, j), pushedVisibleTiles[k].Contains(i, j));
        }

        // Move the transform in the same direction without invalidating.
        // Check that non-visible pre-painting occured in the correct direction.
        // Ignore diagonal scrolls here (k > 3) as these have new visible content now.
        if (k <= 3) {
            layer->drawProperties().visible_content_rect = nextVisibleRect;
            needsUpdate = updateAndPush(layer, layerImpl);
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 5; j++)
                    EXPECT_EQ(layerImpl->hasResourceIdForTileAt(i, j), pushedPrepaintTiles[k].Contains(i, j));
            }
        }

        // We should always finish painting eventually.
        for (int i = 0; i < 20; i++)
            needsUpdate = updateAndPush(layer, layerImpl);
        EXPECT_FALSE(needsUpdate);
    }
}

TEST_F(TiledLayerTest, pushTilesAfterIdlePaintFailed)
{
    // Start with 2mb of memory, but the test is going to try to use just more than 1mb, so we reduce to 1mb later.
    m_resourceManager->setMaxMemoryLimitBytes(2 * 1024 * 1024);
    scoped_refptr<FakeTiledLayer> layer1 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl1 = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
    scoped_refptr<FakeTiledLayer> layer2 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl2 = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 2));

    // For this test we have two layers. layer1 exhausts most texture memory, leaving room for 2 more tiles from
    // layer2, but not all three tiles. First we paint layer1, and one tile from layer2. Then when we idle paint
    // layer2, we will fail on the third tile of layer2, and this should not leave the second tile in a bad state.

    // This uses 960000 bytes, leaving 88576 bytes of memory left, which is enough for 2 tiles only in the other layer.
    gfx::Rect layer1Rect(0, 0, 100, 2400);
    
    // This requires 4*30000 bytes of memory.
    gfx::Rect layer2Rect(0, 0, 100, 300);

    // Paint a single tile in layer2 so that it will idle paint.
    layer1->setBounds(layer1Rect.size());
    layer1->drawProperties().visible_content_rect = layer1Rect;
    layer2->setBounds(layer2Rect.size());
    layer2->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    bool needsUpdate = updateAndPush(layer1, layerImpl1,
                                     layer2, layerImpl2);
    // We should need idle-painting for both remaining tiles in layer2.
    EXPECT_TRUE(needsUpdate);

    // Reduce our memory limits to 1mb.
    m_resourceManager->setMaxMemoryLimitBytes(1024 * 1024);

    // Now idle paint layer2. We are going to run out of memory though!
    // Oh well, commit the frame and push.
    for (int i = 0; i < 4; i++) {
        needsUpdate = updateAndPush(layer1, layerImpl1,
                                    layer2, layerImpl2);
    }

    // Sanity check, we should have textures for the big layer.
    EXPECT_TRUE(layerImpl1->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl1->hasResourceIdForTileAt(0, 23));

    // We should only have the first two tiles from layer2 since
    // it failed to idle update the last tile.
    EXPECT_TRUE(layerImpl2->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->hasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layerImpl2->hasResourceIdForTileAt(0, 1));
    
    EXPECT_FALSE(needsUpdate);
    EXPECT_FALSE(layerImpl2->hasResourceIdForTileAt(0, 2));
}

TEST_F(TiledLayerTest, pushIdlePaintedOccludedTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;
    
    // The tile size is 100x100, so this invalidates one occluded tile, culls it during paint, but prepaints it.
    occluded.setOcclusion(gfx::Rect(0, 0, 100, 100));

    layer->setBounds(gfx::Size(100, 100));
    calcDrawProps(layer);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    updateAndPush(layer, layerImpl);

    // We should have the prepainted tile on the impl side, but culled it during paint.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_EQ(1, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerTest, pushTilesMarkedDirtyDuringPaint)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    // However, during the paint, we invalidate one of the tiles. This should
    // not prevent the tile from being pushed.
    layer->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(0, 50, 100, 50), layer.get());
    layer->setBounds(gfx::Size(100, 200));
    calcDrawProps(layer);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushTilesLayerMarkedDirtyDuringPaintOnNextLayer)
{
    scoped_refptr<FakeTiledLayer> layer1 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_refptr<FakeTiledLayer> layer2 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layer1Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
    scoped_ptr<FakeTiledLayerImpl> layer2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 2));

    // Invalidate a tile on layer1, during update of layer 2.
    layer2->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(0, 50, 100, 50), layer1.get());
    layer1->setBounds(gfx::Size(100, 200));
    layer2->setBounds(gfx::Size(100, 200));
    calcDrawProps(layer1, layer2);
    layer1->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer2->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer1, layer1Impl,
                  layer2, layer2Impl);

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->hasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->hasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushTilesLayerMarkedDirtyDuringPaintOnPreviousLayer)
{
    scoped_refptr<FakeTiledLayer> layer1 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_refptr<FakeTiledLayer> layer2 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layer1Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
    scoped_ptr<FakeTiledLayerImpl> layer2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 2));

    layer1->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(0, 50, 100, 50), layer2.get());
    layer1->setBounds(gfx::Size(100, 200));
    layer2->setBounds(gfx::Size(100, 200));
    calcDrawProps(layer1, layer2);
    layer1->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer2->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer1, layer1Impl,
                  layer2, layer2Impl);

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->hasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->hasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, paintSmallAnimatedLayersImmediately)
{
    // Create a LayerTreeHost that has the right viewportsize,
    // so the layer is considered small enough.
    FakeLayerImplTreeHostClient fakeLayerImplTreeHostClient;
    scoped_ptr<LayerTreeHost> layerTreeHost = LayerTreeHost::create(&fakeLayerImplTreeHostClient, LayerTreeSettings(), scoped_ptr<Thread>(NULL));

    bool runOutOfMemory[2] = {false, true};
    for (int i = 0; i < 2; i++) {
        // Create a layer with 5x5 tiles, with 4x4 size viewport.
        int viewportWidth  = 4 * FakeTiledLayer::tileSize().width();
        int viewportHeight = 4 * FakeTiledLayer::tileSize().width();
        int layerWidth  = 5 * FakeTiledLayer::tileSize().width();
        int layerHeight = 5 * FakeTiledLayer::tileSize().height();
        int memoryForLayer = layerWidth * layerHeight * 4;
        layerTreeHost->setViewportSize(gfx::Size(layerWidth, layerHeight), gfx::Size(layerWidth, layerHeight));

        // Use 10x5 tiles to run out of memory.
        if (runOutOfMemory[i])
            layerWidth *= 2;

        m_resourceManager->setMaxMemoryLimitBytes(memoryForLayer);

        scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

        // Full size layer with half being visible.
        gfx::Size contentBounds(layerWidth, layerHeight);
        gfx::Rect contentRect(gfx::Point(), contentBounds);
        gfx::Rect visibleRect(gfx::Point(), gfx::Size(layerWidth / 2, layerHeight));

        // Pretend the layer is animating.
        layer->drawProperties().target_space_transform_is_animating = true;
        layer->setBounds(contentBounds);
        layer->drawProperties().visible_content_rect = visibleRect;
        layer->invalidateContentRect(contentRect);
        layer->setLayerTreeHost(layerTreeHost.get());

        // The layer should paint it's entire contents on the first paint
        // if it is close to the viewport size and has the available memory.
        layer->setTexturePriorities(m_priorityCalculator);
        m_resourceManager->prioritizeTextures();
        layer->update(*m_queue.get(), 0, NULL);
        updateTextures();
        layerPushPropertiesTo(layer.get(), layerImpl.get());

        // We should have all the tiles for the small animated layer.
        // We should still have the visible tiles when we didn't
        // have enough memory for all the tiles.
        if (!runOutOfMemory[i]) {
            for (int i = 0; i < 5; ++i) {
                for (int j = 0; j < 5; ++j)
                    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(i, j));
            }
        } else {
            for (int i = 0; i < 10; ++i) {
                for (int j = 0; j < 5; ++j)
                    EXPECT_EQ(layerImpl->hasResourceIdForTileAt(i, j), i < 5);
            }
        }
    }
}

TEST_F(TiledLayerTest, idlePaintOutOfMemory)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // We have enough memory for only the visible rect, so we will run out of memory in first idle paint.
    int memoryLimit = 4 * 100 * 100; // 1 tiles, 4 bytes per pixel.
    m_resourceManager->setMaxMemoryLimitBytes(memoryLimit);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    bool needsUpdate = false;
    layer->setBounds(gfx::Size(300, 300));
    calcDrawProps(layer);
    layer->drawProperties().visible_content_rect = gfx::Rect(100, 100, 100, 100);
    for (int i = 0; i < 2; i++)
        needsUpdate = updateAndPush(layer, layerImpl);

    // Idle-painting should see no more priority tiles for painting.
    EXPECT_FALSE(needsUpdate);

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(1, 1));
}

TEST_F(TiledLayerTest, idlePaintZeroSizedLayer)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    bool animating[2] = {false, true};
    for (int i = 0; i < 2; i++) {
        // Pretend the layer is animating.
        layer->drawProperties().target_space_transform_is_animating = animating[i];

        // The layer's bounds are empty.
        // Empty layers don't paint or idle-paint.
        layer->setBounds(gfx::Size());
        calcDrawProps(layer);
        layer->drawProperties().visible_content_rect = gfx::Rect();
        bool needsUpdate = updateAndPush(layer, layerImpl);
        
        // Empty layers don't have tiles.
        EXPECT_EQ(0u, layer->numPaintedTiles());

        // Empty layers don't need prepaint.
        EXPECT_FALSE(needsUpdate);

        // Empty layers don't have tiles.
        EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 0));
    }
}

TEST_F(TiledLayerTest, idlePaintNonVisibleLayers)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // Alternate between not visible and visible.
    gfx::Rect v(0, 0, 100, 100);
    gfx::Rect nv(0, 0, 0, 0);
    gfx::Rect visibleRect[10] = {nv, nv, v, v, nv, nv, v, v, nv, nv};
    bool invalidate[10] =  {true, true, true, true, true, true, true, true, false, false };

    // We should not have any tiles except for when the layer was visible
    // or after the layer was visible and we didn't invalidate.
    bool haveTile[10] = { false, false, true, true, false, false, true, true, true, true };
    
    for (int i = 0; i < 10; i++) {
        layer->setBounds(gfx::Size(100, 100));
        calcDrawProps(layer);
        layer->drawProperties().visible_content_rect = visibleRect[i];

        if (invalidate[i])
            layer->invalidateContentRect(gfx::Rect(0, 0, 100, 100));
        bool needsUpdate = updateAndPush(layer, layerImpl);
        
        // We should never signal idle paint, as we painted the entire layer
        // or the layer was not visible.
        EXPECT_FALSE(needsUpdate);
        EXPECT_EQ(layerImpl->hasResourceIdForTileAt(0, 0), haveTile[i]);
    }
}

TEST_F(TiledLayerTest, invalidateFromPrepare)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(gfx::Size(100, 200));
    calcDrawProps(layer);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 1));

    layer->fakeLayerUpdater()->clearPrepareCount();
    // Invoke update again. As the layer is valid update shouldn't be invoked on
    // the LayerUpdater.
    updateAndPush(layer, layerImpl);
    EXPECT_EQ(0, layer->fakeLayerUpdater()->prepareCount());

    // setRectToInvalidate triggers invalidateContentRect() being invoked from update.
    layer->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(25, 25, 50, 50), layer.get());
    layer->fakeLayerUpdater()->clearPrepareCount();
    layer->invalidateContentRect(gfx::Rect(0, 0, 50, 50));
    updateAndPush(layer, layerImpl);
    EXPECT_EQ(1, layer->fakeLayerUpdater()->prepareCount());
    layer->fakeLayerUpdater()->clearPrepareCount();

    // The layer should still be invalid as update invoked invalidate.
    updateAndPush(layer, layerImpl); // visible
    EXPECT_EQ(1, layer->fakeLayerUpdater()->prepareCount());
}

TEST_F(TiledLayerTest, verifyUpdateRectWhenContentBoundsAreScaled)
{
    // The updateRect (that indicates what was actually painted) should be in
    // layer space, not the content space.
    scoped_refptr<FakeTiledLayerWithScaledBounds> layer = make_scoped_refptr(new FakeTiledLayerWithScaledBounds(m_resourceManager.get()));

    gfx::Rect layerBounds(0, 0, 300, 200);
    gfx::Rect contentBounds(0, 0, 200, 250);

    layer->setBounds(layerBounds.size());
    layer->setContentBounds(contentBounds.size());
    layer->drawProperties().visible_content_rect = contentBounds;

    // On first update, the updateRect includes all tiles, even beyond the boundaries of the layer.
    // However, it should still be in layer space, not content space.
    layer->invalidateContentRect(contentBounds);

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), 0, NULL);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 300, 300 * 0.8), layer->updateRect());
    updateTextures();

    // After the tiles are updated once, another invalidate only needs to update the bounds of the layer.
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->invalidateContentRect(contentBounds);
    layer->update(*m_queue.get(), 0, NULL);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(layerBounds), layer->updateRect());
    updateTextures();

    // Partial re-paint should also be represented by the updateRect in layer space, not content space.
    gfx::Rect partialDamage(30, 100, 10, 10);
    layer->invalidateContentRect(partialDamage);
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), 0, NULL);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(45, 80, 15, 8), layer->updateRect());
}

TEST_F(TiledLayerTest, verifyInvalidationWhenContentsScaleChanges)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));

    // Create a layer with one tile.
    layer->setBounds(gfx::Size(100, 100));
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 100);

    // Invalidate the entire layer.
    layer->setNeedsDisplay();
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Push the tiles to the impl side and check that there is exactly one.
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), 0, NULL);
    updateTextures();
    layerPushPropertiesTo(layer.get(), layerImpl.get());
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 1));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(1, 0));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(1, 1));

    layer->setNeedsDisplayRect(gfx::Rect());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(), layer->lastNeedsDisplayRect());

    // Change the contents scale.
    layer->updateContentsScale(2);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 200, 200);

    // The impl side should get 2x2 tiles now.
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), 0, NULL);
    updateTextures();
    layerPushPropertiesTo(layer.get(), layerImpl.get());
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(1, 0));
    EXPECT_TRUE(layerImpl->hasResourceIdForTileAt(1, 1));

    // Verify that changing the contents scale caused invalidation, and
    // that the layer-space rectangle requiring painting is not scaled.
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Invalidate the entire layer again, but do not paint. All tiles should be gone now from the
    // impl side.
    layer->setNeedsDisplay();
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    layerPushPropertiesTo(layer.get(), layerImpl.get());
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(0, 1));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(1, 0));
    EXPECT_FALSE(layerImpl->hasResourceIdForTileAt(1, 1));
}

TEST_F(TiledLayerTest, skipsDrawGetsReset)
{
    // Create two 300 x 300 tiled layers.
    gfx::Size contentBounds(300, 300);
    gfx::Rect contentRect(gfx::Point(), contentBounds);

    // We have enough memory for only one of the two layers.
    int memoryLimit = 4 * 300 * 300; // 4 bytes per pixel.

    scoped_refptr<FakeTiledLayer> rootLayer = make_scoped_refptr(new FakeTiledLayer(m_layerTreeHost->contentsTextureManager()));
    scoped_refptr<FakeTiledLayer> childLayer = make_scoped_refptr(new FakeTiledLayer(m_layerTreeHost->contentsTextureManager()));
    rootLayer->addChild(childLayer);

    rootLayer->setBounds(contentBounds);
    rootLayer->drawProperties().visible_content_rect = contentRect;
    rootLayer->setPosition(gfx::PointF(0, 0));
    childLayer->setBounds(contentBounds);
    childLayer->drawProperties().visible_content_rect = contentRect;
    childLayer->setPosition(gfx::PointF(0, 0));
    rootLayer->invalidateContentRect(contentRect);
    childLayer->invalidateContentRect(contentRect);

    m_layerTreeHost->setRootLayer(rootLayer);
    m_layerTreeHost->setViewportSize(gfx::Size(300, 300), gfx::Size(300, 300));

    m_layerTreeHost->updateLayers(*m_queue.get(), memoryLimit);

    // We'll skip the root layer.
    EXPECT_TRUE(rootLayer->skipsDraw());
    EXPECT_FALSE(childLayer->skipsDraw());

    m_layerTreeHost->commitComplete();

    // Remove the child layer.
    rootLayer->removeAllChildren();

    m_layerTreeHost->updateLayers(*m_queue.get(), memoryLimit);
    EXPECT_FALSE(rootLayer->skipsDraw());

    resourceManagerClearAllMemory(m_layerTreeHost->contentsTextureManager(), m_resourceProvider.get());
    m_layerTreeHost->setRootLayer(0);
}

TEST_F(TiledLayerTest, resizeToSmaller)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));

    layer->setBounds(gfx::Size(700, 700));
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 700, 700);
    layer->invalidateContentRect(gfx::Rect(0, 0, 700, 700));

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), 0, NULL);

    layer->setBounds(gfx::Size(200, 200));
    layer->invalidateContentRect(gfx::Rect(0, 0, 200, 200));
}

TEST_F(TiledLayerTest, hugeLayerUpdateCrash)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));

    int size = 1 << 30;
    layer->setBounds(gfx::Size(size, size));
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 700, 700);
    layer->invalidateContentRect(gfx::Rect(0, 0, size, size));

    // Ensure no crash for bounds where size * size would overflow an int.
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), 0, NULL);
}

class TiledLayerPartialUpdateTest : public TiledLayerTest {
public:
    TiledLayerPartialUpdateTest()
    {
        m_settings.maxPartialTextureUpdates = 4;
    }
};

TEST_F(TiledLayerPartialUpdateTest, partialUpdates)
{
    // Create one 300 x 200 tiled layer with 3 x 2 tiles.
    gfx::Size contentBounds(300, 200);
    gfx::Rect contentRect(gfx::Point(), contentBounds);

    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_layerTreeHost->contentsTextureManager()));
    layer->setBounds(contentBounds);
    layer->setPosition(gfx::PointF(0, 0));
    layer->drawProperties().visible_content_rect = contentRect;
    layer->invalidateContentRect(contentRect);

    m_layerTreeHost->setRootLayer(layer);
    m_layerTreeHost->setViewportSize(gfx::Size(300, 200), gfx::Size(300, 200));

    // Full update of all 6 tiles.
    m_layerTreeHost->updateLayers(
        *m_queue.get(), std::numeric_limits<size_t>::max());
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
        EXPECT_EQ(6, m_queue->fullUploadSize());
        EXPECT_EQ(0, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    m_layerTreeHost->commitComplete();

    // Full update of 3 tiles and partial update of 3 tiles.
    layer->invalidateContentRect(gfx::Rect(0, 0, 300, 150));
    m_layerTreeHost->updateLayers(*m_queue.get(), std::numeric_limits<size_t>::max());
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
        EXPECT_EQ(3, m_queue->fullUploadSize());
        EXPECT_EQ(3, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    m_layerTreeHost->commitComplete();

    // Partial update of 6 tiles.
    layer->invalidateContentRect(gfx::Rect(50, 50, 200, 100));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
        m_layerTreeHost->updateLayers(*m_queue.get(), std::numeric_limits<size_t>::max());
        EXPECT_EQ(2, m_queue->fullUploadSize());
        EXPECT_EQ(4, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    m_layerTreeHost->commitComplete();

    // Checkerboard all tiles.
    layer->invalidateContentRect(gfx::Rect(0, 0, 300, 200));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    m_layerTreeHost->commitComplete();

    // Partial update of 6 checkerboard tiles.
    layer->invalidateContentRect(gfx::Rect(50, 50, 200, 100));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
        m_layerTreeHost->updateLayers(*m_queue.get(), std::numeric_limits<size_t>::max());
        EXPECT_EQ(6, m_queue->fullUploadSize());
        EXPECT_EQ(0, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    m_layerTreeHost->commitComplete();

    // Partial update of 4 tiles.
    layer->invalidateContentRect(gfx::Rect(50, 50, 100, 100));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), 1));
        m_layerTreeHost->updateLayers(*m_queue.get(), std::numeric_limits<size_t>::max());
        EXPECT_EQ(0, m_queue->fullUploadSize());
        EXPECT_EQ(4, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(4, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    m_layerTreeHost->commitComplete();

    resourceManagerClearAllMemory(m_layerTreeHost->contentsTextureManager(), m_resourceProvider.get());
    m_layerTreeHost->setRootLayer(0);
}

TEST_F(TiledLayerTest, tilesPaintedWithoutOcclusion)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(gfx::Size(100, 200));
    layer->drawProperties().drawable_content_rect = gfx::Rect(0, 0, 100, 200);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer->invalidateContentRect(gfx::Rect(0, 0, 100, 200));

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), 0, NULL);
    EXPECT_EQ(2, layer->fakeLayerUpdater()->updateCount());
}

TEST_F(TiledLayerTest, tilesPaintedWithOcclusion)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    m_layerTreeHost->setViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->setBounds(gfx::Size(600, 600));
    calcDrawProps(layer);

    occluded.setOcclusion(gfx::Rect(200, 200, 300, 100));
    layer->drawProperties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->contentBounds());
    layer->drawProperties().visible_content_rect = gfx::Rect(gfx::Point(), layer->contentBounds());
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerUpdater()->clearUpdateCount();
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    occluded.setOcclusion(gfx::Rect(250, 200, 300, 100));
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-2, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000 + 340000, 1);
    EXPECT_EQ(3 + 2, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerUpdater()->clearUpdateCount();
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    occluded.setOcclusion(gfx::Rect(250, 250, 300, 100));
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000 + 340000 + 360000, 1);
    EXPECT_EQ(3 + 2, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerTest, tilesPaintedWithOcclusionAndVisiblityConstraints)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    m_layerTreeHost->setViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->setBounds(gfx::Size(600, 600));
    calcDrawProps(layer);

    // The partially occluded tiles (by the 150 occlusion height) are visible beyond the occlusion, so not culled.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 150));
    layer->drawProperties().drawable_content_rect = gfx::Rect(0, 0, 600, 360);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 600, 360);
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(24-3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 210000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // Now the visible region stops at the edge of the occlusion so the partly visible tiles become fully occluded.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 150));
    layer->drawProperties().drawable_content_rect = gfx::Rect(0, 0, 600, 350);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 600, 350);
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(24-6, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 210000 + 180000, 1);
    EXPECT_EQ(3 + 6, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // Now the visible region is even smaller than the occlusion, it should have the same result.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 150));
    layer->drawProperties().drawable_content_rect = gfx::Rect(0, 0, 600, 340);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 600, 340);
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(24-6, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 210000 + 180000 + 180000, 1);
    EXPECT_EQ(3 + 6 + 6, occluded.overdrawMetrics().tilesCulledForUpload());

}

TEST_F(TiledLayerTest, tilesNotPaintedWithoutInvalidation)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    m_layerTreeHost->setViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->setBounds(gfx::Size(600, 600));
    calcDrawProps(layer);

    occluded.setOcclusion(gfx::Rect(200, 200, 300, 100));
    layer->drawProperties().drawable_content_rect = gfx::Rect(0, 0, 600, 600);
    layer->drawProperties().visible_content_rect = gfx::Rect(0, 0, 600, 600);
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-3, layer->fakeLayerUpdater()->updateCount());
    {
        updateTextures();
    }

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerUpdater()->clearUpdateCount();
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    // Repaint without marking it dirty. The 3 culled tiles will be pre-painted now.
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(6, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerTest, tilesPaintedWithOcclusionAndTransforms)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    // This makes sure the painting works when the occluded region (in screen space)
    // is transformed differently than the layer.
    m_layerTreeHost->setViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->setBounds(gfx::Size(600, 600));
    calcDrawProps(layer);
    gfx::Transform screenTransform;
    screenTransform.Scale(0.5, 0.5);
    layer->drawProperties().screen_space_transform = screenTransform;
    layer->drawProperties().target_space_transform = screenTransform;

    occluded.setOcclusion(gfx::Rect(100, 100, 150, 50));
    layer->drawProperties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->contentBounds());
    layer->drawProperties().visible_content_rect = gfx::Rect(gfx::Point(), layer->contentBounds());
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerTest, tilesPaintedWithOcclusionAndScaling)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    // This makes sure the painting works when the content space is scaled to
    // a different layer space. In this case tiles are scaled to be 200x200
    // pixels, which means none should be occluded.
    m_layerTreeHost->setViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->setBounds(gfx::Size(600, 600));
    layer->setRasterScale(0.5);
    calcDrawProps(layer);
    EXPECT_FLOAT_EQ(layer->contentsScaleX(), layer->contentsScaleY());
    gfx::Transform drawTransform;
    double invScaleFactor = 1 / layer->contentsScaleX();
    drawTransform.Scale(invScaleFactor, invScaleFactor);
    layer->drawProperties().target_space_transform = drawTransform;
    layer->drawProperties().screen_space_transform = drawTransform;

    occluded.setOcclusion(gfx::Rect(200, 200, 300, 100));
    layer->drawProperties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->bounds());
    layer->drawProperties().visible_content_rect = gfx::Rect(gfx::Point(), layer->contentBounds());
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    // The content is half the size of the layer (so the number of tiles is fewer).
    // In this case, the content is 300x300, and since the tile size is 100, the
    // number of tiles 3x3.
    EXPECT_EQ(9, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // This makes sure the painting works when the content space is scaled to
    // a different layer space. In this case the occluded region catches the
    // blown up tiles.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 200));
    layer->drawProperties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->bounds());
    layer->drawProperties().visible_content_rect = gfx::Rect(gfx::Point(), layer->contentBounds());
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(9-1, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000 + 80000, 1);
    EXPECT_EQ(1, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // This makes sure content scaling and transforms work together.
    gfx::Transform screenTransform;
    screenTransform.Scale(0.5, 0.5);
    layer->drawProperties().screen_space_transform = screenTransform;
    layer->drawProperties().target_space_transform = screenTransform;

    occluded.setOcclusion(gfx::Rect(100, 100, 150, 100));

    gfx::Rect layerBoundsRect(gfx::Point(), layer->bounds());
    layer->drawProperties().drawable_content_rect = gfx::ToEnclosingRect(gfx::ScaleRect(layerBoundsRect, 0.5));
    layer->drawProperties().visible_content_rect = gfx::Rect(gfx::Point(), layer->contentBounds());
    layer->invalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();
    layer->update(*m_queue.get(), &occluded, NULL);
    EXPECT_EQ(9-1, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000 + 80000 + 80000, 1);
    EXPECT_EQ(1 + 1, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerTest, visibleContentOpaqueRegion)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    gfx::Rect opaquePaintRect;
    Region opaqueContents;

    gfx::Rect contentBounds = gfx::Rect(0, 0, 100, 200);
    gfx::Rect visibleBounds = gfx::Rect(0, 0, 100, 150);

    layer->setBounds(contentBounds.size());
    layer->drawProperties().drawable_content_rect = visibleBounds;
    layer->drawProperties().visible_content_rect = visibleBounds;
    layer->drawProperties().opacity = 1;

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    // If the layer doesn't paint opaque content, then the visibleContentOpaqueRegion should be empty.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->invalidateContentRect(contentBounds);
    layer->update(*m_queue.get(), &occluded, NULL);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.IsEmpty());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // visibleContentOpaqueRegion should match the visible part of what is painted opaque.
    opaquePaintRect = gfx::Rect(10, 10, 90, 190);
    layer->fakeLayerUpdater()->setOpaquePaintRect(opaquePaintRect);
    layer->invalidateContentRect(contentBounds);
    layer->update(*m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(opaquePaintRect, visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // If we paint again without invalidating, the same stuff should be opaque.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->update(*m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(opaquePaintRect, visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // If we repaint a non-opaque part of the tile, then it shouldn't lose its opaque-ness. And other tiles should
    // not be affected.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->invalidateContentRect(gfx::Rect(0, 0, 1, 1));
    layer->update(*m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(opaquePaintRect, visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100 + 1, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // If we repaint an opaque part of the tile, then it should lose its opaque-ness. But other tiles should still
    // not be affected.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->invalidateContentRect(gfx::Rect(10, 10, 1, 1));
    layer->update(*m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(gfx::Rect(10, 100, 90, 100), visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1  + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100 + 1 + 1, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerTest, pixelsPaintedMetrics)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    gfx::Rect opaquePaintRect;
    Region opaqueContents;

    gfx::Rect contentBounds = gfx::Rect(0, 0, 100, 300);
    gfx::Rect visibleBounds = gfx::Rect(0, 0, 100, 300);

    layer->setBounds(contentBounds.size());
    layer->drawProperties().drawable_content_rect = visibleBounds;
    layer->drawProperties().visible_content_rect = visibleBounds;
    layer->drawProperties().opacity = 1;

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    // Invalidates and paints the whole layer.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->invalidateContentRect(contentBounds);
    layer->update(*m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.IsEmpty());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 30000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 30000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // Invalidates an area on the top and bottom tile, which will cause us to paint the tile in the middle,
    // even though it is not dirty and will not be uploaded.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->invalidateContentRect(gfx::Rect(0, 0, 1, 1));
    layer->invalidateContentRect(gfx::Rect(50, 200, 10, 10));
    layer->update(*m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.IsEmpty());

    // The middle tile was painted even though not invalidated.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 30000 + 60 * 210, 1);
    // The pixels uploaded will not include the non-invalidated tile in the middle.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 30000 + 1 + 100, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerTest, dontAllocateContentsWhenTargetSurfaceCantBeAllocated)
{
    // Tile size is 100x100.
    gfx::Rect rootRect(0, 0, 300, 200);
    gfx::Rect childRect(0, 0, 300, 100);
    gfx::Rect child2Rect(0, 100, 300, 100);

    scoped_refptr<FakeTiledLayer> root = make_scoped_refptr(new FakeTiledLayer(m_layerTreeHost->contentsTextureManager()));
    scoped_refptr<Layer> surface = Layer::create();
    scoped_refptr<FakeTiledLayer> child = make_scoped_refptr(new FakeTiledLayer(m_layerTreeHost->contentsTextureManager()));
    scoped_refptr<FakeTiledLayer> child2 = make_scoped_refptr(new FakeTiledLayer(m_layerTreeHost->contentsTextureManager()));

    root->setBounds(rootRect.size());
    root->setAnchorPoint(gfx::PointF());
    root->drawProperties().drawable_content_rect = rootRect;
    root->drawProperties().visible_content_rect = rootRect;
    root->addChild(surface);

    surface->setForceRenderSurface(true);
    surface->setAnchorPoint(gfx::PointF());
    surface->setOpacity(0.5);
    surface->addChild(child);
    surface->addChild(child2);

    child->setBounds(childRect.size());
    child->setAnchorPoint(gfx::PointF());
    child->setPosition(childRect.origin());
    child->drawProperties().visible_content_rect = childRect;
    child->drawProperties().drawable_content_rect = rootRect;

    child2->setBounds(child2Rect.size());
    child2->setAnchorPoint(gfx::PointF());
    child2->setPosition(child2Rect.origin());
    child2->drawProperties().visible_content_rect = child2Rect;
    child2->drawProperties().drawable_content_rect = rootRect;

    m_layerTreeHost->setRootLayer(root);
    m_layerTreeHost->setViewportSize(rootRect.size(), rootRect.size());

    // With a huge memory limit, all layers should update and push their textures.
    root->invalidateContentRect(rootRect);
    child->invalidateContentRect(childRect);
    child2->invalidateContentRect(child2Rect);
    m_layerTreeHost->updateLayers(
        *m_queue.get(), std::numeric_limits<size_t>::max());
    {
        updateTextures();
        EXPECT_EQ(6, root->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(3, child->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(3, child2->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());

        root->fakeLayerUpdater()->clearUpdateCount();
        child->fakeLayerUpdater()->clearUpdateCount();
        child2->fakeLayerUpdater()->clearUpdateCount();

        scoped_ptr<FakeTiledLayerImpl> rootImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), root->id()));
        scoped_ptr<FakeTiledLayerImpl> childImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), child->id()));
        scoped_ptr<FakeTiledLayerImpl> child2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), child2->id()));
        layerPushPropertiesTo(root.get(), rootImpl.get());
        layerPushPropertiesTo(child.get(), childImpl.get());
        layerPushPropertiesTo(child2.get(), child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_TRUE(rootImpl->hasResourceIdForTileAt(i, j));
            EXPECT_TRUE(childImpl->hasResourceIdForTileAt(i, 0));
            EXPECT_TRUE(child2Impl->hasResourceIdForTileAt(i, 0));
        }
    }
    m_layerTreeHost->commitComplete();

    // With a memory limit that includes only the root layer (3x2 tiles) and half the surface that
    // the child layers draw into, the child layers will not be allocated. If the surface isn't
    // accounted for, then one of the children would fit within the memory limit.
    root->invalidateContentRect(rootRect);
    child->invalidateContentRect(childRect);
    child2->invalidateContentRect(child2Rect);
    m_layerTreeHost->updateLayers(
        *m_queue.get(), (3 * 2 + 3 * 1) * (100 * 100) * 4);
    {
        updateTextures();
        EXPECT_EQ(6, root->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child2->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());

        root->fakeLayerUpdater()->clearUpdateCount();
        child->fakeLayerUpdater()->clearUpdateCount();
        child2->fakeLayerUpdater()->clearUpdateCount();

        scoped_ptr<FakeTiledLayerImpl> rootImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), root->id()));
        scoped_ptr<FakeTiledLayerImpl> childImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), child->id()));
        scoped_ptr<FakeTiledLayerImpl> child2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), child2->id()));
        layerPushPropertiesTo(root.get(), rootImpl.get());
        layerPushPropertiesTo(child.get(), childImpl.get());
        layerPushPropertiesTo(child2.get(), child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_TRUE(rootImpl->hasResourceIdForTileAt(i, j));
            EXPECT_FALSE(childImpl->hasResourceIdForTileAt(i, 0));
            EXPECT_FALSE(child2Impl->hasResourceIdForTileAt(i, 0));
        }
    }
    m_layerTreeHost->commitComplete();

    // With a memory limit that includes only half the root layer, no contents will be
    // allocated. If render surface memory wasn't accounted for, there is enough space
    // for one of the children layers, but they draw into a surface that can't be
    // allocated.
    root->invalidateContentRect(rootRect);
    child->invalidateContentRect(childRect);
    child2->invalidateContentRect(child2Rect);
    m_layerTreeHost->updateLayers(
        *m_queue.get(), (3 * 1) * (100 * 100) * 4);
    {
        updateTextures();
        EXPECT_EQ(0, root->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child2->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());

        root->fakeLayerUpdater()->clearUpdateCount();
        child->fakeLayerUpdater()->clearUpdateCount();
        child2->fakeLayerUpdater()->clearUpdateCount();

        scoped_ptr<FakeTiledLayerImpl> rootImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), root->id()));
        scoped_ptr<FakeTiledLayerImpl> childImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), child->id()));
        scoped_ptr<FakeTiledLayerImpl> child2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->activeTree(), child2->id()));
        layerPushPropertiesTo(root.get(), rootImpl.get());
        layerPushPropertiesTo(child.get(), childImpl.get());
        layerPushPropertiesTo(child2.get(), child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_FALSE(rootImpl->hasResourceIdForTileAt(i, j));
            EXPECT_FALSE(childImpl->hasResourceIdForTileAt(i, 0));
            EXPECT_FALSE(child2Impl->hasResourceIdForTileAt(i, 0));
        }
    }
    m_layerTreeHost->commitComplete();

    resourceManagerClearAllMemory(m_layerTreeHost->contentsTextureManager(), m_resourceProvider.get());
    m_layerTreeHost->setRootLayer(0);
}

class TrackingLayerPainter : public LayerPainter {
public:
    static scoped_ptr<TrackingLayerPainter> create() { return make_scoped_ptr(new TrackingLayerPainter()); }

    virtual void paint(SkCanvas*, gfx::Rect contentRect, gfx::RectF&) OVERRIDE
    {
        m_paintedRect = contentRect;
    }

    const gfx::Rect& paintedRect() const { return m_paintedRect; }
    void resetPaintedRect() { m_paintedRect = gfx::Rect(); }

private:
    TrackingLayerPainter() { }

    gfx::Rect m_paintedRect;
};

class UpdateTrackingTiledLayer : public FakeTiledLayer {
public:
    explicit UpdateTrackingTiledLayer(PrioritizedResourceManager* manager)
        : FakeTiledLayer(manager)
    {
        scoped_ptr<TrackingLayerPainter> trackingLayerPainter(TrackingLayerPainter::create());
        m_trackingLayerPainter = trackingLayerPainter.get();
        m_layerUpdater = BitmapContentLayerUpdater::create(trackingLayerPainter.PassAs<LayerPainter>());
    }

    TrackingLayerPainter* trackingLayerPainter() const { return m_trackingLayerPainter; }

protected:
    virtual ~UpdateTrackingTiledLayer() { }

    virtual LayerUpdater* updater() const OVERRIDE { return m_layerUpdater.get(); }

private:
    TrackingLayerPainter* m_trackingLayerPainter;
    scoped_refptr<BitmapContentLayerUpdater> m_layerUpdater;
};

TEST_F(TiledLayerTest, nonIntegerContentsScaleIsNotDistortedDuringPaint)
{
    scoped_refptr<UpdateTrackingTiledLayer> layer = make_scoped_refptr(new UpdateTrackingTiledLayer(m_resourceManager.get()));

    gfx::Rect layerRect(0, 0, 30, 31);
    layer->setPosition(layerRect.origin());
    layer->setBounds(layerRect.size());
    layer->updateContentsScale(1.5);

    gfx::Rect contentRect(0, 0, 45, 47);
    EXPECT_EQ(contentRect.size(), layer->contentBounds());
    layer->drawProperties().visible_content_rect = contentRect;
    layer->drawProperties().drawable_content_rect = contentRect;

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    // Update the whole tile.
    layer->update(*m_queue.get(), 0, NULL);
    layer->trackingLayerPainter()->resetPaintedRect();

    EXPECT_RECT_EQ(gfx::Rect(), layer->trackingLayerPainter()->paintedRect());
    updateTextures();

    // Invalidate the entire layer in content space. When painting, the rect given to webkit should match the layer's bounds.
    layer->invalidateContentRect(contentRect);
    layer->update(*m_queue.get(), 0, NULL);

    EXPECT_RECT_EQ(layerRect, layer->trackingLayerPainter()->paintedRect());
}

TEST_F(TiledLayerTest, nonIntegerContentsScaleIsNotDistortedDuringInvalidation)
{
    scoped_refptr<UpdateTrackingTiledLayer> layer = make_scoped_refptr(new UpdateTrackingTiledLayer(m_resourceManager.get()));

    gfx::Rect layerRect(0, 0, 30, 31);
    layer->setPosition(layerRect.origin());
    layer->setBounds(layerRect.size());
    layer->updateContentsScale(1.3f);

    gfx::Rect contentRect(gfx::Point(), layer->contentBounds());
    layer->drawProperties().visible_content_rect = contentRect;
    layer->drawProperties().drawable_content_rect = contentRect;

    layer->setTexturePriorities(m_priorityCalculator);
    m_resourceManager->prioritizeTextures();

    // Update the whole tile.
    layer->update(*m_queue.get(), 0, NULL);
    layer->trackingLayerPainter()->resetPaintedRect();

    EXPECT_RECT_EQ(gfx::Rect(), layer->trackingLayerPainter()->paintedRect());
    updateTextures();

    // Invalidate the entire layer in layer space. When painting, the rect given to webkit should match the layer's bounds.
    layer->setNeedsDisplayRect(layerRect);
    layer->update(*m_queue.get(), 0, NULL);

    EXPECT_RECT_EQ(layerRect, layer->trackingLayerPainter()->paintedRect());
}

}  // namespace
}  // namespace cc
