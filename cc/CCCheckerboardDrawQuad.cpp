// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCCheckerboardDrawQuad.h"

namespace cc {

PassOwnPtr<CCCheckerboardDrawQuad> CCCheckerboardDrawQuad::create(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect)
{
    return adoptPtr(new CCCheckerboardDrawQuad(sharedQuadState, quadRect));
}

CCCheckerboardDrawQuad::CCCheckerboardDrawQuad(const CCSharedQuadState* sharedQuadState, const IntRect& quadRect)
    : CCDrawQuad(sharedQuadState, CCDrawQuad::Checkerboard, quadRect)
{
}

const CCCheckerboardDrawQuad* CCCheckerboardDrawQuad::materialCast(const CCDrawQuad* quad)
{
    ASSERT(quad->material() == CCDrawQuad::Checkerboard);
    return static_cast<const CCCheckerboardDrawQuad*>(quad);
}


}
