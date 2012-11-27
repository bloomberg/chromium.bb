// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/render_surface.h"

#include "cc/layer.h"
#include "cc/math_util.h"
#include "ui/gfx/transform.h"

namespace cc {

RenderSurface::RenderSurface(Layer* owningLayer)
    : m_owningLayer(owningLayer)
    , m_drawOpacity(1)
    , m_drawOpacityIsAnimating(false)
    , m_targetSurfaceTransformsAreAnimating(false)
    , m_screenSpaceTransformsAreAnimating(false)
    , m_isClipped(false)
    , m_nearestAncestorThatMovesPixels(0)
{
}

RenderSurface::~RenderSurface()
{
}

gfx::RectF RenderSurface::drawableContentRect() const
{
    gfx::RectF drawableContentRect = MathUtil::mapClippedRect(m_drawTransform, m_contentRect);
    if (m_owningLayer->hasReplica())
        drawableContentRect.Union(MathUtil::mapClippedRect(m_replicaDrawTransform, m_contentRect));
    return drawableContentRect;
}

}  // namespace cc
