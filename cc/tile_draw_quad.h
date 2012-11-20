// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_DRAW_QUAD_H_
#define CC_TILE_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT TileDrawQuad : public DrawQuad {
public:
    static scoped_ptr<TileDrawQuad> create(const SharedQuadState*, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned resourceId, const gfx::RectF& texCoordRect, const gfx::Size& textureSize, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA);

    unsigned resourceId() const { return m_resourceId; }
    gfx::RectF texCoordRect() const { return m_texCoordRect; }
    gfx::Size textureSize() const { return m_textureSize; }
    bool swizzleContents() const { return m_swizzleContents; }

    // TODO(danakj): Stick the data used to compute these things in the quad
    // instead so the parent compositor can decide to use AA on its own.
    bool leftEdgeAA() const { return m_leftEdgeAA; }
    bool topEdgeAA() const { return m_topEdgeAA; }
    bool rightEdgeAA() const { return m_rightEdgeAA; }
    bool bottomEdgeAA() const { return m_bottomEdgeAA; }

    bool isAntialiased() const { return leftEdgeAA() || topEdgeAA() || rightEdgeAA() || bottomEdgeAA(); }

    static const TileDrawQuad* materialCast(const DrawQuad*);
private:
    TileDrawQuad(const SharedQuadState*, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned resourceId, const gfx::RectF& texCoordRect, const gfx::Size& textureSize, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA);

    unsigned m_resourceId;
    gfx::RectF m_texCoordRect;
    gfx::Size m_textureSize;
    bool m_swizzleContents;
    bool m_leftEdgeAA;
    bool m_topEdgeAA;
    bool m_rightEdgeAA;
    bool m_bottomEdgeAA;
};

}

#endif  // CC_TILE_DRAW_QUAD_H_
