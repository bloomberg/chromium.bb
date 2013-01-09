// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_utils.h"

#include "cc/append_quads_data.h"
#include "cc/quad_sink.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/shared_quad_state.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/test/mock_quad_culler.h"
#include "cc/test/render_pass_test_common.h"
#include "ui/gfx/rect.h"

namespace cc {

TestRenderPass* addRenderPass(ScopedPtrVector<RenderPass>& passList,
                              RenderPass::Id id,
                              const gfx::Rect& outputRect,
                              const gfx::Transform& rootTransform) {
  scoped_ptr<TestRenderPass> pass(TestRenderPass::Create());
  pass->SetNew(id, outputRect, outputRect, rootTransform);
  TestRenderPass* saved = pass.get();
  passList.append(pass.PassAs<RenderPass>());
  return saved;
}

SolidColorDrawQuad* addQuad(TestRenderPass* pass,
                            const gfx::Rect& rect,
                            SkColor color) {
  MockQuadCuller quadSink(pass->quad_list, pass->shared_quad_state_list);
  AppendQuadsData data(pass->id);
  SharedQuadState* sharedState =
      quadSink.useSharedQuadState(SharedQuadState::Create());
  sharedState->SetAll(gfx::Transform(), rect, rect, rect, false, 1);
  scoped_ptr<SolidColorDrawQuad> quad = SolidColorDrawQuad::Create();
  quad->SetNew(sharedState, rect, color);
  SolidColorDrawQuad* quadPtr = quad.get();
  quadSink.append(quad.PassAs<DrawQuad>(), data);
  return quadPtr;
}

void addRenderPassQuad(TestRenderPass* toPass,
                       TestRenderPass* contributingPass) {
  MockQuadCuller quadSink(toPass->quad_list, toPass->shared_quad_state_list);
  AppendQuadsData data(toPass->id);
  gfx::Rect outputRect = contributingPass->output_rect;
  SharedQuadState* sharedState =
      quadSink.useSharedQuadState(SharedQuadState::Create());
  sharedState->SetAll(gfx::Transform(), outputRect, outputRect, outputRect,
                      false, 1);
  scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
  quad->SetNew(sharedState, outputRect, contributingPass->id, false, 0,
               outputRect, gfx::RectF(), WebKit::WebFilterOperations(),
               skia::RefPtr<SkImageFilter>(), WebKit::WebFilterOperations());
  quadSink.append(quad.PassAs<DrawQuad>(), data);
}

}  // namespace cc
