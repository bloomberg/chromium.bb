// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/solid_color_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<SolidColorDrawQuad> SolidColorDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, SkColor color)
{
    return make_scoped_ptr(new SolidColorDrawQuad(sharedQuadState, quadRect, color));
}

SolidColorDrawQuad::SolidColorDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, SkColor color)
    : m_color(color)
{
    gfx::Rect opaqueRect = SkColorGetA(m_color) == 255 ? quadRect : gfx::Rect();
    gfx::Rect visibleRect = quadRect;
    bool needsBlending = false;
    DrawQuad::SetAll(sharedQuadState, DrawQuad::SOLID_COLOR, quadRect, opaqueRect, visibleRect, needsBlending);
}

const SolidColorDrawQuad* SolidColorDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material == DrawQuad::SOLID_COLOR);
    return static_cast<const SolidColorDrawQuad*>(quad);
}

}  // namespacec cc
