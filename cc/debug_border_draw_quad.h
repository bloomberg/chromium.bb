// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDebugBorderDrawQuad_h
#define CCDebugBorderDrawQuad_h

#include "CCDrawQuad.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

#pragma pack(push, 4)

class DebugBorderDrawQuad : public DrawQuad {
public:
    static scoped_ptr<DebugBorderDrawQuad> create(const SharedQuadState*, const IntRect&, SkColor, int width);

    SkColor color() const { return m_color; };
    int width() const { return m_width; }

    static const DebugBorderDrawQuad* materialCast(const DrawQuad*);
private:
    DebugBorderDrawQuad(const SharedQuadState*, const IntRect&, SkColor, int width);

    SkColor m_color;
    int m_width;
};

#pragma pack(pop)

}

#endif
