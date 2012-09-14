// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCSharedQuadState.h"

#include "FloatQuad.h"

namespace cc {

PassOwnPtr<CCSharedQuadState> CCSharedQuadState::create(const WebKit::WebTransformationMatrix& quadTransform, const IntRect& visibleContentRect, const IntRect& clippedRectInTarget, float opacity, bool opaque)
{
    return adoptPtr(new CCSharedQuadState(quadTransform, visibleContentRect, clippedRectInTarget, opacity, opaque));
}

CCSharedQuadState::CCSharedQuadState(const WebKit::WebTransformationMatrix& quadTransform, const IntRect& visibleContentRect, const IntRect& clippedRectInTarget, float opacity, bool opaque)
    : id(-1)
    , quadTransform(quadTransform)
    , visibleContentRect(visibleContentRect)
    , clippedRectInTarget(clippedRectInTarget)
    , opacity(opacity)
    , opaque(opaque)
{
}

PassOwnPtr<CCSharedQuadState> CCSharedQuadState::copy() const
{
    OwnPtr<CCSharedQuadState> copiedState(create(quadTransform, visibleContentRect, clippedRectInTarget, opacity, opaque));
    copiedState->id = id;
    return copiedState.release();
}

}
