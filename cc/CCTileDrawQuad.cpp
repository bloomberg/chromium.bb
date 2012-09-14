// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTileDrawQuad.h"

namespace cc {

PassOwnPtr<CCTileDrawQuad> CCTileDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const IntRect& opaqueRect, unsigned resourceId, const IntPoint& textureOffset, const IntSize& textureSize, GC3Dint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA)
{
    return adoptPtr(new CCTileDrawQuad(sharedQuadState, quadRect, opaqueRect, resourceId, textureOffset, textureSize, textureFilter, swizzleContents, leftEdgeAA, topEdgeAA, rightEdgeAA, bottomEdgeAA));
}

CCTileDrawQuad::CCTileDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, const IntRect& opaqueRect, unsigned resourceId, const IntPoint& textureOffset, const IntSize& textureSize, GC3Dint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::TiledContent, quadRect)
    , m_resourceId(resourceId)
    , m_textureOffset(textureOffset)
    , m_textureSize(textureSize)
    , m_textureFilter(textureFilter)
    , m_swizzleContents(swizzleContents)
    , m_leftEdgeAA(leftEdgeAA)
    , m_topEdgeAA(topEdgeAA)
    , m_rightEdgeAA(rightEdgeAA)
    , m_bottomEdgeAA(bottomEdgeAA)
{
    if (isAntialiased())
        m_needsBlending = true;
    m_opaqueRect = opaqueRect;
}

const CCTileDrawQuad* CCTileDrawQuad::materialCast(const CCDrawQuad* quad)
{
    ASSERT(quad->material() == CCDrawQuad::TiledContent);
    return static_cast<const CCTileDrawQuad*>(quad);
}

}
