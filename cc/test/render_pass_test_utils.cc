// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_utils.h"

#include "cc/layers/append_quads_data.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/render_pass_test_common.h"
#include "ui/gfx/rect.h"

namespace cc {

TestRenderPass* AddRenderPass(RenderPassList& pass_list,
                              RenderPass::Id id,
                              gfx::Rect output_rect,
                              const gfx::Transform& root_transform) {
  scoped_ptr<TestRenderPass> pass(TestRenderPass::Create());
  pass->SetNew(id, output_rect, output_rect, root_transform);
  TestRenderPass* saved = pass.get();
  pass_list.push_back(pass.PassAs<RenderPass>());
  return saved;
}

SolidColorDrawQuad* AddQuad(TestRenderPass* pass,
                            gfx::Rect rect,
                            SkColor color) {
  MockQuadCuller quad_sink(&pass->quad_list, &pass->shared_quad_state_list);
  AppendQuadsData data(pass->id);
  SharedQuadState* shared_state =
      quad_sink.UseSharedQuadState(SharedQuadState::Create());
  shared_state->SetAll(gfx::Transform(), rect.size(), rect, rect, false, 1);
  scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
  quad->SetNew(shared_state, rect, color);
  SolidColorDrawQuad* quad_ptr = quad.get();
  quad_sink.Append(quad.PassAs<DrawQuad>(), &data);
  return quad_ptr;
}

SolidColorDrawQuad* AddClippedQuad(TestRenderPass* pass,
                                   gfx::Rect rect,
                                   SkColor color) {
  MockQuadCuller quad_sink(&pass->quad_list, &pass->shared_quad_state_list);
  AppendQuadsData data(pass->id);
  SharedQuadState* shared_state =
      quad_sink.UseSharedQuadState(SharedQuadState::Create());
  shared_state->SetAll(gfx::Transform(), rect.size(), rect, rect, true, 1);
  scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
  quad->SetNew(shared_state, rect, color);
  SolidColorDrawQuad* quad_ptr = quad.get();
  quad_sink.Append(quad.PassAs<DrawQuad>(), &data);
  return quad_ptr;
}

void AddRenderPassQuad(TestRenderPass* to_pass,
                       TestRenderPass* contributing_pass) {
  MockQuadCuller quad_sink(&to_pass->quad_list,
                           &to_pass->shared_quad_state_list);
  AppendQuadsData data(to_pass->id);
  gfx::Rect output_rect = contributing_pass->output_rect;
  SharedQuadState* shared_state =
      quad_sink.UseSharedQuadState(SharedQuadState::Create());
  shared_state->SetAll(
      gfx::Transform(), output_rect.size(), output_rect, output_rect, false, 1);
  scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
  quad->SetNew(shared_state, output_rect, contributing_pass->id, false, 0,
               output_rect, gfx::RectF(), WebKit::WebFilterOperations(),
               skia::RefPtr<SkImageFilter>(), WebKit::WebFilterOperations());
  quad_sink.Append(quad.PassAs<DrawQuad>(), &data);
}

}  // namespace cc
