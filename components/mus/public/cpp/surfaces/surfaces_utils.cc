// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/surfaces/surfaces_utils.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

using cc::mojom::RenderPass;
using cc::mojom::RenderPassPtr;

namespace mojo {

void ConfigureSharedQuadState(const gfx::Size& size, cc::SharedQuadState* out) {
  out->quad_layer_bounds = size;
  out->visible_quad_layer_rect = gfx::Rect(size);
  out->clip_rect = gfx::Rect(size);
  out->is_clipped = false;
  out->opacity = 1.f;
  out->blend_mode = SkXfermode::kSrc_Mode;
  out->sorting_context_id = 0;
}

RenderPassPtr CreateDefaultPass(int id, const gfx::Rect& rect) {
  RenderPassPtr pass = RenderPass::New();
  cc::RenderPassId render_pass_id;
  render_pass_id.layer_id = 1;
  render_pass_id.index = id;
  pass->id = render_pass_id;
  pass->output_rect = rect;
  pass->damage_rect = rect;
  pass->has_transparent_background = false;
  return pass;
}

}  // namespace mojo
