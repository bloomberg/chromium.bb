// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/texture_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<CCTextureDrawQuad> CCTextureDrawQuad::create(const CCSharedQuadState* sharedQuadState, const gfx::Rect& quadRect, unsigned resourceId, bool premultipliedAlpha, const gfx::RectF& uvRect, bool flipped)
{
    return make_scoped_ptr(new CCTextureDrawQuad(sharedQuadState, quadRect, resourceId, premultipliedAlpha, uvRect, flipped));
}

CCTextureDrawQuad::CCTextureDrawQuad(const CCSharedQuadState* sharedQuadState, const gfx::Rect& quadRect, unsigned resourceId, bool premultipliedAlpha, const gfx::RectF& uvRect, bool flipped)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::TextureContent, quadRect)
    , m_resourceId(resourceId)
    , m_premultipliedAlpha(premultipliedAlpha)
    , m_uvRect(uvRect)
    , m_flipped(flipped)
{
}

void CCTextureDrawQuad::setNeedsBlending()
{
    m_needsBlending = true;
}

const CCTextureDrawQuad* CCTextureDrawQuad::materialCast(const CCDrawQuad* quad)
{
    DCHECK(quad->material() == CCDrawQuad::TextureContent);
    return static_cast<const CCTextureDrawQuad*>(quad);
}

}
