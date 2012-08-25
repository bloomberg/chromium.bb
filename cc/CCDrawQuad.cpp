// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "CCDrawQuad.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCIOSurfaceDrawQuad.h"
#include "CCRenderPassDrawQuad.h"
#include "CCSolidColorDrawQuad.h"
#include "CCStreamVideoDrawQuad.h"
#include "CCTextureDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "CCYUVVideoDrawQuad.h"
#include "IntRect.h"

namespace WebCore {

CCDrawQuad::CCDrawQuad(const CCSharedQuadState* sharedQuadState, Material material, const IntRect& quadRect)
    : m_sharedQuadState(sharedQuadState)
    , m_sharedQuadStateId(sharedQuadState->id)
    , m_material(material)
    , m_quadRect(quadRect)
    , m_quadVisibleRect(quadRect)
    , m_quadOpaque(true)
    , m_needsBlending(false)
{
    ASSERT(m_sharedQuadState);
    ASSERT(m_material != Invalid);
}

IntRect CCDrawQuad::opaqueRect() const
{
    if (opacity() != 1)
        return IntRect();
    if (m_sharedQuadState->opaque && m_quadOpaque)
        return m_quadRect;
    return m_opaqueRect;
}

void CCDrawQuad::setQuadVisibleRect(const IntRect& quadVisibleRect)
{
    IntRect intersection = quadVisibleRect;
    intersection.intersect(m_quadRect);
    m_quadVisibleRect = intersection;
}

unsigned CCDrawQuad::size() const
{
    switch (material()) {
    case Checkerboard:
        return sizeof(CCCheckerboardDrawQuad);
    case DebugBorder:
        return sizeof(CCDebugBorderDrawQuad);
    case IOSurfaceContent:
        return sizeof(CCIOSurfaceDrawQuad);
    case TextureContent:
        return sizeof(CCTextureDrawQuad);
    case SolidColor:
        return sizeof(CCSolidColorDrawQuad);
    case TiledContent:
        return sizeof(CCTileDrawQuad);
    case StreamVideoContent:
        return sizeof(CCStreamVideoDrawQuad);
    case RenderPass:
        return sizeof(CCRenderPassDrawQuad);
    case YUVVideoContent:
        return sizeof(CCYUVVideoDrawQuad);
    case Invalid:
        break;
    }

    CRASH();
    return sizeof(CCDrawQuad);
}

void CCDrawQuad::setSharedQuadState(const CCSharedQuadState* sharedQuadState)
{
    m_sharedQuadState = sharedQuadState;
    m_sharedQuadStateId = sharedQuadState->id;
}

}
