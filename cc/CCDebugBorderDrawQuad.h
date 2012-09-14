// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDebugBorderDrawQuad_h
#define CCDebugBorderDrawQuad_h

#include "CCDrawQuad.h"
#include "SkColor.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

#pragma pack(push, 4)

class CCDebugBorderDrawQuad : public CCDrawQuad {
public:
    static PassOwnPtr<CCDebugBorderDrawQuad> create(const CCSharedQuadState*, const IntRect&, SkColor, int width);

    SkColor color() const { return m_color; };
    int width() const { return m_width; }

    static const CCDebugBorderDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCDebugBorderDrawQuad(const CCSharedQuadState*, const IntRect&, SkColor, int width);

    SkColor m_color;
    int m_width;
};

#pragma pack(pop)

}

#endif
