// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCDebugBorderDrawQuad.h"

namespace cc {

PassOwnPtr<CCDebugBorderDrawQuad> CCDebugBorderDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, SkColor color, int width)
{
    return adoptPtr(new CCDebugBorderDrawQuad(sharedQuadState, quadRect, color, width));
}

CCDebugBorderDrawQuad::CCDebugBorderDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, SkColor color, int width)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::DebugBorder, quadRect)
    , m_color(color)
    , m_width(width)
{
    m_quadOpaque = false;
    if (SkColorGetA(m_color) < 255)
        m_needsBlending = true;
}

const CCDebugBorderDrawQuad* CCDebugBorderDrawQuad::materialCast(const CCDrawQuad* quad)
{
    ASSERT(quad->material() == CCDrawQuad::DebugBorder);
    return static_cast<const CCDebugBorderDrawQuad*>(quad);
}

}
