// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_draw_quad.h"

#include "base/logging.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

scoped_ptr<TileDrawQuad> TileDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned resourceId, const gfx::RectF& texCoordRect, const gfx::Size& textureSize, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA)
{
    return make_scoped_ptr(new TileDrawQuad(sharedQuadState, quadRect, opaqueRect, resourceId, texCoordRect, textureSize, swizzleContents, leftEdgeAA, topEdgeAA, rightEdgeAA, bottomEdgeAA));
}

TileDrawQuad::TileDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned resourceId, const gfx::RectF& texCoordRect, const gfx::Size& textureSize, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA)
    : m_resourceId(resourceId)
    , m_texCoordRect(texCoordRect)
    , m_textureSize(textureSize)
    , m_swizzleContents(swizzleContents)
    , m_leftEdgeAA(leftEdgeAA)
    , m_topEdgeAA(topEdgeAA)
    , m_rightEdgeAA(rightEdgeAA)
    , m_bottomEdgeAA(bottomEdgeAA)
{
    gfx::Rect visibleRect = quadRect;
    bool needsBlending = isAntialiased();
    DrawQuad::SetAll(sharedQuadState, DrawQuad::TILED_CONTENT, quadRect, opaqueRect, visibleRect, needsBlending);
}

const TileDrawQuad* TileDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material == DrawQuad::TILED_CONTENT);
    return static_cast<const TileDrawQuad*>(quad);
}

}  // namespace cc
