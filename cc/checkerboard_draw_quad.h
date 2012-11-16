// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_CHECKERBOARD_DRAW_QUAD_H_
#define CC_CHECKERBOARD_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class CC_EXPORT CheckerboardDrawQuad : public DrawQuad {
public:
    static scoped_ptr<CheckerboardDrawQuad> create(const SharedQuadState*, const gfx::Rect&, SkColor);

    SkColor color() const { return m_color; };

    static const CheckerboardDrawQuad* materialCast(const DrawQuad*);
private:
    CheckerboardDrawQuad(const SharedQuadState*, const gfx::Rect&, SkColor);

    SkColor m_color;
};

}

#endif  // CC_CHECKERBOARD_DRAW_QUAD_H_
