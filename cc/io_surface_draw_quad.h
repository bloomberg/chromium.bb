// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCIOSurfaceDrawQuad_h
#define CCIOSurfaceDrawQuad_h

#include "CCDrawQuad.h"
#include "IntSize.h"
#include "base/memory/scoped_ptr.h"

namespace cc {

#pragma pack(push, 4)

class IOSurfaceDrawQuad : public DrawQuad {
public:
    enum Orientation {
      Flipped,
      Unflipped
    };

    static scoped_ptr<IOSurfaceDrawQuad> create(const SharedQuadState*, const IntRect&, const IntSize& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    IntSize ioSurfaceSize() const { return m_ioSurfaceSize; }
    unsigned ioSurfaceTextureId() const { return m_ioSurfaceTextureId; }
    Orientation orientation() const { return m_orientation; }

    static const IOSurfaceDrawQuad* materialCast(const DrawQuad*);
private:
    IOSurfaceDrawQuad(const SharedQuadState*, const IntRect&, const IntSize& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    IntSize m_ioSurfaceSize;
    unsigned m_ioSurfaceTextureId;
    Orientation m_orientation;
};

#pragma pack(pop)

}

#endif
