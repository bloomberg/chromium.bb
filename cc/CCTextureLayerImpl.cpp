// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCTextureLayerImpl.h"

#include "CCQuadSink.h"
#include "CCRenderer.h"
#include "CCTextureDrawQuad.h"
#include "TextStream.h"

namespace WebCore {

CCTextureLayerImpl::CCTextureLayerImpl(int id)
    : CCLayerImpl(id)
    , m_textureId(0)
    , m_externalTextureResource(0)
    , m_premultipliedAlpha(true)
    , m_flipped(true)
    , m_uvRect(0, 0, 1, 1)
{
}

CCTextureLayerImpl::~CCTextureLayerImpl()
{
}

void CCTextureLayerImpl::willDraw(CCResourceProvider* resourceProvider)
{
    if (!m_textureId)
        return;
    ASSERT(!m_externalTextureResource);
    m_externalTextureResource = resourceProvider->createResourceFromExternalTexture(m_textureId);
}

void CCTextureLayerImpl::appendQuads(CCQuadSink& quadSink, bool&)
{
    if (!m_externalTextureResource)
        return;

    CCSharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState);

    IntRect quadRect(IntPoint(), contentBounds());
    quadSink.append(CCTextureDrawQuad::create(sharedQuadState, quadRect, m_externalTextureResource, m_premultipliedAlpha, m_uvRect, m_flipped));
}

void CCTextureLayerImpl::didDraw(CCResourceProvider* resourceProvider)
{
    if (!m_externalTextureResource)
        return;
    // FIXME: the following assert will not be true when sending resources to a
    // parent compositor. A synchronization scheme (double-buffering or
    // pipelining of updates) for the client will need to exist to solve this.
    ASSERT(!resourceProvider->inUseByConsumer(m_externalTextureResource));
    resourceProvider->deleteResource(m_externalTextureResource);
    m_externalTextureResource = 0;
}

void CCTextureLayerImpl::dumpLayerProperties(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "texture layer texture id: " << m_textureId << " premultiplied: " << m_premultipliedAlpha << "\n";
    CCLayerImpl::dumpLayerProperties(ts, indent);
}

void CCTextureLayerImpl::didLoseContext()
{
    m_textureId = 0;
    m_externalTextureResource = 0;
}

}

#endif // USE(ACCELERATED_COMPOSITING)
