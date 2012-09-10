// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCIOSurfaceLayerImpl.h"

#include "base/stringprintf.h"
#include "CCGraphicsContext.h"
#include "CCIOSurfaceDrawQuad.h"
#include "CCLayerTreeHostImpl.h"
#include "CCQuadSink.h"
#include "CCRendererGL.h" // For the GLC() macro.
#include "Extensions3D.h"
#include <public/WebGraphicsContext3D.h>

namespace WebCore {

CCIOSurfaceLayerImpl::CCIOSurfaceLayerImpl(int id)
    : CCLayerImpl(id)
    , m_ioSurfaceId(0)
    , m_ioSurfaceChanged(false)
    , m_ioSurfaceTextureId(0)
{
}

CCIOSurfaceLayerImpl::~CCIOSurfaceLayerImpl()
{
    if (!m_ioSurfaceTextureId)
        return;

    CCGraphicsContext* context = layerTreeHostImpl()->context();
    // FIXME: Implement this path for software compositing.
    WebKit::WebGraphicsContext3D* context3d = context->context3D();
    if (context3d)
        context3d->deleteTexture(m_ioSurfaceTextureId);
}

void CCIOSurfaceLayerImpl::willDraw(CCResourceProvider* resourceProvider)
{
    CCLayerImpl::willDraw(resourceProvider);

    if (m_ioSurfaceChanged) {
        WebKit::WebGraphicsContext3D* context3d = resourceProvider->graphicsContext3D();
        if (!context3d) {
            // FIXME: Implement this path for software compositing.
            return;
        }

        // FIXME: Do this in a way that we can track memory usage.
        if (!m_ioSurfaceTextureId)
            m_ioSurfaceTextureId = context3d->createTexture();

        GLC(context3d, context3d->activeTexture(GraphicsContext3D::TEXTURE0));
        GLC(context3d, context3d->bindTexture(Extensions3D::TEXTURE_RECTANGLE_ARB, m_ioSurfaceTextureId));
        GLC(context3d, context3d->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
        GLC(context3d, context3d->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));
        GLC(context3d, context3d->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE));
        GLC(context3d, context3d->texParameteri(Extensions3D::TEXTURE_RECTANGLE_ARB, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE));
        context3d->texImageIOSurface2DCHROMIUM(Extensions3D::TEXTURE_RECTANGLE_ARB,
                                               m_ioSurfaceSize.width(),
                                               m_ioSurfaceSize.height(),
                                               m_ioSurfaceId,
                                               0);
        // Do not check for error conditions. texImageIOSurface2DCHROMIUM is supposed to hold on to
        // the last good IOSurface if the new one is already closed. This is only a possibility
        // during live resizing of plugins. However, it seems that this is not sufficient to
        // completely guard against garbage being drawn. If this is found to be a significant issue,
        // it may be necessary to explicitly tell the embedder when to free the surfaces it has
        // allocated.
        m_ioSurfaceChanged = false;
    }
}

void CCIOSurfaceLayerImpl::appendQuads(CCQuadSink& quadSink, CCAppendQuadsData& appendQuadsData)
{
    CCSharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    IntRect quadRect(IntPoint(), contentBounds());
    quadSink.append(CCIOSurfaceDrawQuad::create(sharedQuadState, quadRect, m_ioSurfaceSize, m_ioSurfaceTextureId, CCIOSurfaceDrawQuad::Flipped), appendQuadsData);
}

void CCIOSurfaceLayerImpl::dumpLayerProperties(std::string* str, int indent) const
{
    str->append(indentString(indent));
    base::StringAppendF(str, "iosurface id: %u texture id: %u\n", m_ioSurfaceId, m_ioSurfaceTextureId);
    CCLayerImpl::dumpLayerProperties(str, indent);
}

void CCIOSurfaceLayerImpl::didLoseContext()
{
    // We don't have a valid texture ID in the new context; however,
    // the IOSurface is still valid.
    m_ioSurfaceTextureId = 0;
    m_ioSurfaceChanged = true;
}

void CCIOSurfaceLayerImpl::setIOSurfaceProperties(unsigned ioSurfaceId, const IntSize& size)
{
    if (m_ioSurfaceId != ioSurfaceId)
        m_ioSurfaceChanged = true;

    m_ioSurfaceId = ioSurfaceId;
    m_ioSurfaceSize = size;
}
}

#endif // USE(ACCELERATED_COMPOSITING)
