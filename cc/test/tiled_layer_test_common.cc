// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/test/tiled_layer_test_common.h"

using namespace cc;

namespace WebKitTests {

FakeLayerTextureUpdater::Texture::Texture(FakeLayerTextureUpdater* layer, scoped_ptr<PrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture.Pass())
    , m_layer(layer)
{
    m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    m_bitmap.allocPixels();
}

FakeLayerTextureUpdater::Texture::~Texture()
{
}

void FakeLayerTextureUpdater::Texture::update(TextureUpdateQueue& queue, const IntRect&, const IntSize&, bool partialUpdate, RenderingStats&)
{
    const IntRect rect(0, 0, 10, 10);
    ResourceUpdate upload = ResourceUpdate::Create(
        texture(), &m_bitmap, rect, rect, IntSize());
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);

    m_layer->update();
}

FakeLayerTextureUpdater::FakeLayerTextureUpdater()
    : m_prepareCount(0)
    , m_updateCount(0)
{
}

FakeLayerTextureUpdater::~FakeLayerTextureUpdater()
{
}

void FakeLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize&, float, float, IntRect& resultingOpaqueRect, RenderingStats&)
{
    m_prepareCount++;
    m_lastUpdateRect = contentRect;
    if (!m_rectToInvalidate.isEmpty()) {
        m_layer->invalidateContentRect(m_rectToInvalidate);
        m_rectToInvalidate = IntRect();
        m_layer = NULL;
    }
    resultingOpaqueRect = m_opaquePaintRect;
}

void FakeLayerTextureUpdater::setRectToInvalidate(const IntRect& rect, FakeTiledLayer* layer)
{
    m_rectToInvalidate = rect;
    m_layer = layer;
}

scoped_ptr<LayerTextureUpdater::Texture> FakeLayerTextureUpdater::createTexture(PrioritizedTextureManager* manager)
{
    return scoped_ptr<LayerTextureUpdater::Texture>(new Texture(this, PrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat FakeLayerTextureUpdater::sampledTexelFormat(GLenum)
{
    return SampledTexelFormatRGBA;
}

FakeTiledLayerImpl::FakeTiledLayerImpl(int id)
    : TiledLayerImpl(id)
{
}

FakeTiledLayerImpl::~FakeTiledLayerImpl()
{
}

FakeTiledLayer::FakeTiledLayer(PrioritizedTextureManager* textureManager)
    : TiledLayer()
    , m_fakeTextureUpdater(make_scoped_refptr(new FakeLayerTextureUpdater))
    , m_textureManager(textureManager)
{
    setTileSize(tileSize());
    setTextureFormat(GL_RGBA);
    setBorderTexelOption(LayerTilingData::NoBorderTexels);
    setIsDrawable(true); // So that we don't get false positives if any of these tests expect to return false from drawsContent() for other reasons.
}

FakeTiledLayerWithScaledBounds::FakeTiledLayerWithScaledBounds(PrioritizedTextureManager* textureManager)
    : FakeTiledLayer(textureManager)
{
}

FakeTiledLayerWithScaledBounds::~FakeTiledLayerWithScaledBounds()
{
}

FakeTiledLayer::~FakeTiledLayer()
{
}

void FakeTiledLayer::setNeedsDisplayRect(const FloatRect& rect)
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
        setRenderTarget(0);
    }
}

cc::PrioritizedTextureManager* FakeTiledLayer::textureManager() const
{
    return m_textureManager;
}

cc::LayerTextureUpdater* FakeTiledLayer::textureUpdater() const
{
    return m_fakeTextureUpdater.get();
}

cc::IntSize FakeTiledLayerWithScaledBounds::contentBounds() const
{
    return m_forcedContentBounds;
}

} // namespace
