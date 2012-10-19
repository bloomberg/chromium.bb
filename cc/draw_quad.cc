// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCDrawQuad.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCIOSurfaceDrawQuad.h"
#include "CCRenderPassDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "CCYUVVideoDrawQuad.h"
#include "IntRect.h"
#include "base/logging.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/stream_video_draw_quad.h"
#include "cc/texture_draw_quad.h"

namespace cc {

DrawQuad::DrawQuad(const SharedQuadState* sharedQuadState, Material material, const IntRect& quadRect)
    : m_sharedQuadState(sharedQuadState)
    , m_sharedQuadStateId(sharedQuadState->id)
    , m_material(material)
    , m_quadRect(quadRect)
    , m_quadVisibleRect(quadRect)
    , m_quadOpaque(true)
    , m_needsBlending(false)
{
    DCHECK(m_sharedQuadState);
    DCHECK(m_material != Invalid);
}

IntRect DrawQuad::opaqueRect() const
{
    if (opacity() != 1)
        return IntRect();
    if (m_sharedQuadState->opaque && m_quadOpaque)
        return m_quadRect;
    return m_opaqueRect;
}

void DrawQuad::setQuadVisibleRect(const IntRect& quadVisibleRect)
{
    IntRect intersection = quadVisibleRect;
    intersection.intersect(m_quadRect);
    m_quadVisibleRect = intersection;
}

unsigned DrawQuad::size() const
{
    switch (material()) {
    case Checkerboard:
        return sizeof(CheckerboardDrawQuad);
    case DebugBorder:
        return sizeof(DebugBorderDrawQuad);
    case IOSurfaceContent:
        return sizeof(IOSurfaceDrawQuad);
    case TextureContent:
        return sizeof(TextureDrawQuad);
    case SolidColor:
        return sizeof(SolidColorDrawQuad);
    case TiledContent:
        return sizeof(TileDrawQuad);
    case StreamVideoContent:
        return sizeof(StreamVideoDrawQuad);
    case RenderPass:
        return sizeof(RenderPassDrawQuad);
    case YUVVideoContent:
        return sizeof(YUVVideoDrawQuad);
    case Invalid:
        break;
    }

    CRASH();
    return sizeof(DrawQuad);
}

scoped_ptr<DrawQuad> DrawQuad::copy(const SharedQuadState* copiedSharedQuadState) const
{
    // RenderPass quads have their own copy() method.
    DCHECK(material() != RenderPass);

    unsigned bytes = size();
    DCHECK(bytes > 0);

    scoped_ptr<DrawQuad> copyQuad(reinterpret_cast<DrawQuad*>(new char[bytes]));
    memcpy(copyQuad.get(), this, bytes);
    copyQuad->setSharedQuadState(copiedSharedQuadState);

    return copyQuad.Pass();
}

void DrawQuad::setSharedQuadState(const SharedQuadState* sharedQuadState)
{
    m_sharedQuadState = sharedQuadState;
    m_sharedQuadStateId = sharedQuadState->id;
}

}
