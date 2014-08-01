// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/draw_quad.h"

#include "base/debug/trace_event_argument.h"
#include "base/logging.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "ui/gfx/quad_f.h"

namespace cc {

DrawQuad::DrawQuad()
    : material(INVALID),
      needs_blending(false),
      shared_quad_state() {
}

void DrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                      Material material,
                      const gfx::Rect& rect,
                      const gfx::Rect& opaque_rect,
                      const gfx::Rect& visible_rect,
                      bool needs_blending) {
  DCHECK(rect.Contains(visible_rect)) << "rect: " << rect.ToString()
                                      << " visible_rect: "
                                      << visible_rect.ToString();
  DCHECK(opaque_rect.IsEmpty() || rect.Contains(opaque_rect))
      << "rect: " << rect.ToString() << "opaque_rect "
      << opaque_rect.ToString();

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

void DrawQuad::AsValueInto(base::debug::TracedValue* value) const {
  value->SetInteger("material", material);
  TracedValue::SetIDRef(shared_quad_state, value, "shared_state");

  value->BeginArray("content_space_rect");
  MathUtil::AddToTracedValue(rect, value);
  value->EndArray();

  bool rect_is_clipped;
  gfx::QuadF rect_as_target_space_quad = MathUtil::MapQuad(
      shared_quad_state->content_to_target_transform,
      gfx::QuadF(rect),
      &rect_is_clipped);
  value->BeginArray("rect_as_target_space_quad");
  MathUtil::AddToTracedValue(rect_as_target_space_quad, value);
  value->EndArray();

  value->SetBoolean("rect_is_clipped", rect_is_clipped);

  value->BeginArray("content_space_opaque_rect");
  MathUtil::AddToTracedValue(opaque_rect, value);
  value->EndArray();

  bool opaque_rect_is_clipped;
  gfx::QuadF opaque_rect_as_target_space_quad = MathUtil::MapQuad(
      shared_quad_state->content_to_target_transform,
      gfx::QuadF(opaque_rect),
      &opaque_rect_is_clipped);
  value->BeginArray("opaque_rect_as_target_space_quad");
  MathUtil::AddToTracedValue(opaque_rect_as_target_space_quad, value);
  value->EndArray();

  value->SetBoolean("opaque_rect_is_clipped", opaque_rect_is_clipped);

  value->BeginArray("content_space_visible_rect");
  MathUtil::AddToTracedValue(visible_rect, value);
  value->EndArray();

  bool visible_rect_is_clipped;
  gfx::QuadF visible_rect_as_target_space_quad = MathUtil::MapQuad(
      shared_quad_state->content_to_target_transform,
      gfx::QuadF(visible_rect),
      &visible_rect_is_clipped);

  value->BeginArray("visible_rect_as_target_space_quad");
  MathUtil::AddToTracedValue(visible_rect_as_target_space_quad, value);
  value->EndArray();

  value->SetBoolean("visible_rect_is_clipped", visible_rect_is_clipped);

  value->SetBoolean("needs_blending", needs_blending);
  value->SetBoolean("should_draw_with_blending", ShouldDrawWithBlending());
  ExtendValue(value);
}

}  // namespace cc
