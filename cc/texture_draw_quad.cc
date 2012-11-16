// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<TextureDrawQuad> TextureDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, unsigned resourceId, bool premultipliedAlpha, const gfx::RectF& uvRect, bool flipped)
{
    return make_scoped_ptr(new TextureDrawQuad(sharedQuadState, quadRect, resourceId, premultipliedAlpha, uvRect, flipped));
}

TextureDrawQuad::TextureDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, unsigned resourceId, bool premultipliedAlpha, const gfx::RectF& uvRect, bool flipped)
    : DrawQuad(sharedQuadState, DrawQuad::TEXTURE_CONTENT, quadRect)
    , m_resourceId(resourceId)
    , m_premultipliedAlpha(premultipliedAlpha)
    , m_uvRect(uvRect)
    , m_flipped(flipped)
{
}

void TextureDrawQuad::setNeedsBlending()
{
    m_needsBlending = true;
}

const TextureDrawQuad* TextureDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material() == DrawQuad::TEXTURE_CONTENT);
    return static_cast<const TextureDrawQuad*>(quad);
}

}  // namespace cc
