// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/surfaces_app/surfaces_util.h"

#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"

namespace mojo {
namespace examples {

using cc::SharedQuadState;

void CreateAndAppendSimpleSharedQuadState(cc::RenderPass* render_pass,
                                          const gfx::Transform& transform,
                                          const gfx::Size& size) {
  const gfx::Size content_bounds = size;
  const gfx::Rect visible_content_rect = gfx::Rect(size);
  const gfx::Rect clip_rect = gfx::Rect(size);
  bool is_clipped = false;
  float opacity = 1.f;
  const SkXfermode::Mode blend_mode = SkXfermode::kSrcOver_Mode;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform,
                       content_bounds,
                       visible_content_rect,
                       clip_rect,
                       is_clipped,
                       opacity,
                       blend_mode,
                       0);
}

}  // namespace mojo
}  // namespace examples
