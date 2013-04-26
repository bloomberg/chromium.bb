// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass.h"

#include "cc/base/math_util.h"
#include "cc/quads/checkerboard_draw_quad.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/transform.h"

using cc::TestRenderPass;

namespace cc {
namespace {

struct RenderPassSize {
  // If you add a new field to this class, make sure to add it to the
  // Copy() tests.
  RenderPass::Id id;
  QuadList quad_list;
  SharedQuadStateList shared_quad_state_list;
  gfx::Transform transform_to_root_target;
  gfx::Rect output_rect;
  gfx::RectF damage_rect;
  bool has_transparent_background;
  bool has_occlusion_from_outside_target_surface;
};

TEST(RenderPassTest, CopyShouldBeIdenticalExceptIdAndQuads) {
  RenderPass::Id id(3, 2);
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  bool has_transparent_background = true;
  bool has_occlusion_from_outside_target_surface = true;

  scoped_ptr<TestRenderPass> pass = TestRenderPass::Create();
  pass->SetAll(id,
               output_rect,
               damage_rect,
               transform_to_root,
               has_transparent_background,
               has_occlusion_from_outside_target_surface);

  // Stick a quad in the pass, this should not get copied.
  scoped_ptr<SharedQuadState> shared_state = SharedQuadState::Create();
  shared_state->SetAll(
      gfx::Transform(), gfx::Size(), gfx::Rect(), gfx::Rect(), false, 1);
  pass->AppendSharedQuadState(shared_state.Pass());

  scoped_ptr<CheckerboardDrawQuad> checkerboard_quad =
      CheckerboardDrawQuad::Create();
  checkerboard_quad->SetNew(
      pass->shared_quad_state_list.back(), gfx::Rect(), SkColor());
  pass->quad_list.push_back(checkerboard_quad.PassAs<DrawQuad>());

  RenderPass::Id new_id(63, 4);

  scoped_ptr<RenderPass> copy = pass->Copy(new_id);
  EXPECT_EQ(new_id, copy->id);
  EXPECT_RECT_EQ(pass->output_rect, copy->output_rect);
  EXPECT_EQ(pass->transform_to_root_target, copy->transform_to_root_target);
  EXPECT_RECT_EQ(pass->damage_rect, copy->damage_rect);
  EXPECT_EQ(pass->has_transparent_background, copy->has_transparent_background);
  EXPECT_EQ(pass->has_occlusion_from_outside_target_surface,
            copy->has_occlusion_from_outside_target_surface);
  EXPECT_EQ(0u, copy->quad_list.size());

  EXPECT_EQ(sizeof(RenderPassSize), sizeof(RenderPass));
}

}  // namespace
}  // namespace cc
