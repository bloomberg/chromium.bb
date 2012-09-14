// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCIOSurfaceDrawQuad_h
#define CCIOSurfaceDrawQuad_h

#include "CCDrawQuad.h"
#include "IntSize.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

#pragma pack(push, 4)

class CCIOSurfaceDrawQuad : public CCDrawQuad {
public:
    enum Orientation {
      Flipped,
      Unflipped
    };

    static PassOwnPtr<CCIOSurfaceDrawQuad> create(const CCSharedQuadState*, const IntRect&, const IntSize& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    IntSize ioSurfaceSize() const { return m_ioSurfaceSize; }
    unsigned ioSurfaceTextureId() const { return m_ioSurfaceTextureId; }
    Orientation orientation() const { return m_orientation; }

    static const CCIOSurfaceDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCIOSurfaceDrawQuad(const CCSharedQuadState*, const IntRect&, const IntSize& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation);

    IntSize m_ioSurfaceSize;
    unsigned m_ioSurfaceTextureId;
    Orientation m_orientation;
};

#pragma pack(pop)

}

#endif
