// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/render_pass.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/test/geometry_test_utils.h"
#include "components/viz/common/quads/copy_output_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

struct RenderPassSize {
  // If you add a new field to this class, make sure to add it to the
  // Copy() tests.
  uint64_t id;
  gfx::Rect output_rect;
  gfx::Rect damage_rect;
  gfx::Transform transform_to_root_target;
  FilterOperations filters;
  FilterOperations background_filters;
  gfx::ColorSpace color_space;
  bool has_transparent_background;
  std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_callbacks;
  QuadList quad_list;
  SharedQuadStateList shared_quad_state_list;
};

static void CompareRenderPassLists(const RenderPassList& expected_list,
                                   const RenderPassList& actual_list) {
  EXPECT_EQ(expected_list.size(), actual_list.size());
  for (size_t i = 0; i < actual_list.size(); ++i) {
    RenderPass* expected = expected_list[i].get();
    RenderPass* actual = actual_list[i].get();

    EXPECT_EQ(expected->id, actual->id);
    EXPECT_EQ(expected->output_rect, actual->output_rect);
    EXPECT_EQ(expected->transform_to_root_target,
              actual->transform_to_root_target);
    EXPECT_EQ(expected->damage_rect, actual->damage_rect);
    EXPECT_EQ(expected->filters, actual->filters);
    EXPECT_EQ(expected->background_filters, expected->background_filters);
    EXPECT_EQ(expected->has_transparent_background,
              actual->has_transparent_background);

    EXPECT_EQ(expected->shared_quad_state_list.size(),
              actual->shared_quad_state_list.size());
    EXPECT_EQ(expected->quad_list.size(), actual->quad_list.size());

    for (auto exp_iter = expected->quad_list.cbegin(),
              act_iter = actual->quad_list.cbegin();
         exp_iter != expected->quad_list.cend();
         ++exp_iter, ++act_iter) {
      EXPECT_EQ(exp_iter->rect.ToString(), act_iter->rect.ToString());
      EXPECT_EQ(exp_iter->shared_quad_state->quad_layer_rect.ToString(),
                act_iter->shared_quad_state->quad_layer_rect.ToString());
    }
  }
}

TEST(RenderPassTest, CopyShouldBeIdenticalExceptIdAndQuads) {
  RenderPassId render_pass_id = 3u;
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateOpacityFilter(0.5));
  FilterOperations background_filters;
  background_filters.Append(FilterOperation::CreateInvertFilter(1.0));
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  bool has_transparent_background = true;
  bool cache_render_pass = false;
  bool has_damage_from_contributing_content = false;

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetAll(render_pass_id, output_rect, damage_rect, transform_to_root,
               filters, background_filters, color_space,
               has_transparent_background, cache_render_pass,
               has_damage_from_contributing_content);
  pass->copy_requests.push_back(viz::CopyOutputRequest::CreateEmptyRequest());

  // Stick a quad in the pass, this should not get copied.
  viz::SharedQuadState* shared_state = pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(gfx::Transform(), gfx::Rect(), gfx::Rect(), gfx::Rect(),
                       false, 1, SkBlendMode::kSrcOver, 0);

  SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(pass->shared_quad_state_list.back(), gfx::Rect(),
                     gfx::Rect(), SkColor(), false);

  RenderPassId new_render_pass_id = 63u;

  std::unique_ptr<RenderPass> copy = pass->Copy(new_render_pass_id);
  EXPECT_EQ(new_render_pass_id, copy->id);
  EXPECT_EQ(pass->output_rect, copy->output_rect);
  EXPECT_EQ(pass->transform_to_root_target, copy->transform_to_root_target);
  EXPECT_EQ(pass->damage_rect, copy->damage_rect);
  EXPECT_EQ(pass->filters, copy->filters);
  EXPECT_EQ(pass->background_filters, copy->background_filters);
  EXPECT_EQ(pass->has_transparent_background, copy->has_transparent_background);
  EXPECT_EQ(0u, copy->quad_list.size());

  // The copy request should not be copied/duplicated.
  EXPECT_EQ(1u, pass->copy_requests.size());
  EXPECT_EQ(0u, copy->copy_requests.size());

  EXPECT_EQ(sizeof(RenderPassSize), sizeof(RenderPass));
}

TEST(RenderPassTest, CopyAllShouldBeIdentical) {
  RenderPassList pass_list;

  int id = 3;
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateOpacityFilter(0.5));
  FilterOperations background_filters;
  background_filters.Append(FilterOperation::CreateInvertFilter(1.0));
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateXYZD50();
  bool has_transparent_background = true;
  bool cache_render_pass = false;
  bool has_damage_from_contributing_content = false;

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetAll(id, output_rect, damage_rect, transform_to_root, filters,
               background_filters, color_space, has_transparent_background,
               cache_render_pass, has_damage_from_contributing_content);

  // Two quads using one shared state.
  viz::SharedQuadState* shared_state1 = pass->CreateAndAppendSharedQuadState();
  shared_state1->SetAll(gfx::Transform(), gfx::Rect(0, 0, 1, 1), gfx::Rect(),
                        gfx::Rect(), false, 1, SkBlendMode::kSrcOver, 0);

  SolidColorDrawQuad* color_quad1 =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad1->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 1, 1, 1), SkColor(),
                      false);

  SolidColorDrawQuad* color_quad2 =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad2->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(2, 2, 2, 2), gfx::Rect(2, 2, 2, 2), SkColor(),
                      false);

  // And two quads using another shared state.
  viz::SharedQuadState* shared_state2 = pass->CreateAndAppendSharedQuadState();
  shared_state2->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::Rect(), false, 1, SkBlendMode::kSrcOver, 0);

  SolidColorDrawQuad* color_quad3 =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad3->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(3, 3, 3, 3), gfx::Rect(3, 3, 3, 3), SkColor(),
                      false);

  SolidColorDrawQuad* color_quad4 =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad4->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(4, 4, 4, 4), gfx::Rect(4, 4, 4, 4), SkColor(),
                      false);

  // A second render pass with a quad.
  int contrib_id = 4;
  gfx::Rect contrib_output_rect(10, 15, 12, 17);
  gfx::Transform contrib_transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect contrib_damage_rect(11, 16, 10, 15);
  FilterOperations contrib_filters;
  contrib_filters.Append(FilterOperation::CreateSepiaFilter(0.5));
  FilterOperations contrib_background_filters;
  contrib_background_filters.Append(FilterOperation::CreateSaturateFilter(1));
  gfx::ColorSpace contrib_color_space = gfx::ColorSpace::CreateSCRGBLinear();
  bool contrib_has_transparent_background = true;
  bool contrib_cache_render_pass = false;
  bool contrib_has_damage_from_contributing_content = false;

  std::unique_ptr<RenderPass> contrib = RenderPass::Create();
  contrib->SetAll(contrib_id, contrib_output_rect, contrib_damage_rect,
                  contrib_transform_to_root, contrib_filters,
                  contrib_background_filters, contrib_color_space,
                  contrib_has_transparent_background, contrib_cache_render_pass,
                  contrib_has_damage_from_contributing_content);

  viz::SharedQuadState* contrib_shared_state =
      contrib->CreateAndAppendSharedQuadState();
  contrib_shared_state->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2),
                               gfx::Rect(), gfx::Rect(), false, 1,
                               SkBlendMode::kSrcOver, 0);

  SolidColorDrawQuad* contrib_quad =
      contrib->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  contrib_quad->SetNew(contrib->shared_quad_state_list.back(),
                       gfx::Rect(3, 3, 3, 3), gfx::Rect(3, 3, 3, 3), SkColor(),
                       false);

  // And a RenderPassDrawQuad for the contributing pass.
  std::unique_ptr<RenderPassDrawQuad> pass_quad =
      base::WrapUnique(new RenderPassDrawQuad);
  pass_quad->SetNew(pass->shared_quad_state_list.back(), contrib_output_rect,
                    contrib_output_rect, contrib_id, 0, gfx::RectF(),
                    gfx::Size(), gfx::Vector2dF(), gfx::PointF(), gfx::RectF());

  pass_list.push_back(std::move(pass));
  pass_list.push_back(std::move(contrib));

  // Make a copy with CopyAll().
  RenderPassList copy_list;
  RenderPass::CopyAll(pass_list, &copy_list);

  CompareRenderPassLists(pass_list, copy_list);
}

TEST(RenderPassTest, CopyAllWithCulledQuads) {
  RenderPassList pass_list;

  int id = 3;
  gfx::Rect output_rect(45, 22, 120, 13);
  gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  gfx::Rect damage_rect(56, 123, 19, 43);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateOpacityFilter(0.5));
  FilterOperations background_filters;
  background_filters.Append(FilterOperation::CreateInvertFilter(1.0));
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSCRGBLinear();
  bool has_transparent_background = true;
  bool cache_render_pass = false;
  bool has_damage_from_contributing_content = false;

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetAll(id, output_rect, damage_rect, transform_to_root, filters,
               background_filters, color_space, has_transparent_background,
               cache_render_pass, has_damage_from_contributing_content);

  // A shared state with a quad.
  viz::SharedQuadState* shared_state1 = pass->CreateAndAppendSharedQuadState();
  shared_state1->SetAll(gfx::Transform(), gfx::Rect(0, 0, 1, 1), gfx::Rect(),
                        gfx::Rect(), false, 1, SkBlendMode::kSrcOver, 0);

  SolidColorDrawQuad* color_quad1 =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad1->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 1, 1, 1), SkColor(),
                      false);

  // A shared state with no quads, they were culled.
  viz::SharedQuadState* shared_state2 = pass->CreateAndAppendSharedQuadState();
  shared_state2->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::Rect(), false, 1, SkBlendMode::kSrcOver, 0);

  // A second shared state with no quads.
  viz::SharedQuadState* shared_state3 = pass->CreateAndAppendSharedQuadState();
  shared_state3->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::Rect(), false, 1, SkBlendMode::kSrcOver, 0);

  // A last shared state with a quad again.
  viz::SharedQuadState* shared_state4 = pass->CreateAndAppendSharedQuadState();
  shared_state4->SetAll(gfx::Transform(), gfx::Rect(0, 0, 2, 2), gfx::Rect(),
                        gfx::Rect(), false, 1, SkBlendMode::kSrcOver, 0);

  SolidColorDrawQuad* color_quad2 =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad2->SetNew(pass->shared_quad_state_list.back(),
                      gfx::Rect(3, 3, 3, 3), gfx::Rect(3, 3, 3, 3), SkColor(),
                      false);

  pass_list.push_back(std::move(pass));

  // Make a copy with CopyAll().
  RenderPassList copy_list;
  RenderPass::CopyAll(pass_list, &copy_list);

  CompareRenderPassLists(pass_list, copy_list);
}

TEST(RenderPassTest, ReplacedQuadsShouldntMove) {
  std::unique_ptr<viz::SharedQuadState> quad_state =
      std::make_unique<viz::SharedQuadState>();
  QuadList quad_list;
  SolidColorDrawQuad* quad =
      quad_list.AllocateAndConstruct<SolidColorDrawQuad>();
  gfx::Rect quad_rect(1, 2, 3, 4);
  quad->SetNew(quad_state.get(), quad_rect, quad_rect, SkColor(), false);
  quad_list.ReplaceExistingQuadWithOpaqueTransparentSolidColor(
      quad_list.begin());
  EXPECT_EQ(quad_list.begin()->rect, quad_rect);
}

}  // namespace
}  // namespace cc
