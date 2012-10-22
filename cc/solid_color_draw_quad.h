// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCSolidColorDrawQuad_h
#define CCSolidColorDrawQuad_h

#include "CCDrawQuad.h"
#include "third_party/skia/include/core/SkColor.h"
#include "base/memory/scoped_ptr.h"

namespace cc {

#pragma pack(push, 4)

class SolidColorDrawQuad : public DrawQuad {
public:
    static scoped_ptr<SolidColorDrawQuad> create(const SharedQuadState*, const gfx::Rect&, SkColor);

    SkColor color() const { return m_color; };

    static const SolidColorDrawQuad* materialCast(const DrawQuad*);
private:
    SolidColorDrawQuad(const SharedQuadState*, const gfx::Rect&, SkColor);

    SkColor m_color;
};

#pragma pack(pop)

}

#endif
