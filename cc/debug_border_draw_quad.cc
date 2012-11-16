// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug_border_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<DebugBorderDrawQuad> DebugBorderDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, SkColor color, int width)
{
    return make_scoped_ptr(new DebugBorderDrawQuad(sharedQuadState, quadRect, color, width));
}

DebugBorderDrawQuad::DebugBorderDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, SkColor color, int width)
    : DrawQuad(sharedQuadState, DrawQuad::DEBUG_BORDER, quadRect)
    , m_color(color)
    , m_width(width)
{
    m_quadOpaque = false;
    if (SkColorGetA(m_color) < 255)
        m_needsBlending = true;
}

const DebugBorderDrawQuad* DebugBorderDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material() == DrawQuad::DEBUG_BORDER);
    return static_cast<const DebugBorderDrawQuad*>(quad);
}

}  // namespace cc
