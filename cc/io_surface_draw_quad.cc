// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/io_surface_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<IOSurfaceDrawQuad> IOSurfaceDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const gfx::Size& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation orientation)
{
    return make_scoped_ptr(new IOSurfaceDrawQuad(sharedQuadState, quadRect, ioSurfaceSize, ioSurfaceTextureId, orientation));
}

IOSurfaceDrawQuad::IOSurfaceDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const gfx::Size& ioSurfaceSize, unsigned ioSurfaceTextureId, Orientation orientation)
    : DrawQuad(sharedQuadState, DrawQuad::IO_SURFACE_CONTENT, quadRect)
    , m_ioSurfaceSize(ioSurfaceSize)
    , m_ioSurfaceTextureId(ioSurfaceTextureId)
    , m_orientation(orientation)
{
}

const IOSurfaceDrawQuad* IOSurfaceDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material() == DrawQuad::IO_SURFACE_CONTENT);
    return static_cast<const IOSurfaceDrawQuad*>(quad);
}

}  // namespace cc
