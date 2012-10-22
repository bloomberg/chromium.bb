// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTileDrawQuad_h
#define CCTileDrawQuad_h

#include "CCDrawQuad.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace cc {

#pragma pack(push, 4)

class TileDrawQuad : public DrawQuad {
public:
    static scoped_ptr<TileDrawQuad> create(const SharedQuadState*, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned resourceId, const gfx::Point& textureOffset, const gfx::Size& textureSize, GLint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA);

    unsigned resourceId() const { return m_resourceId; }
    gfx::Point textureOffset() const { return m_textureOffset; }
    gfx::Size textureSize() const { return m_textureSize; }
    GLint textureFilter() const { return m_textureFilter; }
    bool swizzleContents() const { return m_swizzleContents; }

    bool leftEdgeAA() const { return m_leftEdgeAA; }
    bool topEdgeAA() const { return m_topEdgeAA; }
    bool rightEdgeAA() const { return m_rightEdgeAA; }
    bool bottomEdgeAA() const { return m_bottomEdgeAA; }

    bool isAntialiased() const { return leftEdgeAA() || topEdgeAA() || rightEdgeAA() || bottomEdgeAA(); }

    static const TileDrawQuad* materialCast(const DrawQuad*);
private:
    TileDrawQuad(const SharedQuadState*, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned resourceId, const gfx::Point& textureOffset, const gfx::Size& textureSize, GLint textureFilter, bool swizzleContents, bool leftEdgeAA, bool topEdgeAA, bool rightEdgeAA, bool bottomEdgeAA);

    unsigned m_resourceId;
    gfx::Point m_textureOffset;
    gfx::Size m_textureSize;
    GLint m_textureFilter;
    bool m_swizzleContents;
    bool m_leftEdgeAA;
    bool m_topEdgeAA;
    bool m_rightEdgeAA;
    bool m_bottomEdgeAA;
};

#pragma pack(pop)

}

#endif
