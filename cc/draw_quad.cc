// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  return new T(*T::MaterialCast(other));
}

}

namespace cc {

DrawQuad::DrawQuad()
    : shared_quad_state(),
      material(INVALID),
      needs_blending(false) {
}

void DrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                      Material material,
                      gfx::Rect rect,
                      gfx::Rect opaque_rect,
                      gfx::Rect visible_rect,
                      bool needs_blending) {
  this->material = material;
  this->rect = rect;
  this->opaque_rect = opaque_rect;
  this->visible_rect = visible_rect;
  this->needs_blending = needs_blending;
  this->shared_quad_state = shared_quad_state;

  DCHECK(shared_quad_state);
  DCHECK(material != INVALID);
}

DrawQuad::~DrawQuad() {
}

scoped_ptr<DrawQuad> DrawQuad::Copy(
    const SharedQuadState* copied_shared_quad_state) const
{
  scoped_ptr<DrawQuad> copy_quad;
  switch (material) {
    case CHECKERBOARD:
      copy_quad.reset(TypedCopy<CheckerboardDrawQuad>(this));
      break;
    case DEBUG_BORDER:
      copy_quad.reset(TypedCopy<DebugBorderDrawQuad>(this));
      break;
    case IO_SURFACE_CONTENT:
      copy_quad.reset(TypedCopy<IOSurfaceDrawQuad>(this));
      break;
    case TEXTURE_CONTENT:
      copy_quad.reset(TypedCopy<TextureDrawQuad>(this));
      break;
    case SOLID_COLOR:
      copy_quad.reset(TypedCopy<SolidColorDrawQuad>(this));
      break;
    case TILED_CONTENT:
      copy_quad.reset(TypedCopy<TileDrawQuad>(this));
      break;
    case STREAM_VIDEO_CONTENT:
      copy_quad.reset(TypedCopy<StreamVideoDrawQuad>(this));
      break;
    case YUV_VIDEO_CONTENT:
      copy_quad.reset(TypedCopy<YUVVideoDrawQuad>(this));
      break;
    case RENDER_PASS:  // RenderPass quads have their own copy() method.
    case INVALID:
      LOG(FATAL) << "Invalid DrawQuad material " << material;
      break;
  }
  copy_quad->shared_quad_state = copied_shared_quad_state;
  return copy_quad.Pass();
}

void DrawQuad::AppendResources(ResourceProvider::ResourceIdArray* resources) {}

}  // namespace cc
