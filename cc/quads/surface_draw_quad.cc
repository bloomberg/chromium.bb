// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/surface_draw_quad.h"

#include "base/logging.h"
#include "base/values.h"

namespace cc {

SurfaceDrawQuad::SurfaceDrawQuad() : surface_id(0) {}

scoped_ptr<SurfaceDrawQuad> SurfaceDrawQuad::Create() {
  return make_scoped_ptr(new SurfaceDrawQuad);
}

void SurfaceDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             gfx::Rect rect,
                             int surface_id) {
  gfx::Rect opaque_rect;
  gfx::Rect visible_rect = rect;
  bool needs_blending = false;
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SURFACE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->surface_id = surface_id;
}


void SurfaceDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             gfx::Rect rect,
                             gfx::Rect opaque_rect,
                             gfx::Rect visible_rect,
                             bool needs_blending,
                             int surface_id) {
  DrawQuad::SetAll(shared_quad_state, DrawQuad::SURFACE_CONTENT, rect,
                   opaque_rect, visible_rect, needs_blending);
  this->surface_id = surface_id;
}

void SurfaceDrawQuad::IterateResources(
    const ResourceIteratorCallback& callback) {}

const SurfaceDrawQuad* SurfaceDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK_EQ(quad->material, DrawQuad::SURFACE_CONTENT);
  return static_cast<const SurfaceDrawQuad*>(quad);
}

void SurfaceDrawQuad::ExtendValue(base::DictionaryValue* value) const {
  value->SetInteger("surface_id", surface_id);
}


}  // namespace cc
