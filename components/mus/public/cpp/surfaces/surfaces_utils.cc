// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/surfaces/surfaces_utils.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

using mus::mojom::Pass;
using mus::mojom::PassPtr;

namespace mojo {

cc::SharedQuadState CreateDefaultSQS(const gfx::Size& size) {
  cc::SharedQuadState sqs;
  sqs.quad_layer_bounds = size;
  sqs.visible_quad_layer_rect = gfx::Rect(size);
  sqs.clip_rect = gfx::Rect(size);
  sqs.is_clipped = false;
  sqs.opacity = 1.f;
  sqs.blend_mode = SkXfermode::kSrc_Mode;
  sqs.sorting_context_id = 0;
  return sqs;
}

PassPtr CreateDefaultPass(int id, const gfx::Rect& rect) {
  PassPtr pass = Pass::New();
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
