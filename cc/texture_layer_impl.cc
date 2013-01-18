// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_layer_impl.h"

#include "base/stringprintf.h"
#include "cc/layer_tree_impl.h"
#include "cc/quad_sink.h"
#include "cc/renderer.h"
#include "cc/texture_draw_quad.h"

namespace cc {

TextureLayerImpl::TextureLayerImpl(LayerTreeImpl* treeImpl, int id, bool usesMailbox)
    : LayerImpl(treeImpl, id)
    , m_textureId(0)
    , m_externalTextureResource(0)
    , m_premultipliedAlpha(true)
    , m_flipped(true)
    , m_uvTopLeft(0.f, 0.f)
    , m_uvBottomRight(1.f, 1.f)
    , m_hasPendingMailbox(false)
    , m_usesMailbox(usesMailbox)
{
  m_vertexOpacity[0] = 1.0f;
  m_vertexOpacity[1] = 1.0f;
  m_vertexOpacity[2] = 1.0f;
  m_vertexOpacity[3] = 1.0f;
}

TextureLayerImpl::~TextureLayerImpl()
{
    if (m_externalTextureResource) {
        DCHECK(m_usesMailbox);
        ResourceProvider* provider = layerTreeImpl()->resource_provider();
        provider->deleteResource(m_externalTextureResource);
    }
    if (m_hasPendingMailbox && !m_pendingMailboxName.empty())
        m_pendingMailboxReleaseCallback.Run(0);
}

void TextureLayerImpl::setTextureMailbox(const std::string& mailboxName, const base::Callback<void(unsigned)>& releaseCallback)
{
    DCHECK(m_usesMailbox);
    // Same mailbox name was commited, nothing to do.
    if (m_pendingMailboxName.compare(mailboxName) == 0)
        return;
    // Two commits without a draw, ack the previous mailbox.
    if (m_hasPendingMailbox && !m_pendingMailboxReleaseCallback.is_null())
        m_pendingMailboxReleaseCallback.Run(0);

    m_pendingMailboxName = mailboxName;
    m_hasPendingMailbox = true;
    m_pendingMailboxReleaseCallback = releaseCallback;
}

void TextureLayerImpl::willDraw(ResourceProvider* resourceProvider)
{
    if (!m_usesMailbox) {
        if (!m_textureId)
            return;
        DCHECK(!m_externalTextureResource);
        m_externalTextureResource = resourceProvider->createResourceFromExternalTexture(m_textureId);
        return;
    }

    if (!m_hasPendingMailbox)
        return;

    if (m_externalTextureResource) {
        resourceProvider->deleteResource(m_externalTextureResource);
        m_externalTextureResource = 0;
    }
    if (!m_pendingMailboxName.empty())
        m_externalTextureResource = resourceProvider->createResourceFromTextureMailbox(m_pendingMailboxName, m_pendingMailboxReleaseCallback);
    m_hasPendingMailbox = false;
}

void TextureLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    if (!m_externalTextureResource)
        return;

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    gfx::Rect quadRect(gfx::Point(), contentBounds());
    gfx::Rect opaqueRect(contentsOpaque() ? quadRect : gfx::Rect());
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(sharedQuadState, quadRect, opaqueRect, m_externalTextureResource, m_premultipliedAlpha, m_uvTopLeft, m_uvBottomRight, m_vertexOpacity, m_flipped);

    // Perform explicit clipping on a quad to avoid setting a scissor later.
    if (sharedQuadState->is_clipped && quad->PerformClipping())
        sharedQuadState->is_clipped = false;
    if (!quad->rect.IsEmpty())
        quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
}

void TextureLayerImpl::didDraw(ResourceProvider* resourceProvider)
{
    if (m_usesMailbox)
        return;
    if (!m_externalTextureResource)
        return;
    // FIXME: the following assert will not be true when sending resources to a
    // parent compositor. A synchronization scheme (double-buffering or
    // pipelining of updates) for the client will need to exist to solve this.
    DCHECK(!resourceProvider->inUseByConsumer(m_externalTextureResource));
    resourceProvider->deleteResource(m_externalTextureResource);
    m_externalTextureResource = 0;
}

void TextureLayerImpl::dumpLayerProperties(std::string* str, int indent) const
{
    str->append(indentString(indent));
    base::StringAppendF(str, "texture layer texture id: %u premultiplied: %d\n", m_textureId, m_premultipliedAlpha);
    LayerImpl::dumpLayerProperties(str, indent);
}

void TextureLayerImpl::setVertexOpacity(const float vertexOpacity[4]) {
    m_vertexOpacity[0] = vertexOpacity[0];
    m_vertexOpacity[1] = vertexOpacity[1];
    m_vertexOpacity[2] = vertexOpacity[2];
    m_vertexOpacity[3] = vertexOpacity[3];
}

void TextureLayerImpl::didLoseOutputSurface()
{
    m_textureId = 0;
    m_externalTextureResource = 0;
}

const char* TextureLayerImpl::layerTypeAsString() const
{
    return "TextureLayer";
}

bool TextureLayerImpl::canClipSelf() const
{
    return true;
}

}  // namespace cc
