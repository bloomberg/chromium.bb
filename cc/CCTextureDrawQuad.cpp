// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTextureDrawQuad.h"

namespace cc {

PassOwnPtr<CCTextureDrawQuad> CCTextureDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, unsigned resourceId, bool premultipliedAlpha, const FloatRect& uvRect, bool flipped)
{
    return adoptPtr(new CCTextureDrawQuad(sharedQuadState, quadRect, resourceId, premultipliedAlpha, uvRect, flipped));
}

CCTextureDrawQuad::CCTextureDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, unsigned resourceId, bool premultipliedAlpha, const FloatRect& uvRect, bool flipped)
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
    ASSERT(quad->material() == CCDrawQuad::TextureContent);
    return static_cast<const CCTextureDrawQuad*>(quad);
}

}
