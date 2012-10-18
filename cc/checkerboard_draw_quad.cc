// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCCheckerboardDrawQuad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<CCCheckerboardDrawQuad> CCCheckerboardDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, SkColor color)
{
    return make_scoped_ptr(new CCCheckerboardDrawQuad(sharedQuadState, quadRect, color));
}

CCCheckerboardDrawQuad::CCCheckerboardDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect, SkColor color)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::Checkerboard, quadRect)
    , m_color(color)
{
}

const CCCheckerboardDrawQuad* CCCheckerboardDrawQuad::materialCast(const CCDrawQuad* quad)
{
    DCHECK(quad->material() == CCDrawQuad::Checkerboard);
    return static_cast<const CCCheckerboardDrawQuad*>(quad);
}

}  // namespace cc
