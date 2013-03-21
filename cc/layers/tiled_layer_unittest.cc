// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tiled_layer.h"

#include "cc/debug/overdraw_metrics.h"
#include "cc/resources/bitmap_content_layer_updater.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource_manager.h"
#include "cc/resources/resource_update_controller.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/tiled_layer_test_common.h"
#include "cc/trees/single_thread_proxy.h" // For DebugScopedSetImplThread
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
        stack_.push_back(StackObject());
    }

    void setRenderTarget(Layer* renderTarget)
    {
        stack_.back().target = renderTarget;
    }

    void setOcclusion(const Region& occlusion)
    {
        stack_.back().occlusion_from_inside_target = occlusion;
    }
};

class TiledLayerTest : public testing::Test {
public:
    TiledLayerTest()
        : proxy_(NULL)
        , m_outputSurface(createFakeOutputSurface())
        , m_queue(make_scoped_ptr(new ResourceUpdateQueue))
        , m_fakeLayerImplTreeHostClient(FakeLayerTreeHostClient::DIRECT_3D)
        , m_occlusion(0)
    {
        m_settings.max_partial_texture_updates = std::numeric_limits<size_t>::max();
    }

    virtual void SetUp()
    {
        layer_tree_host_ = LayerTreeHost::Create(&m_fakeLayerImplTreeHostClient, m_settings, scoped_ptr<Thread>(NULL));
        proxy_ = layer_tree_host_->proxy();
        m_resourceManager = PrioritizedResourceManager::Create(proxy_);
        layer_tree_host_->InitializeRendererIfNeeded();
        layer_tree_host_->SetRootLayer(Layer::Create());

        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(proxy_);
        m_resourceProvider = ResourceProvider::Create(m_outputSurface.get());
        m_hostImpl = make_scoped_ptr(new FakeLayerTreeHostImpl(proxy_));
    }

    virtual ~TiledLayerTest()
    {
        resourceManagerClearAllMemory(m_resourceManager.get(), m_resourceProvider.get());

        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(proxy_);
        m_resourceProvider.reset();
        m_hostImpl.reset();
    }

    void resourceManagerClearAllMemory(PrioritizedResourceManager* resourceManager, ResourceProvider* resourceProvider)
    {
        {
            DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(proxy_);
            resourceManager->ClearAllMemory(resourceProvider);
            resourceManager->ReduceMemory(resourceProvider);
        }
        resourceManager->UnlinkAndClearEvictedBackings();
    }
    void updateTextures()
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(proxy_);
        DCHECK(m_queue);
        scoped_ptr<ResourceUpdateController> updateController =
            ResourceUpdateController::Create(
                NULL,
                proxy_->ImplThread(),
                m_queue.Pass(),
                m_resourceProvider.get());
        updateController->Finalize();
        m_queue = make_scoped_ptr(new ResourceUpdateQueue);
    }
    void layerPushPropertiesTo(FakeTiledLayer* layer, FakeTiledLayerImpl* layerImpl)
    {
        DebugScopedSetImplThreadAndMainThreadBlocked implThreadAndMainThreadBlocked(proxy_);
        layer->PushPropertiesTo(layerImpl);
    }
    void layerUpdate(FakeTiledLayer* layer, TestOcclusionTracker* occluded)
    {
        DebugScopedSetMainThread mainThread(proxy_);
        layer->Update(m_queue.get(), occluded, NULL);
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
            layer_tree_host_->root_layer()->AddChild(layer1);
        if (layer2 && !layer2->parent())
            layer_tree_host_->root_layer()->AddChild(layer2);
        if (m_occlusion)
            m_occlusion->setRenderTarget(layer_tree_host_->root_layer());

        std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
        LayerTreeHostCommon::calculateDrawProperties(
            layer_tree_host_->root_layer(),
            layer_tree_host_->device_viewport_size(),
            layer_tree_host_->device_scale_factor(),
            1, // page_scale_factor
            layer_tree_host_->GetRendererCapabilities().max_texture_size,
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
        m_resourceManager->ClearPriorities();
        if (layer1)
            layer1->SetTexturePriorities(m_priorityCalculator);
        if (layer2)
            layer2->SetTexturePriorities(m_priorityCalculator);
        m_resourceManager->PrioritizeTextures();

        // Update content
        if (layer1)
            layer1->Update(m_queue.get(), m_occlusion, NULL);
        if (layer2)
            layer2->Update(m_queue.get(), m_occlusion, NULL);

        bool needsUpdate = false;
        if (layer1)
            needsUpdate |= layer1->NeedsIdlePaint();
        if (layer2)
            needsUpdate |= layer2->NeedsIdlePaint();

        // Update textures and push.
        updateTextures();
        if (layer1)
            layerPushPropertiesTo(layer1.get(), layerImpl1.get());
        if (layer2)
            layerPushPropertiesTo(layer2.get(), layerImpl2.get());

        return needsUpdate;
    }

public:
    Proxy* proxy_;
    LayerTreeSettings m_settings;
    scoped_ptr<OutputSurface> m_outputSurface;
    scoped_ptr<ResourceProvider> m_resourceProvider;
    scoped_ptr<ResourceUpdateQueue> m_queue;
    PriorityCalculator m_priorityCalculator;
    FakeLayerTreeHostClient m_fakeLayerImplTreeHostClient;
    scoped_ptr<LayerTreeHost> layer_tree_host_;
    scoped_ptr<FakeLayerTreeHostImpl> m_hostImpl;
    scoped_ptr<PrioritizedResourceManager> m_resourceManager;
    TestOcclusionTracker* m_occlusion;
};

TEST_F(TiledLayerTest, pushDirtyTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer);
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 1));

    // Invalidates both tiles, but then only update one of them.
    layer->InvalidateContentRect(gfx::Rect(0, 0, 100, 200));
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    updateAndPush(layer, layerImpl);

    // We should only have the first tile since the other tile was invalidated but not painted.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushOccludedDirtyTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;
    layer_tree_host_->SetViewportSize(gfx::Size(1000, 1000), gfx::Size(1000, 1000));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer);
    updateAndPush(layer, layerImpl);

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 20000, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 1));

    // Invalidates part of the top tile...
    layer->InvalidateContentRect(gfx::Rect(0, 0, 50, 50));
    // ....but the area is occluded.
    occluded.setOcclusion(gfx::Rect(0, 0, 50, 50));
    calcDrawProps(layer);
    updateAndPush(layer, layerImpl);

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 20000 + 2500, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    // We should still have both tiles, as part of the top tile is still unoccluded.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushDeletedTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer);
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 1));

    m_resourceManager->ClearPriorities();
    resourceManagerClearAllMemory(m_resourceManager.get(), m_resourceProvider.get());
    m_resourceManager->SetMaxMemoryLimitBytes(4*1024*1024);

    // This should drop the tiles on the impl thread.
    layerPushPropertiesTo(layer.get(), layerImpl.get());

    // We should now have no textures on the impl thread.
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 1));

    // This should recreate and update one of the deleted textures.
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    updateAndPush(layer, layerImpl);

    // We should have one tiles on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushIdlePaintTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // The tile size is 100x100. Setup 5x5 tiles with one visible tile in the center.
    // This paints 1 visible of the 25 invalid tiles.
    layer->SetBounds(gfx::Size(500, 500));
    calcDrawProps(layer);
    layer->draw_properties().visible_content_rect = gfx::Rect(200, 200, 100, 100);
    bool needsUpdate = updateAndPush(layer, layerImpl);
    // We should need idle-painting for surrounding tiles.
    EXPECT_TRUE(needsUpdate);

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(2, 2));

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
            EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(i, j));
    }

    EXPECT_FALSE(needsUpdate);
}

TEST_F(TiledLayerTest, predictivePainting)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

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
        gfx::Size bounds = gfx::Size(500, 500);
        gfx::Rect visibleRect = gfx::Rect(200, 200, 100, 100);
        gfx::Rect previousVisibleRect = gfx::Rect(visibleRect.origin() + directions[k], visibleRect.size());
        gfx::Rect nextVisibleRect = gfx::Rect(visibleRect.origin() - directions[k], visibleRect.size());

        // Setup. Use the previousVisibleRect to setup the prediction for next frame.
        layer->SetBounds(bounds);
        calcDrawProps(layer);
        layer->draw_properties().visible_content_rect = previousVisibleRect;
        bool needsUpdate = updateAndPush(layer, layerImpl);

        // Invalidate and move the visibleRect in the scroll direction.
        // Check that the correct tiles have been painted in the visible pass.
        layer->SetNeedsDisplay();
        layer->draw_properties().visible_content_rect = visibleRect;
        needsUpdate = updateAndPush(layer, layerImpl);
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++)
                EXPECT_EQ(layerImpl->HasResourceIdForTileAt(i, j), pushedVisibleTiles[k].Contains(i, j));
        }

        // Move the transform in the same direction without invalidating.
        // Check that non-visible pre-painting occured in the correct direction.
        // Ignore diagonal scrolls here (k > 3) as these have new visible content now.
        if (k <= 3) {
            layer->draw_properties().visible_content_rect = nextVisibleRect;
            needsUpdate = updateAndPush(layer, layerImpl);
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 5; j++)
                    EXPECT_EQ(layerImpl->HasResourceIdForTileAt(i, j), pushedPrepaintTiles[k].Contains(i, j));
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
    m_resourceManager->SetMaxMemoryLimitBytes(2 * 1024 * 1024);
    scoped_refptr<FakeTiledLayer> layer1 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl1 = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
    scoped_refptr<FakeTiledLayer> layer2 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl2 = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 2));

    // For this test we have two layers. layer1 exhausts most texture memory, leaving room for 2 more tiles from
    // layer2, but not all three tiles. First we paint layer1, and one tile from layer2. Then when we idle paint
    // layer2, we will fail on the third tile of layer2, and this should not leave the second tile in a bad state.

    // This uses 960000 bytes, leaving 88576 bytes of memory left, which is enough for 2 tiles only in the other layer.
    gfx::Rect layer1Rect(0, 0, 100, 2400);

    // This requires 4*30000 bytes of memory.
    gfx::Rect layer2Rect(0, 0, 100, 300);

    // Paint a single tile in layer2 so that it will idle paint.
    layer1->SetBounds(layer1Rect.size());
    layer2->SetBounds(layer2Rect.size());
    calcDrawProps(layer1, layer2);
    layer1->draw_properties().visible_content_rect = layer1Rect;
    layer2->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    bool needsUpdate = updateAndPush(layer1, layerImpl1,
                                     layer2, layerImpl2);
    // We should need idle-painting for both remaining tiles in layer2.
    EXPECT_TRUE(needsUpdate);

    // Reduce our memory limits to 1mb.
    m_resourceManager->SetMaxMemoryLimitBytes(1024 * 1024);

    // Now idle paint layer2. We are going to run out of memory though!
    // Oh well, commit the frame and push.
    for (int i = 0; i < 4; i++) {
        needsUpdate = updateAndPush(layer1, layerImpl1,
                                    layer2, layerImpl2);
    }

    // Sanity check, we should have textures for the big layer.
    EXPECT_TRUE(layerImpl1->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl1->HasResourceIdForTileAt(0, 23));

    // We should only have the first two tiles from layer2 since
    // it failed to idle update the last tile.
    EXPECT_TRUE(layerImpl2->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->HasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layerImpl2->HasResourceIdForTileAt(0, 1));

    EXPECT_FALSE(needsUpdate);
    EXPECT_FALSE(layerImpl2->HasResourceIdForTileAt(0, 2));
}

TEST_F(TiledLayerTest, pushIdlePaintedOccludedTiles)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;
    
    // The tile size is 100x100, so this invalidates one occluded tile, culls it during paint, but prepaints it.
    occluded.setOcclusion(gfx::Rect(0, 0, 100, 100));

    layer->SetBounds(gfx::Size(100, 100));
    calcDrawProps(layer);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    updateAndPush(layer, layerImpl);

    // We should have the prepainted tile on the impl side, but culled it during paint.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_EQ(1, occluded.overdraw_metrics()->tiles_culled_for_upload());
}

TEST_F(TiledLayerTest, pushTilesMarkedDirtyDuringPaint)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    // However, during the paint, we invalidate one of the tiles. This should
    // not prevent the tile from being pushed.
    layer->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(0, 50, 100, 50), layer.get());
    layer->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushTilesLayerMarkedDirtyDuringPaintOnNextLayer)
{
    scoped_refptr<FakeTiledLayer> layer1 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_refptr<FakeTiledLayer> layer2 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layer1Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
    scoped_ptr<FakeTiledLayerImpl> layer2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 2));

    // Invalidate a tile on layer1, during update of layer 2.
    layer2->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(0, 50, 100, 50), layer1.get());
    layer1->SetBounds(gfx::Size(100, 200));
    layer2->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer1, layer2);
    layer1->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer2->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer1, layer1Impl,
                  layer2, layer2Impl);

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->HasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, pushTilesLayerMarkedDirtyDuringPaintOnPreviousLayer)
{
    scoped_refptr<FakeTiledLayer> layer1 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_refptr<FakeTiledLayer> layer2 = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layer1Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
    scoped_ptr<FakeTiledLayerImpl> layer2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 2));

    layer1->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(0, 50, 100, 50), layer2.get());
    layer1->SetBounds(gfx::Size(100, 200));
    layer2->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer1, layer2);
    layer1->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    layer2->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer1, layer1Impl,
                  layer2, layer2Impl);

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->HasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->HasResourceIdForTileAt(0, 1));
}

TEST_F(TiledLayerTest, paintSmallAnimatedLayersImmediately)
{
    // Create a LayerTreeHost that has the right viewportsize,
    // so the layer is considered small enough.
    bool runOutOfMemory[2] = {false, true};
    for (int i = 0; i < 2; i++) {
        // Create a layer with 5x5 tiles, with 4x4 size viewport.
        int viewportWidth  = 4 * FakeTiledLayer::tileSize().width();
        int viewportHeight = 4 * FakeTiledLayer::tileSize().width();
        int layerWidth  = 5 * FakeTiledLayer::tileSize().width();
        int layerHeight = 5 * FakeTiledLayer::tileSize().height();
        int memoryForLayer = layerWidth * layerHeight * 4;
        layer_tree_host_->SetViewportSize(gfx::Size(layerWidth, layerHeight), gfx::Size(layerWidth, layerHeight));

        // Use 10x5 tiles to run out of memory.
        if (runOutOfMemory[i])
            layerWidth *= 2;

        m_resourceManager->SetMaxMemoryLimitBytes(memoryForLayer);

        scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

        // Full size layer with half being visible.
        layer->SetBounds(gfx::Size(layerWidth, layerHeight));
        gfx::Rect visibleRect(gfx::Point(), gfx::Size(layerWidth / 2, layerHeight));
        calcDrawProps(layer);

        // Pretend the layer is animating.
        layer->draw_properties().target_space_transform_is_animating = true;
        layer->draw_properties().visible_content_rect = visibleRect;
        layer->SetLayerTreeHost(layer_tree_host_.get());

        // The layer should paint its entire contents on the first paint
        // if it is close to the viewport size and has the available memory.
        layer->SetTexturePriorities(m_priorityCalculator);
        m_resourceManager->PrioritizeTextures();
        layer->Update(m_queue.get(), 0, NULL);
        updateTextures();
        layerPushPropertiesTo(layer.get(), layerImpl.get());

        // We should have all the tiles for the small animated layer.
        // We should still have the visible tiles when we didn't
        // have enough memory for all the tiles.
        if (!runOutOfMemory[i]) {
            for (int i = 0; i < 5; ++i) {
                for (int j = 0; j < 5; ++j)
                    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(i, j));
            }
        } else {
            for (int i = 0; i < 10; ++i) {
                for (int j = 0; j < 5; ++j)
                    EXPECT_EQ(layerImpl->HasResourceIdForTileAt(i, j), i < 5);
            }
        }
    }
}

TEST_F(TiledLayerTest, idlePaintOutOfMemory)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // We have enough memory for only the visible rect, so we will run out of memory in first idle paint.
    int memoryLimit = 4 * 100 * 100; // 1 tiles, 4 bytes per pixel.
    m_resourceManager->SetMaxMemoryLimitBytes(memoryLimit);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    bool needsUpdate = false;
    layer->SetBounds(gfx::Size(300, 300));
    calcDrawProps(layer);
    layer->draw_properties().visible_content_rect = gfx::Rect(100, 100, 100, 100);
    for (int i = 0; i < 2; i++)
        needsUpdate = updateAndPush(layer, layerImpl);

    // Idle-painting should see no more priority tiles for painting.
    EXPECT_FALSE(needsUpdate);

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(1, 1));
}

TEST_F(TiledLayerTest, idlePaintZeroSizedLayer)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    bool animating[2] = {false, true};
    for (int i = 0; i < 2; i++) {
        // Pretend the layer is animating.
        layer->draw_properties().target_space_transform_is_animating = animating[i];

        // The layer's bounds are empty.
        // Empty layers don't paint or idle-paint.
        layer->SetBounds(gfx::Size());
        calcDrawProps(layer);
        layer->draw_properties().visible_content_rect = gfx::Rect();
        bool needsUpdate = updateAndPush(layer, layerImpl);
        
        // Empty layers don't have tiles.
        EXPECT_EQ(0u, layer->NumPaintedTiles());

        // Empty layers don't need prepaint.
        EXPECT_FALSE(needsUpdate);

        // Empty layers don't have tiles.
        EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 0));
    }
}

TEST_F(TiledLayerTest, idlePaintNonVisibleLayers)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // Alternate between not visible and visible.
    gfx::Rect v(0, 0, 100, 100);
    gfx::Rect nv(0, 0, 0, 0);
    gfx::Rect visibleRect[10] = {nv, nv, v, v, nv, nv, v, v, nv, nv};
    bool invalidate[10] =  {true, true, true, true, true, true, true, true, false, false };

    // We should not have any tiles except for when the layer was visible
    // or after the layer was visible and we didn't invalidate.
    bool haveTile[10] = { false, false, true, true, false, false, true, true, true, true };
    
    for (int i = 0; i < 10; i++) {
        layer->SetBounds(gfx::Size(100, 100));
        calcDrawProps(layer);
        layer->draw_properties().visible_content_rect = visibleRect[i];

        if (invalidate[i])
            layer->InvalidateContentRect(gfx::Rect(0, 0, 100, 100));
        bool needsUpdate = updateAndPush(layer, layerImpl);
        
        // We should never signal idle paint, as we painted the entire layer
        // or the layer was not visible.
        EXPECT_FALSE(needsUpdate);
        EXPECT_EQ(layerImpl->HasResourceIdForTileAt(0, 0), haveTile[i]);
    }
}

TEST_F(TiledLayerTest, invalidateFromPrepare)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 200);
    updateAndPush(layer, layerImpl);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 1));

    layer->fakeLayerUpdater()->clearPrepareCount();
    // Invoke update again. As the layer is valid update shouldn't be invoked on
    // the LayerUpdater.
    updateAndPush(layer, layerImpl);
    EXPECT_EQ(0, layer->fakeLayerUpdater()->prepareCount());

    // setRectToInvalidate triggers InvalidateContentRect() being invoked from update.
    layer->fakeLayerUpdater()->setRectToInvalidate(gfx::Rect(25, 25, 50, 50), layer.get());
    layer->fakeLayerUpdater()->clearPrepareCount();
    layer->InvalidateContentRect(gfx::Rect(0, 0, 50, 50));
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

    layer->SetBounds(layerBounds.size());
    layer->setContentBounds(contentBounds.size());
    layer->draw_properties().visible_content_rect = contentBounds;

    // On first update, the updateRect includes all tiles, even beyond the boundaries of the layer.
    // However, it should still be in layer space, not content space.
    layer->InvalidateContentRect(contentBounds);

    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), 0, NULL);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 300, 300 * 0.8), layer->updateRect());
    updateTextures();

    // After the tiles are updated once, another invalidate only needs to update the bounds of the layer.
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->InvalidateContentRect(contentBounds);
    layer->Update(m_queue.get(), 0, NULL);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(layerBounds), layer->updateRect());
    updateTextures();

    // Partial re-paint should also be represented by the updateRect in layer space, not content space.
    gfx::Rect partialDamage(30, 100, 10, 10);
    layer->InvalidateContentRect(partialDamage);
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), 0, NULL);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(45, 80, 15, 8), layer->updateRect());
}

TEST_F(TiledLayerTest, verifyInvalidationWhenContentsScaleChanges)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));

    // Create a layer with one tile.
    layer->SetBounds(gfx::Size(100, 100));
    calcDrawProps(layer);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 100, 100);
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Push the tiles to the impl side and check that there is exactly one.
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), 0, NULL);
    updateTextures();
    layerPushPropertiesTo(layer.get(), layerImpl.get());
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 1));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(1, 0));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(1, 1));

    layer->SetNeedsDisplayRect(gfx::Rect());
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(), layer->lastNeedsDisplayRect());

    // Change the contents scale.
    layer->updateContentsScale(2);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 200, 200);

    // The impl side should get 2x2 tiles now.
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), 0, NULL);
    updateTextures();
    layerPushPropertiesTo(layer.get(), layerImpl.get());
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(0, 1));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(1, 0));
    EXPECT_TRUE(layerImpl->HasResourceIdForTileAt(1, 1));

    // Verify that changing the contents scale caused invalidation, and
    // that the layer-space rectangle requiring painting is not scaled.
    EXPECT_FLOAT_RECT_EQ(gfx::RectF(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Invalidate the entire layer again, but do not paint. All tiles should be gone now from the
    // impl side.
    layer->SetNeedsDisplay();
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();

    layerPushPropertiesTo(layer.get(), layerImpl.get());
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(0, 1));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(1, 0));
    EXPECT_FALSE(layerImpl->HasResourceIdForTileAt(1, 1));
}

TEST_F(TiledLayerTest, skipsDrawGetsReset)
{
    // Create two 300 x 300 tiled layers.
    gfx::Size contentBounds(300, 300);
    gfx::Rect contentRect(gfx::Point(), contentBounds);

    // We have enough memory for only one of the two layers.
    int memoryLimit = 4 * 300 * 300; // 4 bytes per pixel.

    scoped_refptr<FakeTiledLayer> root_layer = make_scoped_refptr(new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
    scoped_refptr<FakeTiledLayer> childLayer = make_scoped_refptr(new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
    root_layer->AddChild(childLayer);

    root_layer->SetBounds(contentBounds);
    root_layer->draw_properties().visible_content_rect = contentRect;
    root_layer->SetPosition(gfx::PointF(0, 0));
    childLayer->SetBounds(contentBounds);
    childLayer->draw_properties().visible_content_rect = contentRect;
    childLayer->SetPosition(gfx::PointF(0, 0));
    root_layer->InvalidateContentRect(contentRect);
    childLayer->InvalidateContentRect(contentRect);

    layer_tree_host_->SetRootLayer(root_layer);
    layer_tree_host_->SetViewportSize(gfx::Size(300, 300), gfx::Size(300, 300));

    layer_tree_host_->UpdateLayers(m_queue.get(), memoryLimit);

    // We'll skip the root layer.
    EXPECT_TRUE(root_layer->SkipsDraw());
    EXPECT_FALSE(childLayer->SkipsDraw());

    layer_tree_host_->CommitComplete();

    // Remove the child layer.
    root_layer->RemoveAllChildren();

    layer_tree_host_->UpdateLayers(m_queue.get(), memoryLimit);
    EXPECT_FALSE(root_layer->SkipsDraw());

    resourceManagerClearAllMemory(layer_tree_host_->contents_texture_manager(), m_resourceProvider.get());
    layer_tree_host_->SetRootLayer(NULL);
}

TEST_F(TiledLayerTest, resizeToSmaller)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));

    layer->SetBounds(gfx::Size(700, 700));
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 700, 700);
    layer->InvalidateContentRect(gfx::Rect(0, 0, 700, 700));

    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), 0, NULL);

    layer->SetBounds(gfx::Size(200, 200));
    layer->InvalidateContentRect(gfx::Rect(0, 0, 200, 200));
}

TEST_F(TiledLayerTest, hugeLayerUpdateCrash)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));

    int size = 1 << 30;
    layer->SetBounds(gfx::Size(size, size));
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 700, 700);
    layer->InvalidateContentRect(gfx::Rect(0, 0, size, size));

    // Ensure no crash for bounds where size * size would overflow an int.
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), 0, NULL);
}

class TiledLayerPartialUpdateTest : public TiledLayerTest {
public:
    TiledLayerPartialUpdateTest()
    {
        m_settings.max_partial_texture_updates = 4;
    }
};

TEST_F(TiledLayerPartialUpdateTest, partialUpdates)
{
    // Create one 300 x 200 tiled layer with 3 x 2 tiles.
    gfx::Size contentBounds(300, 200);
    gfx::Rect contentRect(gfx::Point(), contentBounds);

    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
    layer->SetBounds(contentBounds);
    layer->SetPosition(gfx::PointF(0, 0));
    layer->draw_properties().visible_content_rect = contentRect;
    layer->InvalidateContentRect(contentRect);

    layer_tree_host_->SetRootLayer(layer);
    layer_tree_host_->SetViewportSize(gfx::Size(300, 200), gfx::Size(300, 200));

    // Full update of all 6 tiles.
    layer_tree_host_->UpdateLayers(m_queue.get(),
                                   std::numeric_limits<size_t>::max());
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
        EXPECT_EQ(6, m_queue->fullUploadSize());
        EXPECT_EQ(0, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    layer_tree_host_->CommitComplete();

    // Full update of 3 tiles and partial update of 3 tiles.
    layer->InvalidateContentRect(gfx::Rect(0, 0, 300, 150));
    layer_tree_host_->UpdateLayers(m_queue.get(), std::numeric_limits<size_t>::max());
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
        EXPECT_EQ(3, m_queue->fullUploadSize());
        EXPECT_EQ(3, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    layer_tree_host_->CommitComplete();

    // Partial update of 6 tiles.
    layer->InvalidateContentRect(gfx::Rect(50, 50, 200, 100));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
        layer_tree_host_->UpdateLayers(m_queue.get(), std::numeric_limits<size_t>::max());
        EXPECT_EQ(2, m_queue->fullUploadSize());
        EXPECT_EQ(4, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    layer_tree_host_->CommitComplete();

    // Checkerboard all tiles.
    layer->InvalidateContentRect(gfx::Rect(0, 0, 300, 200));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    layer_tree_host_->CommitComplete();

    // Partial update of 6 checkerboard tiles.
    layer->InvalidateContentRect(gfx::Rect(50, 50, 200, 100));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
        layer_tree_host_->UpdateLayers(m_queue.get(), std::numeric_limits<size_t>::max());
        EXPECT_EQ(6, m_queue->fullUploadSize());
        EXPECT_EQ(0, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(6, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    layer_tree_host_->CommitComplete();

    // Partial update of 4 tiles.
    layer->InvalidateContentRect(gfx::Rect(50, 50, 100, 100));
    {
        scoped_ptr<FakeTiledLayerImpl> layerImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), 1));
        layer_tree_host_->UpdateLayers(m_queue.get(), std::numeric_limits<size_t>::max());
        EXPECT_EQ(0, m_queue->fullUploadSize());
        EXPECT_EQ(4, m_queue->partialUploadSize());
        updateTextures();
        EXPECT_EQ(4, layer->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());
        layer->fakeLayerUpdater()->clearUpdateCount();
        layerPushPropertiesTo(layer.get(), layerImpl.get());
    }
    layer_tree_host_->CommitComplete();

    resourceManagerClearAllMemory(layer_tree_host_->contents_texture_manager(), m_resourceProvider.get());
    layer_tree_host_->SetRootLayer(NULL);
}

TEST_F(TiledLayerTest, tilesPaintedWithoutOcclusion)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->SetBounds(gfx::Size(100, 200));
    calcDrawProps(layer);

    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), 0, NULL);
    EXPECT_EQ(2, layer->fakeLayerUpdater()->updateCount());
}

TEST_F(TiledLayerTest, tilesPaintedWithOcclusion)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    layer_tree_host_->SetViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->SetBounds(gfx::Size(600, 600));
    calcDrawProps(layer);

    occluded.setOcclusion(gfx::Rect(200, 200, 300, 100));
    layer->draw_properties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
    layer->draw_properties().visible_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));

    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdraw_metrics()->tiles_culled_for_upload());

    layer->fakeLayerUpdater()->clearUpdateCount();
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();

    occluded.setOcclusion(gfx::Rect(250, 200, 300, 100));
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-2, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 330000 + 340000, 1);
    EXPECT_EQ(3 + 2, occluded.overdraw_metrics()->tiles_culled_for_upload());

    layer->fakeLayerUpdater()->clearUpdateCount();
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();

    occluded.setOcclusion(gfx::Rect(250, 250, 300, 100));
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 330000 + 340000 + 360000, 1);
    EXPECT_EQ(3 + 2, occluded.overdraw_metrics()->tiles_culled_for_upload());
}

TEST_F(TiledLayerTest, tilesPaintedWithOcclusionAndVisiblityConstraints)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    layer_tree_host_->SetViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->SetBounds(gfx::Size(600, 600));
    calcDrawProps(layer);

    // The partially occluded tiles (by the 150 occlusion height) are visible beyond the occlusion, so not culled.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 150));
    layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 360);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 360);
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));

    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(24-3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 210000, 1);
    EXPECT_EQ(3, occluded.overdraw_metrics()->tiles_culled_for_upload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // Now the visible region stops at the edge of the occlusion so the partly visible tiles become fully occluded.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 150));
    layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 350);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 350);
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(24-6, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 210000 + 180000, 1);
    EXPECT_EQ(3 + 6, occluded.overdraw_metrics()->tiles_culled_for_upload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // Now the visible region is even smaller than the occlusion, it should have the same result.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 150));
    layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 340);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 340);
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(24-6, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 210000 + 180000 + 180000, 1);
    EXPECT_EQ(3 + 6 + 6, occluded.overdraw_metrics()->tiles_culled_for_upload());

}

TEST_F(TiledLayerTest, tilesNotPaintedWithoutInvalidation)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    layer_tree_host_->SetViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->SetBounds(gfx::Size(600, 600));
    calcDrawProps(layer);

    occluded.setOcclusion(gfx::Rect(200, 200, 300, 100));
    layer->draw_properties().drawable_content_rect = gfx::Rect(0, 0, 600, 600);
    layer->draw_properties().visible_content_rect = gfx::Rect(0, 0, 600, 600);
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-3, layer->fakeLayerUpdater()->updateCount());
    {
        updateTextures();
    }

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdraw_metrics()->tiles_culled_for_upload());

    layer->fakeLayerUpdater()->clearUpdateCount();
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();

    // Repaint without marking it dirty. The 3 culled tiles will be pre-painted now.
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 330000, 1);
    EXPECT_EQ(6, occluded.overdraw_metrics()->tiles_culled_for_upload());
}

TEST_F(TiledLayerTest, tilesPaintedWithOcclusionAndTransforms)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;

    // The tile size is 100x100.

    // This makes sure the painting works when the occluded region (in screen space)
    // is transformed differently than the layer.
    layer_tree_host_->SetViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->SetBounds(gfx::Size(600, 600));
    calcDrawProps(layer);
    gfx::Transform screenTransform;
    screenTransform.Scale(0.5, 0.5);
    layer->draw_properties().screen_space_transform = screenTransform;
    layer->draw_properties().target_space_transform = screenTransform;

    occluded.setOcclusion(gfx::Rect(100, 100, 150, 50));
    layer->draw_properties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
    layer->draw_properties().visible_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(36-3, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdraw_metrics()->tiles_culled_for_upload());
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
    layer_tree_host_->SetViewportSize(gfx::Size(600, 600), gfx::Size(600, 600));
    layer->SetBounds(gfx::Size(600, 600));
    layer->SetRasterScale(0.5);
    calcDrawProps(layer);
    EXPECT_FLOAT_EQ(layer->contents_scale_x(), layer->contents_scale_y());
    gfx::Transform drawTransform;
    double invScaleFactor = 1 / layer->contents_scale_x();
    drawTransform.Scale(invScaleFactor, invScaleFactor);
    layer->draw_properties().target_space_transform = drawTransform;
    layer->draw_properties().screen_space_transform = drawTransform;

    occluded.setOcclusion(gfx::Rect(200, 200, 300, 100));
    layer->draw_properties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->bounds());
    layer->draw_properties().visible_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    // The content is half the size of the layer (so the number of tiles is fewer).
    // In this case, the content is 300x300, and since the tile size is 100, the
    // number of tiles 3x3.
    EXPECT_EQ(9, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 90000, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // This makes sure the painting works when the content space is scaled to
    // a different layer space. In this case the occluded region catches the
    // blown up tiles.
    occluded.setOcclusion(gfx::Rect(200, 200, 300, 200));
    layer->draw_properties().drawable_content_rect = gfx::Rect(gfx::Point(), layer->bounds());
    layer->draw_properties().visible_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(9-1, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 90000 + 80000, 1);
    EXPECT_EQ(1, occluded.overdraw_metrics()->tiles_culled_for_upload());

    layer->fakeLayerUpdater()->clearUpdateCount();

    // This makes sure content scaling and transforms work together.
    gfx::Transform screenTransform;
    screenTransform.Scale(0.5, 0.5);
    layer->draw_properties().screen_space_transform = screenTransform;
    layer->draw_properties().target_space_transform = screenTransform;

    occluded.setOcclusion(gfx::Rect(100, 100, 150, 100));

    gfx::Rect layerBoundsRect(gfx::Point(), layer->bounds());
    layer->draw_properties().drawable_content_rect = gfx::ToEnclosingRect(gfx::ScaleRect(layerBoundsRect, 0.5));
    layer->draw_properties().visible_content_rect = gfx::Rect(gfx::Point(), layer->content_bounds());
    layer->InvalidateContentRect(gfx::Rect(0, 0, 600, 600));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    EXPECT_EQ(9-1, layer->fakeLayerUpdater()->updateCount());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 90000 + 80000 + 80000, 1);
    EXPECT_EQ(1 + 1, occluded.overdraw_metrics()->tiles_culled_for_upload());
}

TEST_F(TiledLayerTest, visibleContentOpaqueRegion)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;
    layer_tree_host_->SetViewportSize(gfx::Size(1000, 1000), gfx::Size(1000, 1000));

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    gfx::Rect opaquePaintRect;
    Region opaqueContents;

    gfx::Rect contentBounds = gfx::Rect(0, 0, 100, 200);
    gfx::Rect visibleBounds = gfx::Rect(0, 0, 100, 150);

    layer->SetBounds(contentBounds.size());
    calcDrawProps(layer);
    layer->draw_properties().drawable_content_rect = visibleBounds;
    layer->draw_properties().visible_content_rect = visibleBounds;

    // If the layer doesn't paint opaque content, then the visibleContentOpaqueRegion should be empty.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->InvalidateContentRect(contentBounds);
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    opaqueContents = layer->VisibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.IsEmpty());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_painted(), 20000, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 20000, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    // visibleContentOpaqueRegion should match the visible part of what is painted opaque.
    opaquePaintRect = gfx::Rect(10, 10, 90, 190);
    layer->fakeLayerUpdater()->setOpaquePaintRect(opaquePaintRect);
    layer->InvalidateContentRect(contentBounds);
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->VisibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(opaquePaintRect, visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_painted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 20000 + 20000 - 17100, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    // If we paint again without invalidating, the same stuff should be opaque.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->VisibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(opaquePaintRect, visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_painted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 20000 + 20000 - 17100, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    // If we repaint a non-opaque part of the tile, then it shouldn't lose its opaque-ness. And other tiles should
    // not be affected.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->InvalidateContentRect(gfx::Rect(0, 0, 1, 1));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->VisibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(opaquePaintRect, visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_painted(), 20000 * 2 + 1, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 20000 + 20000 - 17100 + 1, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    // If we repaint an opaque part of the tile, then it should lose its opaque-ness. But other tiles should still
    // not be affected.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->InvalidateContentRect(gfx::Rect(10, 10, 1, 1));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->VisibleContentOpaqueRegion();
    EXPECT_EQ(gfx::IntersectRects(gfx::Rect(10, 100, 90, 100), visibleBounds).ToString(), opaqueContents.ToString());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_painted(), 20000 * 2 + 1  + 1, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 20000 + 20000 - 17100 + 1 + 1, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());
}

TEST_F(TiledLayerTest, pixels_paintedMetrics)
{
    scoped_refptr<FakeTiledLayer> layer = make_scoped_refptr(new FakeTiledLayer(m_resourceManager.get()));
    TestOcclusionTracker occluded;
    m_occlusion = &occluded;
    layer_tree_host_->SetViewportSize(gfx::Size(1000, 1000), gfx::Size(1000, 1000));

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    gfx::Rect opaquePaintRect;
    Region opaqueContents;

    gfx::Rect contentBounds = gfx::Rect(0, 0, 100, 300);
    layer->SetBounds(contentBounds.size());
    calcDrawProps(layer);

    // Invalidates and paints the whole layer.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->InvalidateContentRect(contentBounds);
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->VisibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.IsEmpty());

    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_painted(), 30000, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 30000, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());

    // Invalidates an area on the top and bottom tile, which will cause us to paint the tile in the middle,
    // even though it is not dirty and will not be uploaded.
    layer->fakeLayerUpdater()->setOpaquePaintRect(gfx::Rect());
    layer->InvalidateContentRect(gfx::Rect(0, 0, 1, 1));
    layer->InvalidateContentRect(gfx::Rect(50, 200, 10, 10));
    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();
    layer->Update(m_queue.get(), &occluded, NULL);
    updateTextures();
    opaqueContents = layer->VisibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.IsEmpty());

    // The middle tile was painted even though not invalidated.
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_painted(), 30000 + 60 * 210, 1);
    // The pixels uploaded will not include the non-invalidated tile in the middle.
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_opaque(), 0, 1);
    EXPECT_NEAR(occluded.overdraw_metrics()->pixels_uploaded_translucent(), 30000 + 1 + 100, 1);
    EXPECT_EQ(0, occluded.overdraw_metrics()->tiles_culled_for_upload());
}

TEST_F(TiledLayerTest, dontAllocateContentsWhenTargetSurfaceCantBeAllocated)
{
    // Tile size is 100x100.
    gfx::Rect rootRect(0, 0, 300, 200);
    gfx::Rect childRect(0, 0, 300, 100);
    gfx::Rect child2Rect(0, 100, 300, 100);

    scoped_refptr<FakeTiledLayer> root = make_scoped_refptr(new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
    scoped_refptr<Layer> surface = Layer::Create();
    scoped_refptr<FakeTiledLayer> child = make_scoped_refptr(new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));
    scoped_refptr<FakeTiledLayer> child2 = make_scoped_refptr(new FakeTiledLayer(layer_tree_host_->contents_texture_manager()));

    root->SetBounds(rootRect.size());
    root->SetAnchorPoint(gfx::PointF());
    root->draw_properties().drawable_content_rect = rootRect;
    root->draw_properties().visible_content_rect = rootRect;
    root->AddChild(surface);

    surface->SetForceRenderSurface(true);
    surface->SetAnchorPoint(gfx::PointF());
    surface->SetOpacity(0.5);
    surface->AddChild(child);
    surface->AddChild(child2);

    child->SetBounds(childRect.size());
    child->SetAnchorPoint(gfx::PointF());
    child->SetPosition(childRect.origin());
    child->draw_properties().visible_content_rect = childRect;
    child->draw_properties().drawable_content_rect = rootRect;

    child2->SetBounds(child2Rect.size());
    child2->SetAnchorPoint(gfx::PointF());
    child2->SetPosition(child2Rect.origin());
    child2->draw_properties().visible_content_rect = child2Rect;
    child2->draw_properties().drawable_content_rect = rootRect;

    layer_tree_host_->SetRootLayer(root);
    layer_tree_host_->SetViewportSize(rootRect.size(), rootRect.size());

    // With a huge memory limit, all layers should update and push their textures.
    root->InvalidateContentRect(rootRect);
    child->InvalidateContentRect(childRect);
    child2->InvalidateContentRect(child2Rect);
    layer_tree_host_->UpdateLayers(m_queue.get(), std::numeric_limits<size_t>::max());
    {
        updateTextures();
        EXPECT_EQ(6, root->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(3, child->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(3, child2->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());

        root->fakeLayerUpdater()->clearUpdateCount();
        child->fakeLayerUpdater()->clearUpdateCount();
        child2->fakeLayerUpdater()->clearUpdateCount();

        scoped_ptr<FakeTiledLayerImpl> rootImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), root->id()));
        scoped_ptr<FakeTiledLayerImpl> childImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), child->id()));
        scoped_ptr<FakeTiledLayerImpl> child2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), child2->id()));
        layerPushPropertiesTo(root.get(), rootImpl.get());
        layerPushPropertiesTo(child.get(), childImpl.get());
        layerPushPropertiesTo(child2.get(), child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_TRUE(rootImpl->HasResourceIdForTileAt(i, j));
            EXPECT_TRUE(childImpl->HasResourceIdForTileAt(i, 0));
            EXPECT_TRUE(child2Impl->HasResourceIdForTileAt(i, 0));
        }
    }
    layer_tree_host_->CommitComplete();

    // With a memory limit that includes only the root layer (3x2 tiles) and half the surface that
    // the child layers draw into, the child layers will not be allocated. If the surface isn't
    // accounted for, then one of the children would fit within the memory limit.
    root->InvalidateContentRect(rootRect);
    child->InvalidateContentRect(childRect);
    child2->InvalidateContentRect(child2Rect);
    layer_tree_host_->UpdateLayers(m_queue.get(), (3 * 2 + 3 * 1) * (100 * 100) * 4);
    {
        updateTextures();
        EXPECT_EQ(6, root->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child2->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());

        root->fakeLayerUpdater()->clearUpdateCount();
        child->fakeLayerUpdater()->clearUpdateCount();
        child2->fakeLayerUpdater()->clearUpdateCount();

        scoped_ptr<FakeTiledLayerImpl> rootImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), root->id()));
        scoped_ptr<FakeTiledLayerImpl> childImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), child->id()));
        scoped_ptr<FakeTiledLayerImpl> child2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), child2->id()));
        layerPushPropertiesTo(root.get(), rootImpl.get());
        layerPushPropertiesTo(child.get(), childImpl.get());
        layerPushPropertiesTo(child2.get(), child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_TRUE(rootImpl->HasResourceIdForTileAt(i, j));
            EXPECT_FALSE(childImpl->HasResourceIdForTileAt(i, 0));
            EXPECT_FALSE(child2Impl->HasResourceIdForTileAt(i, 0));
        }
    }
    layer_tree_host_->CommitComplete();

    // With a memory limit that includes only half the root layer, no contents will be
    // allocated. If render surface memory wasn't accounted for, there is enough space
    // for one of the children layers, but they draw into a surface that can't be
    // allocated.
    root->InvalidateContentRect(rootRect);
    child->InvalidateContentRect(childRect);
    child2->InvalidateContentRect(child2Rect);
    layer_tree_host_->UpdateLayers(m_queue.get(), (3 * 1) * (100 * 100) * 4);
    {
        updateTextures();
        EXPECT_EQ(0, root->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child->fakeLayerUpdater()->updateCount());
        EXPECT_EQ(0, child2->fakeLayerUpdater()->updateCount());
        EXPECT_FALSE(m_queue->hasMoreUpdates());

        root->fakeLayerUpdater()->clearUpdateCount();
        child->fakeLayerUpdater()->clearUpdateCount();
        child2->fakeLayerUpdater()->clearUpdateCount();

        scoped_ptr<FakeTiledLayerImpl> rootImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), root->id()));
        scoped_ptr<FakeTiledLayerImpl> childImpl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), child->id()));
        scoped_ptr<FakeTiledLayerImpl> child2Impl = make_scoped_ptr(new FakeTiledLayerImpl(m_hostImpl->active_tree(), child2->id()));
        layerPushPropertiesTo(root.get(), rootImpl.get());
        layerPushPropertiesTo(child.get(), childImpl.get());
        layerPushPropertiesTo(child2.get(), child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_FALSE(rootImpl->HasResourceIdForTileAt(i, j));
            EXPECT_FALSE(childImpl->HasResourceIdForTileAt(i, 0));
            EXPECT_FALSE(child2Impl->HasResourceIdForTileAt(i, 0));
        }
    }
    layer_tree_host_->CommitComplete();

    resourceManagerClearAllMemory(layer_tree_host_->contents_texture_manager(), m_resourceProvider.get());
    layer_tree_host_->SetRootLayer(NULL);
}

class TrackingLayerPainter : public LayerPainter {
public:
    static scoped_ptr<TrackingLayerPainter> Create() { return make_scoped_ptr(new TrackingLayerPainter()); }

    virtual void Paint(SkCanvas* canvas, gfx::Rect content_rect, gfx::RectF* opaque) OVERRIDE
    {
        m_paintedRect = content_rect;
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
        scoped_ptr<TrackingLayerPainter> trackingLayerPainter(TrackingLayerPainter::Create());
        m_trackingLayerPainter = trackingLayerPainter.get();
        m_layerUpdater = BitmapContentLayerUpdater::Create(trackingLayerPainter.PassAs<LayerPainter>());
    }

    TrackingLayerPainter* trackingLayerPainter() const { return m_trackingLayerPainter; }

protected:
    virtual ~UpdateTrackingTiledLayer() { }

    virtual LayerUpdater* Updater() const OVERRIDE { return m_layerUpdater.get(); }

private:
    TrackingLayerPainter* m_trackingLayerPainter;
    scoped_refptr<BitmapContentLayerUpdater> m_layerUpdater;
};

TEST_F(TiledLayerTest, nonIntegerContentsScaleIsNotDistortedDuringPaint)
{
    scoped_refptr<UpdateTrackingTiledLayer> layer = make_scoped_refptr(new UpdateTrackingTiledLayer(m_resourceManager.get()));

    gfx::Rect layerRect(0, 0, 30, 31);
    layer->SetPosition(layerRect.origin());
    layer->SetBounds(layerRect.size());
    layer->updateContentsScale(1.5);

    gfx::Rect contentRect(0, 0, 45, 47);
    EXPECT_EQ(contentRect.size(), layer->content_bounds());
    layer->draw_properties().visible_content_rect = contentRect;
    layer->draw_properties().drawable_content_rect = contentRect;

    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();

    // Update the whole tile.
    layer->Update(m_queue.get(), 0, NULL);
    layer->trackingLayerPainter()->resetPaintedRect();

    EXPECT_RECT_EQ(gfx::Rect(), layer->trackingLayerPainter()->paintedRect());
    updateTextures();

    // Invalidate the entire layer in content space. When painting, the rect given to webkit should match the layer's bounds.
    layer->InvalidateContentRect(contentRect);
    layer->Update(m_queue.get(), 0, NULL);

    EXPECT_RECT_EQ(layerRect, layer->trackingLayerPainter()->paintedRect());
}

TEST_F(TiledLayerTest, nonIntegerContentsScaleIsNotDistortedDuringInvalidation)
{
    scoped_refptr<UpdateTrackingTiledLayer> layer = make_scoped_refptr(new UpdateTrackingTiledLayer(m_resourceManager.get()));

    gfx::Rect layerRect(0, 0, 30, 31);
    layer->SetPosition(layerRect.origin());
    layer->SetBounds(layerRect.size());
    layer->updateContentsScale(1.3f);

    gfx::Rect contentRect(gfx::Point(), layer->content_bounds());
    layer->draw_properties().visible_content_rect = contentRect;
    layer->draw_properties().drawable_content_rect = contentRect;

    layer->SetTexturePriorities(m_priorityCalculator);
    m_resourceManager->PrioritizeTextures();

    // Update the whole tile.
    layer->Update(m_queue.get(), 0, NULL);
    layer->trackingLayerPainter()->resetPaintedRect();

    EXPECT_RECT_EQ(gfx::Rect(), layer->trackingLayerPainter()->paintedRect());
    updateTextures();

    // Invalidate the entire layer in layer space. When painting, the rect given to webkit should match the layer's bounds.
    layer->SetNeedsDisplayRect(layerRect);
    layer->Update(m_queue.get(), 0, NULL);

    EXPECT_RECT_EQ(layerRect, layer->trackingLayerPainter()->paintedRect());
}

}  // namespace
}  // namespace cc
