// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/checkerboard_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<CheckerboardDrawQuad> CheckerboardDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, SkColor color)
{
    return make_scoped_ptr(new CheckerboardDrawQuad(sharedQuadState, quadRect, color));
}

CheckerboardDrawQuad::CheckerboardDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, SkColor color)
    : m_color(color)
{
    gfx::Rect opaqueRect = SkColorGetA(m_color) == 255 ? quadRect : gfx::Rect();
    gfx::Rect visibleRect = quadRect;
    bool needsBlending = false;
    DrawQuad::SetAll(sharedQuadState, DrawQuad::CHECKERBOARD, quadRect, opaqueRect, visibleRect, needsBlending);
}

const CheckerboardDrawQuad* CheckerboardDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material == DrawQuad::CHECKERBOARD);
    return static_cast<const CheckerboardDrawQuad*>(quad);
}

}  // namespace cc
