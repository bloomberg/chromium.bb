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

scoped_refptr<TextureLayer> TextureLayer::Create(TextureLayerClient* client)
{
    return scoped_refptr<TextureLayer>(new TextureLayer(client, false));
}

scoped_refptr<TextureLayer> TextureLayer::CreateForMailbox()
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
    if (layer_tree_host()) {
        if (m_textureId)
            layer_tree_host()->AcquireLayerTextures();
        if (m_rateLimitContext && m_client)
            layer_tree_host()->StopRateLimiter(m_client->context());
    }
    if (m_ownMailbox)
        m_textureMailbox.RunReleaseCallback(m_textureMailbox.sync_point());
}

scoped_ptr<LayerImpl> TextureLayer::CreateLayerImpl(LayerTreeImpl* treeImpl)
{
    return TextureLayerImpl::Create(treeImpl, id(), m_usesMailbox).PassAs<LayerImpl>();
}

void TextureLayer::setFlipped(bool flipped)
{
    m_flipped = flipped;
    SetNeedsCommit();
}

void TextureLayer::setUV(gfx::PointF topLeft, gfx::PointF bottomRight)
{
    m_uvTopLeft = topLeft;
    m_uvBottomRight = bottomRight;
    SetNeedsCommit();
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
  SetNeedsCommit();
}

void TextureLayer::setPremultipliedAlpha(bool premultipliedAlpha)
{
    m_premultipliedAlpha = premultipliedAlpha;
    SetNeedsCommit();
}

void TextureLayer::setRateLimitContext(bool rateLimit)
{
    if (!rateLimit && m_rateLimitContext && m_client && layer_tree_host())
        layer_tree_host()->StopRateLimiter(m_client->context());

    m_rateLimitContext = rateLimit;
}

void TextureLayer::setTextureId(unsigned id)
{
    DCHECK(!m_usesMailbox);
    if (m_textureId == id)
        return;
    if (m_textureId && layer_tree_host())
        layer_tree_host()->AcquireLayerTextures();
    m_textureId = id;
    SetNeedsCommit();
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

    SetNeedsCommit();
}

void TextureLayer::willModifyTexture()
{
    if (layer_tree_host() && (DrawsContent() || m_contentCommitted)) {
        layer_tree_host()->AcquireLayerTextures();
        m_contentCommitted = false;
    }
}

void TextureLayer::SetNeedsDisplayRect(const gfx::RectF& dirtyRect)
{
    Layer::SetNeedsDisplayRect(dirtyRect);

    if (m_rateLimitContext && m_client && layer_tree_host() && DrawsContent())
        layer_tree_host()->StartRateLimiter(m_client->context());
}

void TextureLayer::SetLayerTreeHost(LayerTreeHost* host)
{
    if (m_textureId && layer_tree_host() && host != layer_tree_host())
        layer_tree_host()->AcquireLayerTextures();
    Layer::SetLayerTreeHost(host);
}

bool TextureLayer::DrawsContent() const
{
    return (m_client || m_textureId || !m_textureMailbox.IsEmpty()) && !m_contextLost && Layer::DrawsContent();
}

void TextureLayer::Update(ResourceUpdateQueue* queue, const OcclusionTracker*, RenderingStats*)
{
    if (m_client) {
        m_textureId = m_client->prepareTexture(*queue);
        m_contextLost = m_client->context()->getGraphicsResetStatusARB() != GL_NO_ERROR;
    }

    needs_display_ = false;
}

void TextureLayer::PushPropertiesTo(LayerImpl* layer)
{
    Layer::PushPropertiesTo(layer);

    TextureLayerImpl* textureLayer = static_cast<TextureLayerImpl*>(layer);
    textureLayer->setFlipped(m_flipped);
    textureLayer->setUVTopLeft(m_uvTopLeft);
    textureLayer->setUVBottomRight(m_uvBottomRight);
    textureLayer->setVertexOpacity(m_vertexOpacity);
    textureLayer->setPremultipliedAlpha(m_premultipliedAlpha);
    if (m_usesMailbox && m_ownMailbox) {
        Thread* mainThread = layer_tree_host()->proxy()->MainThread();
        TextureMailbox::ReleaseCallback callback;
        if (!m_textureMailbox.IsEmpty())
          callback = base::Bind(&postCallbackToMainThread, mainThread, m_textureMailbox.callback());
        textureLayer->setTextureMailbox(TextureMailbox(m_textureMailbox.name(), callback, m_textureMailbox.sync_point()));
        m_ownMailbox = false;
    } else {
        textureLayer->setTextureId(m_textureId);
    }
    m_contentCommitted = DrawsContent();
}

bool TextureLayer::BlocksPendingCommit() const
{
    // Double-buffered texture layers need to be blocked until they can be made
    // triple-buffered.  Single-buffered layers already prevent draws, so
    // can block too for simplicity.
    return DrawsContent();
}

bool TextureLayer::CanClipSelf() const
{
    return true;
}

}  // namespace cc
