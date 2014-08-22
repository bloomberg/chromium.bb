// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/surfaces_app/embedder.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "mojo/examples/surfaces_app/surfaces_util.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/interfaces/surfaces/surface_id.mojom.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"

namespace mojo {
namespace examples {

using cc::RenderPass;
using cc::RenderPassId;
using cc::SurfaceDrawQuad;
using cc::DrawQuad;
using cc::SolidColorDrawQuad;
using cc::DelegatedFrameData;
using cc::CompositorFrame;

Embedder::Embedder(Surface* surface) : surface_(surface) {
}

Embedder::~Embedder() {
}

void Embedder::ProduceFrame(cc::SurfaceId child_one,
                            cc::SurfaceId child_two,
                            const gfx::Size& child_size,
                            const gfx::Size& size,
                            int offset) {
  gfx::Rect rect(size);
  RenderPassId pass_id(1, 1);
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(pass_id, rect, rect, gfx::Transform());

  gfx::Transform one_transform;
  one_transform.Translate(10 + child_size.width() / 2,
                          50 + child_size.height() / 2);
  one_transform.Translate(0, offset);
  one_transform.Translate(-child_size.width() / 2, -child_size.height() / 2);
  CreateAndAppendSimpleSharedQuadState(pass.get(), one_transform, size);

  SurfaceDrawQuad* surface_one_quad =
      pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  gfx::Rect one_rect(child_size);
  surface_one_quad->SetNew(
      pass->shared_quad_state_list.back(), one_rect, one_rect, child_one);

  gfx::Transform two_transform;
  two_transform.Translate(10 + size.width() / 2 + child_size.width() / 2,
                          50 + child_size.height() / 2);
  two_transform.Translate(0, 200 - offset);
  two_transform.Translate(-child_size.width() / 2, -child_size.height() / 2);
  CreateAndAppendSimpleSharedQuadState(pass.get(), two_transform, size);

  SurfaceDrawQuad* surface_two_quad =
      pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  gfx::Rect two_rect(child_size);
  surface_two_quad->SetNew(
      pass->shared_quad_state_list.back(), two_rect, two_rect, child_two);

  CreateAndAppendSimpleSharedQuadState(pass.get(), gfx::Transform(), size);
  SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bool force_anti_aliasing_off = false;
  color_quad->SetNew(pass->shared_quad_state_list.back(),
                     rect,
                     rect,
                     SK_ColorYELLOW,
                     force_anti_aliasing_off);

  scoped_ptr<DelegatedFrameData> delegated_frame_data(new DelegatedFrameData);
  delegated_frame_data->render_pass_list.push_back(pass.Pass());

  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data = delegated_frame_data.Pass();

  surface_->SubmitFrame(mojo::SurfaceId::From(id_),
                        mojo::Frame::From(*frame));
}

}  // namespace examples
}  // namespace mojo
