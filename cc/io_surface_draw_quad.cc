// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/io_surface_draw_quad.h"

#include "base/logging.h"

namespace cc {

IOSurfaceDrawQuad::IOSurfaceDrawQuad()
    : io_surface_texture_id(0),
      orientation(FLIPPED) {
}

scoped_ptr<IOSurfaceDrawQuad> IOSurfaceDrawQuad::Create() {
  return make_scoped_ptr(new IOSurfaceDrawQuad);
}

void IOSurfaceDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                               gfx::Rect rect,
                               gfx::Rect opaque_rect,
                               gfx::Size io_surface_size,
                               unsigned io_surface_texture_id,
                               Orientation orientation) {
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::IO_SURFACE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending); 
  this->io_surface_size = io_surface_size;
  this->io_surface_texture_id = io_surface_texture_id;
  this->orientation = orientation;
}

void IOSurfaceDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                               gfx::Rect rect,
                               gfx::Rect opaque_rect,
                               gfx::Rect visible_rect,
                               bool needs_blending,
                               gfx::Size io_surface_size,
                               unsigned io_surface_texture_id,
                               Orientation orientation) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::IO_SURFACE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->io_surface_size = io_surface_size;
  this->io_surface_texture_id = io_surface_texture_id;
  this->orientation = orientation;
}

void IOSurfaceDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {
  // TODO(danakj): Convert to TextureDrawQuad?
  NOTIMPLEMENTED();
}

const IOSurfaceDrawQuad* IOSurfaceDrawQuad::MaterialCast(
    const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::IO_SURFACE_CONTENT);
  return static_cast<const IOSurfaceDrawQuad*>(quad);
}

}  // namespace cc
