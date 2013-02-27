// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/tiled_layer_test_common.h"

using cc::LayerTilingData;
using cc::LayerUpdater;
using cc::PriorityCalculator;
using cc::PrioritizedResource;
using cc::PrioritizedResourceManager;
using cc::RenderingStats;
using cc::ResourceUpdate;
using cc::ResourceUpdateQueue;

namespace cc {

FakeLayerUpdater::Resource::Resource(FakeLayerUpdater* layer, scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass())
    , m_layer(layer)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    m_bitmap.allocPixels();
}

FakeLayerUpdater::Resource::~Resource()
{
}

void FakeLayerUpdater::Resource::update(ResourceUpdateQueue& queue, const gfx::Rect&, const gfx::Vector2d&, bool partialUpdate, RenderingStats*)
{
    const gfx::Rect rect(0, 0, 10, 10);
    ResourceUpdate upload = ResourceUpdate::Create(
        texture(), &m_bitmap, rect, rect, gfx::Vector2d());
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);

    m_layer->update();
}

FakeLayerUpdater::FakeLayerUpdater()
    : m_prepareCount(0)
    , m_updateCount(0)
{
}

FakeLayerUpdater::~FakeLayerUpdater()
{
}

void FakeLayerUpdater::prepareToUpdate(const gfx::Rect& contentRect, const gfx::Size&, float, float, gfx::Rect& resultingOpaqueRect, RenderingStats*)
{
    m_prepareCount++;
    m_lastUpdateRect = contentRect;
    if (!m_rectToInvalidate.IsEmpty()) {
        m_layer->invalidateContentRect(m_rectToInvalidate);
        m_rectToInvalidate = gfx::Rect();
        m_layer = NULL;
    }
    resultingOpaqueRect = m_opaquePaintRect;
}

void FakeLayerUpdater::setRectToInvalidate(const gfx::Rect& rect, FakeTiledLayer* layer)
{
    m_rectToInvalidate = rect;
    m_layer = layer;
}

scoped_ptr<LayerUpdater::Resource> FakeLayerUpdater::createResource(PrioritizedResourceManager* manager)
{
    return scoped_ptr<LayerUpdater::Resource>(new Resource(this, PrioritizedResource::create(manager)));
}

FakeTiledLayerImpl::FakeTiledLayerImpl(LayerTreeImpl* treeImpl, int id)
    : TiledLayerImpl(treeImpl, id)
{
}

FakeTiledLayerImpl::~FakeTiledLayerImpl()
{
}

FakeTiledLayer::FakeTiledLayer(PrioritizedResourceManager* resourceManager)
    : TiledLayer()
    , m_fakeUpdater(make_scoped_refptr(new FakeLayerUpdater))
    , m_resourceManager(resourceManager)
{
    setTileSize(tileSize());
    setTextureFormat(GL_RGBA);
    setBorderTexelOption(LayerTilingData::NoBorderTexels);
    setIsDrawable(true); // So that we don't get false positives if any of these tests expect to return false from drawsContent() for other reasons.
}

FakeTiledLayerWithScaledBounds::FakeTiledLayerWithScaledBounds(PrioritizedResourceManager* resourceManager)
    : FakeTiledLayer(resourceManager)
{
}

FakeTiledLayerWithScaledBounds::~FakeTiledLayerWithScaledBounds()
{
}

FakeTiledLayer::~FakeTiledLayer()
{
}

void FakeTiledLayer::setNeedsDisplayRect(const gfx::RectF& rect)
{
    m_lastNeedsDisplayRect = rect;
    TiledLayer::setNeedsDisplayRect(rect);
}

void FakeTiledLayer::setTexturePriorities(const PriorityCalculator& calculator)
{
    // Ensure there is always a target render surface available. If none has been
    // set (the layer is an orphan for the test), then just set a surface on itself.
    bool missingTargetRenderSurface = !renderTarget();

    if (missingTargetRenderSurface)
        createRenderSurface();

    TiledLayer::setTexturePriorities(calculator);

    if (missingTargetRenderSurface) {
        clearRenderSurface();
        drawProperties().render_target = 0;
    }
}

cc::PrioritizedResourceManager* FakeTiledLayer::resourceManager() const
{
    return m_resourceManager;
}

void FakeTiledLayer::updateContentsScale(float idealContentsScale)
{
    calculateContentsScale(
        idealContentsScale,
        false,  // animating_transform_to_screen
        &drawProperties().contents_scale_x,
        &drawProperties().contents_scale_y,
        &drawProperties().content_bounds);
}

cc::LayerUpdater* FakeTiledLayer::updater() const
{
    return m_fakeUpdater.get();
}

void FakeTiledLayerWithScaledBounds::setContentBounds(const gfx::Size& contentBounds)
{
    m_forcedContentBounds = contentBounds;
    drawProperties().content_bounds = m_forcedContentBounds;
}

void FakeTiledLayerWithScaledBounds::calculateContentsScale(
    float idealContentsScale,
    bool animatingTransformToScreen,
    float* contentsScaleX,
    float* contentsScaleY,
    gfx::Size* contentBounds)
{
    *contentsScaleX = static_cast<float>(m_forcedContentBounds.width()) / bounds().width();
    *contentsScaleY = static_cast<float>(m_forcedContentBounds.height()) / bounds().height();
    *contentBounds = m_forcedContentBounds;
}

}  // namespace cc
