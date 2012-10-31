// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/draw_quad.h"

#include "base/logging.h"
#include "cc/checkerboard_draw_quad.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/io_surface_draw_quad.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/stream_video_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "cc/yuv_video_draw_quad.h"

namespace {

template<typename T> T* TypedCopy(const cc::DrawQuad* other) {
    return new T(*T::materialCast(other));
}

}

namespace cc {

DrawQuad::DrawQuad(const SharedQuadState* sharedQuadState, Material material, const gfx::Rect& quadRect)
    : m_sharedQuadState(sharedQuadState)
    , m_sharedQuadStateId(sharedQuadState->id)
    , m_material(material)
    , m_quadRect(quadRect)
    , m_quadVisibleRect(quadRect)
    , m_quadOpaque(true)
    , m_needsBlending(false)
{
    DCHECK(m_sharedQuadState);
    DCHECK(m_material != Invalid);
}

gfx::Rect DrawQuad::opaqueRect() const
{
    if (opacity() != 1)
        return gfx::Rect();
    if (m_sharedQuadState->opaque && m_quadOpaque)
        return m_quadRect;
    return m_opaqueRect;
}

void DrawQuad::setQuadVisibleRect(gfx::Rect quadVisibleRect)
{
    m_quadVisibleRect = gfx::IntersectRects(quadVisibleRect, m_quadRect);
}

scoped_ptr<DrawQuad> DrawQuad::copy(const SharedQuadState* copiedSharedQuadState) const
{
    scoped_ptr<DrawQuad> copyQuad;
    switch (material()) {
    case Checkerboard:
        copyQuad.reset(TypedCopy<CheckerboardDrawQuad>(this));
        break;
    case DebugBorder:
        copyQuad.reset(TypedCopy<DebugBorderDrawQuad>(this));
        break;
    case IOSurfaceContent:
        copyQuad.reset(TypedCopy<IOSurfaceDrawQuad>(this));
        break;
    case TextureContent:
        copyQuad.reset(TypedCopy<TextureDrawQuad>(this));
        break;
    case SolidColor:
        copyQuad.reset(TypedCopy<SolidColorDrawQuad>(this));
        break;
    case TiledContent:
        copyQuad.reset(TypedCopy<TileDrawQuad>(this));
        break;
    case StreamVideoContent:
        copyQuad.reset(TypedCopy<StreamVideoDrawQuad>(this));
        break;
    case YUVVideoContent:
        copyQuad.reset(TypedCopy<YUVVideoDrawQuad>(this));
        break;
    case RenderPass:  // RenderPass quads have their own copy() method.
    case Invalid:
        CRASH();
        break;
    }
    copyQuad->setSharedQuadState(copiedSharedQuadState);
    return copyQuad.Pass();
}

void DrawQuad::setSharedQuadState(const SharedQuadState* sharedQuadState)
{
    m_sharedQuadState = sharedQuadState;
    m_sharedQuadStateId = sharedQuadState->id;
}

}
