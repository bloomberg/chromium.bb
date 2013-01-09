// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/io_surface_layer_impl.h"

#include "base/stringprintf.h"
#include "cc/gl_renderer.h" // For the GLC() macro.
#include "cc/io_surface_draw_quad.h"
#include "cc/layer_tree_impl.h"
#include "cc/output_surface.h"
#include "cc/quad_sink.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

namespace cc {

IOSurfaceLayerImpl::IOSurfaceLayerImpl(LayerTreeImpl* treeImpl, int id)
    : LayerImpl(treeImpl, id)
    , m_ioSurfaceId(0)
    , m_ioSurfaceChanged(false)
    , m_ioSurfaceTextureId(0)
{
}

IOSurfaceLayerImpl::~IOSurfaceLayerImpl()
{
    if (!m_ioSurfaceTextureId)
        return;

    OutputSurface* outputSurface = layerTreeImpl()->output_surface();
    // FIXME: Implement this path for software compositing.
    WebKit::WebGraphicsContext3D* context3d = outputSurface->Context3D();
    if (context3d)
        context3d->deleteTexture(m_ioSurfaceTextureId);
}

void IOSurfaceLayerImpl::willDraw(ResourceProvider* resourceProvider)
{
    LayerImpl::willDraw(resourceProvider);

    if (m_ioSurfaceChanged) {
        WebKit::WebGraphicsContext3D* context3d = resourceProvider->graphicsContext3D();
        if (!context3d) {
            // FIXME: Implement this path for software compositing.
            return;
        }

        // FIXME: Do this in a way that we can track memory usage.
        if (!m_ioSurfaceTextureId)
            m_ioSurfaceTextureId = context3d->createTexture();

        GLC(context3d, context3d->activeTexture(GL_TEXTURE0));
        GLC(context3d, context3d->bindTexture(GL_TEXTURE_RECTANGLE_ARB, m_ioSurfaceTextureId));
        GLC(context3d, context3d->texParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLC(context3d, context3d->texParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GLC(context3d, context3d->texParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLC(context3d, context3d->texParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        context3d->texImageIOSurface2DCHROMIUM(GL_TEXTURE_RECTANGLE_ARB,
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

void IOSurfaceLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    gfx::Rect quadRect(gfx::Point(), contentBounds());
    gfx::Rect opaqueRect(contentsOpaque() ? quadRect : gfx::Rect());
    scoped_ptr<IOSurfaceDrawQuad> quad = IOSurfaceDrawQuad::Create();
    quad->SetNew(sharedQuadState, quadRect, opaqueRect, m_ioSurfaceSize, m_ioSurfaceTextureId, IOSurfaceDrawQuad::FLIPPED);
    quadSink.append(quad.PassAs<DrawQuad>(), appendQuadsData);
}

void IOSurfaceLayerImpl::dumpLayerProperties(std::string* str, int indent) const
{
    str->append(indentString(indent));
    base::StringAppendF(str, "iosurface id: %u texture id: %u\n", m_ioSurfaceId, m_ioSurfaceTextureId);
    LayerImpl::dumpLayerProperties(str, indent);
}

void IOSurfaceLayerImpl::didLoseOutputSurface()
{
    // We don't have a valid texture ID in the new context; however,
    // the IOSurface is still valid.
    m_ioSurfaceTextureId = 0;
    m_ioSurfaceChanged = true;
}

void IOSurfaceLayerImpl::setIOSurfaceProperties(unsigned ioSurfaceId, const gfx::Size& size)
{
    if (m_ioSurfaceId != ioSurfaceId)
        m_ioSurfaceChanged = true;

    m_ioSurfaceId = ioSurfaceId;
    m_ioSurfaceSize = size;
}

const char* IOSurfaceLayerImpl::layerTypeAsString() const
{
    return "IOSurfaceLayer";
}

}  // namespace cc
