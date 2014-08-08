// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/render_pass_test_utils.h"

#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/test/render_pass_test_common.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/rect.h"

namespace cc {

TestRenderPass* AddRenderPass(RenderPassList* pass_list,
                              RenderPass::Id id,
                              const gfx::Rect& output_rect,
                              const gfx::Transform& root_transform) {
  scoped_ptr<TestRenderPass> pass(TestRenderPass::Create());
  pass->SetNew(id, output_rect, output_rect, root_transform);
  TestRenderPass* saved = pass.get();
  pass_list->push_back(pass.PassAs<RenderPass>());
  return saved;
}

SolidColorDrawQuad* AddQuad(TestRenderPass* pass,
                            const gfx::Rect& rect,
                            SkColor color) {
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(),
                       rect.size(),
                       rect,
                       rect,
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode,
                       0);
  SolidColorDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

SolidColorDrawQuad* AddClippedQuad(TestRenderPass* pass,
                                   const gfx::Rect& rect,
                                   SkColor color) {
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(),
                       rect.size(),
                       rect,
                       rect,
                       true,
                       1,
                       SkXfermode::kSrcOver_Mode,
                       0);
  SolidColorDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

SolidColorDrawQuad* AddTransformedQuad(TestRenderPass* pass,
                                       const gfx::Rect& rect,
                                       SkColor color,
                                       const gfx::Transform& transform) {
  SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform,
                       rect.size(),
                       rect,
                       rect,
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode,
                       0);
  SolidColorDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_state, rect, rect, color, false);
  return quad;
}

void AddRenderPassQuad(TestRenderPass* to_pass,
                       TestRenderPass* contributing_pass) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  SharedQuadState* shared_state = to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(),
                       output_rect.size(),
                       output_rect,
                       output_rect,
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode,
                       0);
  RenderPassDrawQuad* quad =
      to_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(shared_state,
               output_rect,
               output_rect,
               contributing_pass->id,
               false,
               0,
               output_rect,
               gfx::RectF(),
               FilterOperations(),
               gfx::Vector2dF(),
               FilterOperations());
}

void AddRenderPassQuad(TestRenderPass* to_pass,
                       TestRenderPass* contributing_pass,
                       ResourceProvider::ResourceId mask_resource_id,
                       const FilterOperations& filters,
                       gfx::Transform transform) {
  gfx::Rect output_rect = contributing_pass->output_rect;
  SharedQuadState* shared_state = to_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform,
                       output_rect.size(),
                       output_rect,
                       output_rect,
                       false,
                       1,
                       SkXfermode::kSrcOver_Mode,
                       0);
  RenderPassDrawQuad* quad =
      to_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(shared_state,
               output_rect,
               output_rect,
               contributing_pass->id,
               false,
               mask_resource_id,
               output_rect,
               gfx::RectF(),
               filters,
               gfx::Vector2dF(),
               FilterOperations());
}

}  // namespace cc
