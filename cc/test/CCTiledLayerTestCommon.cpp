// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTiledLayerTestCommon.h"

using namespace cc;

namespace WebKitTests {

FakeLayerTextureUpdater::Texture::Texture(FakeLayerTextureUpdater* layer, PassOwnPtr<CCPrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture)
    , m_layer(layer)
{
}

FakeLayerTextureUpdater::Texture::~Texture()
{
}

void FakeLayerTextureUpdater::Texture::updateRect(CCResourceProvider* resourceProvider, const IntRect&, const IntSize&)
{
    texture()->acquireBackingTexture(resourceProvider);
    m_layer->updateRect();
}

void FakeLayerTextureUpdater::Texture::prepareRect(const IntRect&, cc::CCRenderingStats&)
{
    m_layer->prepareRect();
}

FakeLayerTextureUpdater::FakeLayerTextureUpdater()
    : m_prepareCount(0)
    , m_updateCount(0)
    , m_prepareRectCount(0)
{
}

FakeLayerTextureUpdater::~FakeLayerTextureUpdater()
{
}

void FakeLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize&, float, float, IntRect& resultingOpaqueRect, CCRenderingStats&)
{
    m_prepareCount++;
    m_lastUpdateRect = contentRect;
    if (!m_rectToInvalidate.isEmpty()) {
        m_layer->invalidateContentRect(m_rectToInvalidate);
        m_rectToInvalidate = IntRect();
        m_layer = 0;
    }
    resultingOpaqueRect = m_opaquePaintRect;
}

void FakeLayerTextureUpdater::setRectToInvalidate(const IntRect& rect, FakeTiledLayerChromium* layer)
{
    m_rectToInvalidate = rect;
    m_layer = layer;
}

PassOwnPtr<LayerTextureUpdater::Texture> FakeLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat FakeLayerTextureUpdater::sampledTexelFormat(GC3Denum)
{
    return SampledTexelFormatRGBA;
}

FakeCCTiledLayerImpl::FakeCCTiledLayerImpl(int id)
    : CCTiledLayerImpl(id)
{
}

FakeCCTiledLayerImpl::~FakeCCTiledLayerImpl()
{
}

FakeTiledLayerChromium::FakeTiledLayerChromium(CCPrioritizedTextureManager* textureManager)
    : TiledLayerChromium()
    , m_fakeTextureUpdater(adoptRef(new FakeLayerTextureUpdater))
    , m_textureManager(textureManager)
{
    setTileSize(tileSize());
    setTextureFormat(GraphicsContext3D::RGBA);
    setBorderTexelOption(CCLayerTilingData::NoBorderTexels);
    setIsDrawable(true); // So that we don't get false positives if any of these tests expect to return false from drawsContent() for other reasons.
}

FakeTiledLayerWithScaledBounds::FakeTiledLayerWithScaledBounds(CCPrioritizedTextureManager* textureManager)
    : FakeTiledLayerChromium(textureManager)
{
}

FakeTiledLayerChromium::~FakeTiledLayerChromium()
{
}

void FakeTiledLayerChromium::setNeedsDisplayRect(const FloatRect& rect)
{
    m_lastNeedsDisplayRect = rect;
    TiledLayerChromium::setNeedsDisplayRect(rect);
}

void FakeTiledLayerChromium::setTexturePriorities(const CCPriorityCalculator& calculator)
{
    // Ensure there is always a target render surface available. If none has been
    // set (the layer is an orphan for the test), then just set a surface on itself.
    bool missingTargetRenderSurface = !renderTarget();

    if (missingTargetRenderSurface)
        createRenderSurface();

    TiledLayerChromium::setTexturePriorities(calculator);

    if (missingTargetRenderSurface) {
        clearRenderSurface();
        setRenderTarget(0);
    }
}

cc::CCPrioritizedTextureManager* FakeTiledLayerChromium::textureManager() const
{
    return m_textureManager;
}

cc::LayerTextureUpdater* FakeTiledLayerChromium::textureUpdater() const
{
    return m_fakeTextureUpdater.get();
}

cc::IntSize FakeTiledLayerWithScaledBounds::contentBounds() const
{
    return m_forcedContentBounds;
}

bool FakeTextureUploader::isBusy()
{
    return false;
}

void FakeTextureUploader::uploadTexture(cc::CCResourceProvider* resourceProvider, Parameters upload)
{
    upload.texture->updateRect(resourceProvider, upload.sourceRect, upload.destOffset);
}

} // namespace
