// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCIOSurfaceDrawQuad_h
#define CCIOSurfaceDrawQuad_h

#include "CCDrawQuad.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/size.h"

namespace cc {

#pragma pack(push, 4)

class CCIOSurfaceDrawQuad : public CCDrawQuad {
public:
    enum Orientation {
      Flipped,
      Unflipped
    };

    static scoped_ptr<CCIOSurfaceDrawQuad> create(const CCSharedQuadState*, const gfx::Rect&, const gfx::Size& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    gfx::Size ioSurfaceSize() const { return m_ioSurfaceSize; }
    unsigned ioSurfaceTextureId() const { return m_ioSurfaceTextureId; }
    Orientation orientation() const { return m_orientation; }

    static const CCIOSurfaceDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCIOSurfaceDrawQuad(const CCSharedQuadState*, const gfx::Rect&, const gfx::Size& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    gfx::Size m_ioSurfaceSize;
    unsigned m_ioSurfaceTextureId;
    Orientation m_orientation;
};

#pragma pack(pop)

}

#endif
