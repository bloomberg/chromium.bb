// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSolidColorDrawQuad_h
#define CCSolidColorDrawQuad_h

#include "CCDrawQuad.h"
#include "SkColor.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

#pragma pack(push, 4)

class CCSolidColorDrawQuad : public CCDrawQuad {
public:
    static PassOwnPtr<CCSolidColorDrawQuad> create(const CCSharedQuadState*, const IntRect&, SkColor);

    SkColor color() const { return m_color; };

    static const CCSolidColorDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCSolidColorDrawQuad(const CCSharedQuadState*, const IntRect&, SkColor);

    SkColor m_color;
};

#pragma pack(pop)

}

#endif
