// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_layer.h"

#include "cc/layer_tree_host.h"
#include "cc/texture_layer_client.h"
#include "cc/texture_layer_impl.h"
#include "cc/thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

static void runCallbackOnMainThread(const TextureMailbox::ReleaseCallback& callback, unsigned syncPoint)
{
    callback.Run(syncPoint);
}

static void postCallbackToMainThread(Thread *mainThread, const TextureMailbox::ReleaseCallback& callback, unsigned syncPoint)
{
    mainThread->postTask(base::Bind(&runCallbackOnMainThread, callback, syncPoint));
}

scoped_refptr<TextureLayer> TextureLayer::create(TextureLayerClient* client)
{
    return scoped_refptr<TextureLayer>(new TextureLayer(client, false));
}

scoped_refptr<TextureLayer> TextureLayer::createForMailbox()
{
    return scoped_refptr<TextureLayer>(new TextureLayer(0, true));
}

TextureLayer::TextureLayer(TextureLayerClient* client, bool usesMailbox)
    : Layer()
    , m_client(client)
    , m_usesMailbox(usesMailbox)
    , m_flipped(true)
    , m_uvTopLeft(0.f, 0.f)
    , m_uvBottomRight(1.f, 1.f)
    , m_premultipliedAlpha(true)
    , m_rateLimitContext(false)
    , m_contextLost(false)
    , m_textureId(0)
    , m_contentCommitted(false)
    , m_ownMailbox(false)
{
  m_vertexOpacity[0] = 1.0f;
  m_vertexOpacity[1] = 1.0f;
  m_vertexOpacity[2] = 1.0f;
  m_vertexOpacity[3] = 1.0f;
}

TextureLayer::~TextureLayer()
{
    if (layerTreeHost()) {
        if (m_textureId)
            layerTreeHost()->acquireLayerTextures();
        if (m_rateLimitContext && m_client)
            layerTreeHost()->stopRateLimiter(m_client->context());
    }
    if (m_ownMailbox)
        m_textureMailbox.RunReleaseCallback(m_textureMailbox.sync_point());
}

scoped_ptr<LayerImpl> TextureLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return TextureLayerImpl::create(treeImpl, m_layerId, m_usesMailbox).PassAs<LayerImpl>();
}

void TextureLayer::setFlipped(bool flipped)
{
    m_flipped = flipped;
    setNeedsCommit();
}

void TextureLayer::setUV(gfx::PointF topLeft, gfx::PointF bottomRight)
{
    m_uvTopLeft = topLeft;
    m_uvBottomRight = bottomRight;
    setNeedsCommit();
}

void TextureLayer::setVertexOpacity(float bottomLeft,
                                    float topLeft,
                                    float topRight,
                                    float bottomRight) {
  // Indexing according to the quad vertex generation:
  // 1--2
  // |  |
  // 0--3
  m_vertexOpacity[0] = bottomLeft;
  m_vertexOpacity[1] = topLeft;
  m_vertexOpacity[2] = topRight;
  m_vertexOpacity[3] = bottomRight;
  setNeedsCommit();
}

void TextureLayer::setPremultipliedAlpha(bool premultipliedAlpha)
{
    m_premultipliedAlpha = premultipliedAlpha;
    setNeedsCommit();
}

void TextureLayer::setRateLimitContext(bool rateLimit)
{
    if (!rateLimit && m_rateLimitContext && m_client && layerTreeHost())
        layerTreeHost()->stopRateLimiter(m_client->context());

    m_rateLimitContext = rateLimit;
}

void TextureLayer::setTextureId(unsigned id)
{
    DCHECK(!m_usesMailbox);
    if (m_textureId == id)
        return;
    if (m_textureId && layerTreeHost())
        layerTreeHost()->acquireLayerTextures();
    m_textureId = id;
    setNeedsCommit();
}

void TextureLayer::setTextureMailbox(const TextureMailbox& mailbox)
{
    DCHECK(m_usesMailbox);
    DCHECK(mailbox.IsEmpty() || !mailbox.Equals(m_textureMailbox));
    // If we never commited the mailbox, we need to release it here
    if (m_ownMailbox)
        m_textureMailbox.RunReleaseCallback(m_textureMailbox.sync_point());
    m_textureMailbox = mailbox;
    m_ownMailbox = true;

    setNeedsCommit();
}

void TextureLayer::willModifyTexture()
{
    if (layerTreeHost() && (drawsContent() || m_contentCommitted)) {
        layerTreeHost()->acquireLayerTextures();
        m_contentCommitted = false;
    }
}

void TextureLayer::setNeedsDisplayRect(const gfx::RectF& dirtyRect)
{
    Layer::setNeedsDisplayRect(dirtyRect);

    if (m_rateLimitContext && m_client && layerTreeHost() && drawsContent())
        layerTreeHost()->startRateLimiter(m_client->context());
}

void TextureLayer::setLayerTreeHost(LayerTreeHost* host)
{
    if (m_textureId && layerTreeHost() && host != layerTreeHost())
        layerTreeHost()->acquireLayerTextures();
    Layer::setLayerTreeHost(host);
}

bool TextureLayer::drawsContent() const
{
    return (m_client || m_textureId || !m_textureMailbox.IsEmpty()) && !m_contextLost && Layer::drawsContent();
}

void TextureLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker*, RenderingStats*)
{
    if (m_client) {
        m_textureId = m_client->prepareTexture(queue);
        m_contextLost = m_client->context()->getGraphicsResetStatusARB() != GL_NO_ERROR;
    }

    m_needsDisplay = false;
}

void TextureLayer::pushPropertiesTo(LayerImpl* layer)
{
    Layer::pushPropertiesTo(layer);

    TextureLayerImpl* textureLayer = static_cast<TextureLayerImpl*>(layer);
    textureLayer->setFlipped(m_flipped);
    textureLayer->setUVTopLeft(m_uvTopLeft);
    textureLayer->setUVBottomRight(m_uvBottomRight);
    textureLayer->setVertexOpacity(m_vertexOpacity);
    textureLayer->setPremultipliedAlpha(m_premultipliedAlpha);
    if (m_usesMailbox && m_ownMailbox) {
        Thread* mainThread = layerTreeHost()->proxy()->mainThread();
        TextureMailbox::ReleaseCallback callback;
        if (!m_textureMailbox.IsEmpty())
          callback = base::Bind(&postCallbackToMainThread, mainThread, m_textureMailbox.callback());
        textureLayer->setTextureMailbox(TextureMailbox(m_textureMailbox.name(), callback, m_textureMailbox.sync_point()));
        m_ownMailbox = false;
    } else {
        textureLayer->setTextureId(m_textureId);
    }
    m_contentCommitted = drawsContent();
}

bool TextureLayer::blocksPendingCommit() const
{
    // Double-buffered texture layers need to be blocked until they can be made
    // triple-buffered.  Single-buffered layers already prevent draws, so
    // can block too for simplicity.
    return drawsContent();
}

bool TextureLayer::canClipSelf() const
{
    return true;
}

}  // namespace cc
