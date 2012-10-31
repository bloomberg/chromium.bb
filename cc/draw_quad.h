// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDrawQuad_h
#define CCDrawQuad_h

#include "cc/shared_quad_state.h"

namespace cc {

// WARNING! All XYZDrawQuad classes must remain PODs (plain old data).
// They are intended to be "serializable" by copying their raw bytes, so they
// must not contain any non-bit-copyable member variables!
//
// Furthermore, the class members need to be packed so they are aligned
// properly and don't have paddings/gaps, otherwise memory check tools
// like Valgrind will complain about uninitialized memory usage when
// transferring these classes over the wire.
#pragma pack(push, 4)

// DrawQuad is a bag of data used for drawing a quad. Because different
// materials need different bits of per-quad data to render, classes that derive
// from DrawQuad store additional data in their derived instance. The Material
// enum is used to "safely" downcast to the derived class.
class DrawQuad {
public:
    enum Material {
        Invalid,
        Checkerboard,
        DebugBorder,
        IOSurfaceContent,
        RenderPass,
        TextureContent,
        SolidColor,
        TiledContent,
        YUVVideoContent,
        StreamVideoContent,
    };

    gfx::Rect quadRect() const { return m_quadRect; }
    const WebKit::WebTransformationMatrix& quadTransform() const { return m_sharedQuadState->quadTransform; }
    gfx::Rect visibleContentRect() const { return m_sharedQuadState->visibleContentRect; }
    gfx::Rect clippedRectInTarget() const { return m_sharedQuadState->clippedRectInTarget; }
    float opacity() const { return m_sharedQuadState->opacity; }
    // For the purposes of blending, what part of the contents of this quad are opaque?
    gfx::Rect opaqueRect() const;
    bool needsBlending() const { return m_needsBlending || !opaqueRect().Contains(m_quadVisibleRect); }

    // Allows changing the rect that gets drawn to make it smaller. Parameter passed
    // in will be clipped to quadRect().
    void setQuadVisibleRect(gfx::Rect);
    gfx::Rect quadVisibleRect() const { return m_quadVisibleRect; }
    bool isDebugQuad() const { return m_material == DebugBorder; }

    Material material() const { return m_material; }

    scoped_ptr<DrawQuad> copy(const SharedQuadState* copiedSharedQuadState) const;

    const SharedQuadState* sharedQuadState() const { return m_sharedQuadState; }
    int sharedQuadStateId() const { return m_sharedQuadStateId; }
    void setSharedQuadState(const SharedQuadState*);

protected:
    DrawQuad(const SharedQuadState*, Material, const gfx::Rect&);

    // Stores state common to a large bundle of quads; kept separate for memory
    // efficiency. There is special treatment to reconstruct these pointers
    // during serialization.
    const SharedQuadState* m_sharedQuadState;
    int m_sharedQuadStateId;

    Material m_material;
    gfx::Rect m_quadRect;
    gfx::Rect m_quadVisibleRect;

    // By default, the shared quad state determines whether or not this quad is
    // opaque or needs blending. Derived classes can override with these
    // variables.
    bool m_quadOpaque;
    bool m_needsBlending;

    // Be default, this rect is empty. It is used when the shared quad state and above
    // variables determine that the quad is not fully opaque but may be partially opaque.
    gfx::Rect m_opaqueRect;
};

#pragma pack(pop)

}

#endif
