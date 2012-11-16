// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_BORDER_DRAW_QUAD_H_
#define CC_DEBUG_BORDER_DRAW_QUAD_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class CC_EXPORT DebugBorderDrawQuad : public DrawQuad {
public:
    static scoped_ptr<DebugBorderDrawQuad> create(const SharedQuadState*, const gfx::Rect&, SkColor, int width);

    SkColor color() const { return m_color; };
    int width() const { return m_width; }

    static const DebugBorderDrawQuad* materialCast(const DrawQuad*);
private:
    DebugBorderDrawQuad(const SharedQuadState*, const gfx::Rect&, SkColor, int width);

    SkColor m_color;
    int m_width;
};

}

#endif  // CC_DEBUG_BORDER_DRAW_QUAD_H_
