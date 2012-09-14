// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "TextureLayerChromium.h"

#include "CCLayerTreeHost.h"
#include "CCTextureLayerImpl.h"
#include "TextureLayerChromiumClient.h"
#include <public/WebGraphicsContext3D.h>

namespace cc {

PassRefPtr<TextureLayerChromium> TextureLayerChromium::create(TextureLayerChromiumClient* client)
{
    return adoptRef(new TextureLayerChromium(client));
}

TextureLayerChromium::TextureLayerChromium(TextureLayerChromiumClient* client)
    : LayerChromium()
    , m_client(client)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
    , m_premultipliedAlpha(true)
    , m_rateLimitContext(false)
    , m_contextLost(false)
    , m_textureId(0)
{
}

TextureLayerChromium::~TextureLayerChromium()
{
    if (layerTreeHost()) {
        if (m_textureId)
            layerTreeHost()->acquireLayerTextures();
        if (m_rateLimitContext && m_client)
            layerTreeHost()->stopRateLimiter(m_client->context());
    }
}

PassOwnPtr<CCLayerImpl> TextureLayerChromium::createCCLayerImpl()
{
    return CCTextureLayerImpl::create(m_layerId);
}

void TextureLayerChromium::setFlipped(bool flipped)
{
    m_flipped = flipped;
    setNeedsCommit();
}

void TextureLayerChromium::setUVRect(const FloatRect& rect)
{
    m_uvRect = rect;
    setNeedsCommit();
}

void TextureLayerChromium::setPremultipliedAlpha(bool premultipliedAlpha)
{
    m_premultipliedAlpha = premultipliedAlpha;
    setNeedsCommit();
}

void TextureLayerChromium::setRateLimitContext(bool rateLimit)
{
    if (!rateLimit && m_rateLimitContext && m_client && layerTreeHost())
        layerTreeHost()->stopRateLimiter(m_client->context());

    m_rateLimitContext = rateLimit;
}

void TextureLayerChromium::setTextureId(unsigned id)
{
    if (m_textureId == id)
        return;
    if (m_textureId && layerTreeHost())
        layerTreeHost()->acquireLayerTextures();
    m_textureId = id;
    setNeedsCommit();
}

void TextureLayerChromium::willModifyTexture()
{
    if (layerTreeHost())
        layerTreeHost()->acquireLayerTextures();
}

void TextureLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    LayerChromium::setNeedsDisplayRect(dirtyRect);

    if (m_rateLimitContext && m_client && layerTreeHost())
        layerTreeHost()->startRateLimiter(m_client->context());
}

void TextureLayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    if (m_textureId && layerTreeHost() && host != layerTreeHost())
        layerTreeHost()->acquireLayerTextures();
    LayerChromium::setLayerTreeHost(host);
}

bool TextureLayerChromium::drawsContent() const
{
    return (m_client || m_textureId) && !m_contextLost && LayerChromium::drawsContent();
}

void TextureLayerChromium::update(CCTextureUpdateQueue& queue, const CCOcclusionTracker*, CCRenderingStats&)
{
    if (m_client) {
        m_textureId = m_client->prepareTexture(queue);
        m_contextLost = m_client->context()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR;
    }

    m_needsDisplay = false;
}

void TextureLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    LayerChromium::pushPropertiesTo(layer);

    CCTextureLayerImpl* textureLayer = static_cast<CCTextureLayerImpl*>(layer);
    textureLayer->setFlipped(m_flipped);
    textureLayer->setUVRect(m_uvRect);
    textureLayer->setPremultipliedAlpha(m_premultipliedAlpha);
    textureLayer->setTextureId(m_textureId);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
