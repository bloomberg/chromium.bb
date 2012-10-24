// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCIOSurfaceDrawQuad_h
#define CCIOSurfaceDrawQuad_h

#include "base/memory/scoped_ptr.h"
#include "cc/draw_quad.h"
#include "ui/gfx/size.h"

namespace cc {

#pragma pack(push, 4)

class IOSurfaceDrawQuad : public DrawQuad {
public:
    enum Orientation {
      Flipped,
      Unflipped
    };

    static scoped_ptr<IOSurfaceDrawQuad> create(const SharedQuadState*, const gfx::Rect&, const gfx::Size& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    gfx::Size ioSurfaceSize() const { return m_ioSurfaceSize; }
    unsigned ioSurfaceTextureId() const { return m_ioSurfaceTextureId; }
    Orientation orientation() const { return m_orientation; }

    static const IOSurfaceDrawQuad* materialCast(const DrawQuad*);
private:
    IOSurfaceDrawQuad(const SharedQuadState*, const gfx::Rect&, const gfx::Size& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    gfx::Size m_ioSurfaceSize;
    unsigned m_ioSurfaceTextureId;
    Orientation m_orientation;
};

#pragma pack(pop)

}

#endif
