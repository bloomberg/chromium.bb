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

void FakeLayerUpdater::Resource::Update(ResourceUpdateQueue* queue, gfx::Rect sourceRect, gfx::Vector2d destOffset, bool partialUpdate, RenderingStats* stats)
{
    const gfx::Rect rect(0, 0, 10, 10);
    ResourceUpdate upload = ResourceUpdate::Create(
        texture(), &m_bitmap, rect, rect, gfx::Vector2d());
    if (partialUpdate)
        queue->appendPartialUpload(upload);
    else
        queue->appendFullUpload(upload);

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

void FakeLayerUpdater::PrepareToUpdate(gfx::Rect contentRect, gfx::Size, float, float, gfx::Rect* resultingOpaqueRect, RenderingStats*)
{
    m_prepareCount++;
    m_lastUpdateRect = contentRect;
    if (!m_rectToInvalidate.IsEmpty()) {
        m_layer->InvalidateContentRect(m_rectToInvalidate);
        m_rectToInvalidate = gfx::Rect();
        m_layer = NULL;
    }
    *resultingOpaqueRect = m_opaquePaintRect;
}

void FakeLayerUpdater::setRectToInvalidate(const gfx::Rect& rect, FakeTiledLayer* layer)
{
    m_rectToInvalidate = rect;
    m_layer = layer;
}

scoped_ptr<LayerUpdater::Resource> FakeLayerUpdater::CreateResource(PrioritizedResourceManager* manager)
{
    return scoped_ptr<LayerUpdater::Resource>(new Resource(this, PrioritizedResource::Create(manager)));
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
    SetTileSize(tileSize());
    SetTextureFormat(GL_RGBA);
    SetBorderTexelOption(LayerTilingData::NO_BORDER_TEXELS);
    SetIsDrawable(true); // So that we don't get false positives if any of these tests expect to return false from DrawsContent() for other reasons.
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

void FakeTiledLayer::SetNeedsDisplayRect(const gfx::RectF& rect)
{
    m_lastNeedsDisplayRect = rect;
    TiledLayer::SetNeedsDisplayRect(rect);
}

void FakeTiledLayer::SetTexturePriorities(const PriorityCalculator& calculator)
{
    // Ensure there is always a target render surface available. If none has been
    // set (the layer is an orphan for the test), then just set a surface on itself.
    bool missingTargetRenderSurface = !render_target();

    if (missingTargetRenderSurface)
        CreateRenderSurface();

    TiledLayer::SetTexturePriorities(calculator);

    if (missingTargetRenderSurface) {
        ClearRenderSurface();
        draw_properties().render_target = 0;
    }
}

cc::PrioritizedResourceManager* FakeTiledLayer::ResourceManager() const
{
    return m_resourceManager;
}

void FakeTiledLayer::updateContentsScale(float ideal_contents_scale)
{
    CalculateContentsScale(
        ideal_contents_scale,
        false,  // animating_transform_to_screen
        &draw_properties().contents_scale_x,
        &draw_properties().contents_scale_y,
        &draw_properties().content_bounds);
}

cc::LayerUpdater* FakeTiledLayer::Updater() const
{
    return m_fakeUpdater.get();
}

void FakeTiledLayerWithScaledBounds::setContentBounds(const gfx::Size& contentBounds)
{
    m_forcedContentBounds = contentBounds;
    draw_properties().content_bounds = m_forcedContentBounds;
}

void FakeTiledLayerWithScaledBounds::CalculateContentsScale(
    float ideal_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* contentBounds)
{
    *contents_scale_x = static_cast<float>(m_forcedContentBounds.width()) / bounds().width();
    *contents_scale_y = static_cast<float>(m_forcedContentBounds.height()) / bounds().height();
    *contentBounds = m_forcedContentBounds;
}

}  // namespace cc
