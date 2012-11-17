// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/shared_quad_state.h"

namespace cc {

scoped_ptr<SharedQuadState> SharedQuadState::create(const WebKit::WebTransformationMatrix& quadTransform, const gfx::Rect& visibleContentRect, const gfx::Rect& clippedRectInTarget, float opacity)
{
    return make_scoped_ptr(new SharedQuadState(quadTransform, visibleContentRect, clippedRectInTarget, opacity));
}

SharedQuadState::SharedQuadState(const WebKit::WebTransformationMatrix& quadTransform, const gfx::Rect& visibleContentRect, const gfx::Rect& clippedRectInTarget, float opacity)
    : id(-1)
    , quadTransform(quadTransform)
    , visibleContentRect(visibleContentRect)
    , clippedRectInTarget(clippedRectInTarget)
    , opacity(opacity)
{
}

scoped_ptr<SharedQuadState> SharedQuadState::copy() const
{
    scoped_ptr<SharedQuadState> copiedState(create(quadTransform, visibleContentRect, clippedRectInTarget, opacity));
    copiedState->id = id;
    return copiedState.Pass();
}

}  // namespace cc
