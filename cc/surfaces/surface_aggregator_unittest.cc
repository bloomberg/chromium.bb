// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_aggregator_test_helpers.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/render_pass_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
namespace {
const int kInvalidSurfaceId = -1;

class SurfaceAggregatorTest : public testing::Test {
 public:
  SurfaceAggregatorTest() : aggregator_(&manager_) {}

 protected:
  SurfaceManager manager_;
  SurfaceAggregator aggregator_;
};

TEST_F(SurfaceAggregatorTest, InvalidSurfaceId) {
  scoped_ptr<CompositorFrame> frame = aggregator_.Aggregate(kInvalidSurfaceId);
  EXPECT_FALSE(frame);
}

TEST_F(SurfaceAggregatorTest, ValidSurfaceNoFrame) {
  Surface one(&manager_, NULL, gfx::Size(5, 5));
  scoped_ptr<CompositorFrame> frame = aggregator_.Aggregate(one.surface_id());
  EXPECT_FALSE(frame);
}

class SurfaceAggregatorValidSurfaceTest : public SurfaceAggregatorTest {
 public:
  SurfaceAggregatorValidSurfaceTest()
      : root_surface_(&manager_, NULL, gfx::Size(5, 5)) {}

  void AggregateAndVerify(test::Pass* expected_passes,
                          size_t expected_pass_count) {
    scoped_ptr<CompositorFrame> aggregated_frame =
        aggregator_.Aggregate(root_surface_.surface_id());

    ASSERT_TRUE(aggregated_frame);
    ASSERT_TRUE(aggregated_frame->delegated_frame_data);

    DelegatedFrameData* frame_data =
        aggregated_frame->delegated_frame_data.get();

    TestPassesMatchExpectations(
        expected_passes, expected_pass_count, &frame_data->render_pass_list);
  }

 protected:
  Surface root_surface_;
};

// Tests that a very simple frame containing only two solid color quads makes it
// through the aggregator correctly.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleFrame) {
  test::Quad quads[] = {test::Quad::SolidColorQuad(SK_ColorRED),
                        test::Quad::SolidColorQuad(SK_ColorBLUE)};
  test::Pass passes[] = {test::Pass(quads, arraysize(quads))};

  SubmitFrame(passes, arraysize(passes), &root_surface_);

  AggregateAndVerify(passes, arraysize(passes));
}

TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSimpleFrame) {
  test::Quad quads[][2] = {{test::Quad::SolidColorQuad(SK_ColorWHITE),
                            test::Quad::SolidColorQuad(SK_ColorLTGRAY)},
                           {test::Quad::SolidColorQuad(SK_ColorGRAY),
                            test::Quad::SolidColorQuad(SK_ColorDKGRAY)}};
  test::Pass passes[] = {test::Pass(quads[0], arraysize(quads[0])),
                         test::Pass(quads[1], arraysize(quads[1]))};

  SubmitFrame(passes, arraysize(passes), &root_surface_);

  AggregateAndVerify(passes, arraysize(passes));
}

// This tests very simple embedding. root_surface has a frame containing a few
// solid color quads and a surface quad referencing embedded_surface.
// embedded_surface has a frame containing only a solid color quad. The solid
// color quad should be aggregated into the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleSurfaceReference) {
  gfx::Size surface_size(5, 5);

  Surface embedded_surface(&manager_, NULL, surface_size);

  test::Quad embedded_quads[] = {test::Quad::SolidColorQuad(SK_ColorGREEN)};
  test::Pass embedded_passes[] = {
      test::Pass(embedded_quads, arraysize(embedded_quads))};

  SubmitFrame(embedded_passes, arraysize(embedded_passes), &embedded_surface);

  test::Quad root_quads[] = {
      test::Quad::SolidColorQuad(SK_ColorWHITE),
      test::Quad::SurfaceQuad(embedded_surface.surface_id()),
      test::Quad::SolidColorQuad(SK_ColorBLACK)};
  test::Pass root_passes[] = {test::Pass(root_quads, arraysize(root_quads))};

  SubmitFrame(root_passes, arraysize(root_passes), &root_surface_);

  test::Quad expected_quads[] = {test::Quad::SolidColorQuad(SK_ColorWHITE),
                                 test::Quad::SolidColorQuad(SK_ColorGREEN),
                                 test::Quad::SolidColorQuad(SK_ColorBLACK)};
  test::Pass expected_passes[] = {
      test::Pass(expected_quads, arraysize(expected_quads))};
  AggregateAndVerify(expected_passes, arraysize(expected_passes));
}

// This tests referencing a surface that has multiple render passes.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSurfaceReference) {
  gfx::Size surface_size(5, 5);

  Surface embedded_surface(&manager_, NULL, surface_size);

  RenderPass::Id pass_ids[] = {RenderPass::Id(1, 1), RenderPass::Id(1, 2),
                               RenderPass::Id(1, 3)};

  test::Quad embedded_quads[][2] = {
      {test::Quad::SolidColorQuad(1), test::Quad::SolidColorQuad(2)},
      {test::Quad::SolidColorQuad(3), test::Quad::RenderPassQuad(pass_ids[0])},
      {test::Quad::SolidColorQuad(4), test::Quad::RenderPassQuad(pass_ids[1])}};
  test::Pass embedded_passes[] = {
      test::Pass(embedded_quads[0], arraysize(embedded_quads[0]), pass_ids[0]),
      test::Pass(embedded_quads[1], arraysize(embedded_quads[1]), pass_ids[1]),
      test::Pass(embedded_quads[2], arraysize(embedded_quads[2]), pass_ids[2])};

  SubmitFrame(embedded_passes, arraysize(embedded_passes), &embedded_surface);

  test::Quad root_quads[][2] = {
      {test::Quad::SolidColorQuad(5), test::Quad::SolidColorQuad(6)},
      {test::Quad::SurfaceQuad(embedded_surface.surface_id()),
       test::Quad::RenderPassQuad(pass_ids[0])},
      {test::Quad::SolidColorQuad(7), test::Quad::RenderPassQuad(pass_ids[1])}};
  test::Pass root_passes[] = {
      test::Pass(root_quads[0], arraysize(root_quads[0]), pass_ids[0]),
      test::Pass(root_quads[1], arraysize(root_quads[1]), pass_ids[1]),
      test::Pass(root_quads[2], arraysize(root_quads[2]), pass_ids[2])};

  SubmitFrame(root_passes, arraysize(root_passes), &root_surface_);

  scoped_ptr<CompositorFrame> aggregated_frame =
      aggregator_.Aggregate(root_surface_.surface_id());

  ASSERT_TRUE(aggregated_frame);
  ASSERT_TRUE(aggregated_frame->delegated_frame_data);

  DelegatedFrameData* frame_data = aggregated_frame->delegated_frame_data.get();

  const RenderPassList& aggregated_pass_list = frame_data->render_pass_list;

  ASSERT_EQ(5u, aggregated_pass_list.size());
  RenderPass::Id actual_pass_ids[] = {
      aggregated_pass_list[0]->id, aggregated_pass_list[1]->id,
      aggregated_pass_list[2]->id, aggregated_pass_list[3]->id,
      aggregated_pass_list[4]->id};
  for (size_t i = 0; i < 5; ++i) {
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(actual_pass_ids[i], actual_pass_ids[j]);
    }
  }

  {
    SCOPED_TRACE("First pass");
    // The first pass will just be the first pass from the root surfaces quad
    // with no render pass quads to remap.
    TestPassMatchesExpectations(root_passes[0], aggregated_pass_list[0]);
  }

  {
    SCOPED_TRACE("Second pass");
    // The next two passes will be from the embedded surface since we have to
    // draw those passes before they are referenced from the render pass draw
    // quad embedded into the root surface's second pass.
    // First, there's the first embedded pass which doesn't reference anything
    // else.
    TestPassMatchesExpectations(embedded_passes[0], aggregated_pass_list[1]);
  }

  {
    SCOPED_TRACE("Third pass");
    const QuadList& third_pass_quad_list = aggregated_pass_list[2]->quad_list;
    ASSERT_EQ(2u, third_pass_quad_list.size());
    TestQuadMatchesExpectations(embedded_quads[1][0],
                                third_pass_quad_list.at(0u));

    // This render pass pass quad will reference the first pass from the
    // embedded surface, which is the second pass in the aggregated frame.
    ASSERT_EQ(DrawQuad::RENDER_PASS, third_pass_quad_list.at(1u)->material);
    const RenderPassDrawQuad* third_pass_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(third_pass_quad_list.at(1u));
    EXPECT_EQ(actual_pass_ids[1],
              third_pass_render_pass_draw_quad->render_pass_id);
  }

  {
    SCOPED_TRACE("Fourth pass");
    // The fourth pass will have aggregated quads from the root surface's second
    // pass and the embedded surface's first pass.
    const QuadList& fourth_pass_quad_list = aggregated_pass_list[3]->quad_list;
    ASSERT_EQ(3u, fourth_pass_quad_list.size());

    // The first quad will be the yellow quad from the embedded surface's last
    // pass.
    TestQuadMatchesExpectations(embedded_quads[2][0],
                                fourth_pass_quad_list.at(0u));

    // The next quad will be a render pass quad referencing the second pass from
    // the embedded surface, which is the third pass in the aggregated frame.
    ASSERT_EQ(DrawQuad::RENDER_PASS, fourth_pass_quad_list.at(1u)->material);
    const RenderPassDrawQuad* fourth_pass_first_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(fourth_pass_quad_list.at(1u));
    EXPECT_EQ(actual_pass_ids[2],
              fourth_pass_first_render_pass_draw_quad->render_pass_id);

    // The last quad will be a render pass quad referencing the first pass from
    // the root surface, which is the first pass overall.
    ASSERT_EQ(DrawQuad::RENDER_PASS, fourth_pass_quad_list.at(2u)->material);
    const RenderPassDrawQuad* fourth_pass_second_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(fourth_pass_quad_list.at(2u));
    EXPECT_EQ(actual_pass_ids[0],
              fourth_pass_second_render_pass_draw_quad->render_pass_id);
  }

  {
    SCOPED_TRACE("Fifth pass");
    const QuadList& fifth_pass_quad_list = aggregated_pass_list[4]->quad_list;
    ASSERT_EQ(2u, fifth_pass_quad_list.size());

    TestQuadMatchesExpectations(root_quads[2][0], fifth_pass_quad_list.at(0));

    // The last quad in the last pass will reference the second pass from the
    // root surface, which after aggregating is the fourth pass in the overall
    // list.
    ASSERT_EQ(DrawQuad::RENDER_PASS, fifth_pass_quad_list.at(1u)->material);
    const RenderPassDrawQuad* fifth_pass_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(fifth_pass_quad_list.at(1u));
    EXPECT_EQ(actual_pass_ids[3],
              fifth_pass_render_pass_draw_quad->render_pass_id);
  }
}

// Tests an invalid surface reference in a frame. The surface quad should just
// be dropped.
TEST_F(SurfaceAggregatorValidSurfaceTest, InvalidSurfaceReference) {
  test::Quad quads[] = {test::Quad::SolidColorQuad(SK_ColorGREEN),
                        test::Quad::SurfaceQuad(kInvalidSurfaceId),
                        test::Quad::SolidColorQuad(SK_ColorBLUE)};
  test::Pass passes[] = {test::Pass(quads, arraysize(quads))};

  SubmitFrame(passes, arraysize(passes), &root_surface_);

  test::Quad expected_quads[] = {test::Quad::SolidColorQuad(SK_ColorGREEN),
                                 test::Quad::SolidColorQuad(SK_ColorBLUE)};
  test::Pass expected_passes[] = {
      test::Pass(expected_quads, arraysize(expected_quads))};
  AggregateAndVerify(expected_passes, arraysize(expected_passes));
}

// Tests a reference to a valid surface with no submitted frame. This quad
// should also just be dropped.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidSurfaceReferenceWithNoFrame) {
  Surface surface_with_no_frame(&manager_, NULL, gfx::Size(5, 5));
  test::Quad quads[] = {
      test::Quad::SolidColorQuad(SK_ColorGREEN),
      test::Quad::SurfaceQuad(surface_with_no_frame.surface_id()),
      test::Quad::SolidColorQuad(SK_ColorBLUE)};
  test::Pass passes[] = {test::Pass(quads, arraysize(quads))};

  SubmitFrame(passes, arraysize(passes), &root_surface_);

  test::Quad expected_quads[] = {test::Quad::SolidColorQuad(SK_ColorGREEN),
                                 test::Quad::SolidColorQuad(SK_ColorBLUE)};
  test::Pass expected_passes[] = {
      test::Pass(expected_quads, arraysize(expected_quads))};
  AggregateAndVerify(expected_passes, arraysize(expected_passes));
}

// Tests a surface quad referencing itself, generating a trivial cycle.
// The quad creating the cycle should be dropped from the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleCyclicalReference) {
  test::Quad quads[] = {test::Quad::SurfaceQuad(root_surface_.surface_id()),
                        test::Quad::SolidColorQuad(SK_ColorYELLOW)};
  test::Pass passes[] = {test::Pass(quads, arraysize(quads))};

  SubmitFrame(passes, arraysize(passes), &root_surface_);

  test::Quad expected_quads[] = {test::Quad::SolidColorQuad(SK_ColorYELLOW)};
  test::Pass expected_passes[] = {
      test::Pass(expected_quads, arraysize(expected_quads))};
  AggregateAndVerify(expected_passes, arraysize(expected_passes));
}

// Tests a more complex cycle with one intermediate surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, TwoSurfaceCyclicalReference) {
  gfx::Size surface_size(5, 5);

  Surface child_surface(&manager_, NULL, surface_size);

  test::Quad parent_quads[] = {
      test::Quad::SolidColorQuad(SK_ColorBLUE),
      test::Quad::SurfaceQuad(child_surface.surface_id()),
      test::Quad::SolidColorQuad(SK_ColorCYAN)};
  test::Pass parent_passes[] = {
      test::Pass(parent_quads, arraysize(parent_quads))};

  SubmitFrame(parent_passes, arraysize(parent_passes), &root_surface_);

  test::Quad child_quads[] = {
      test::Quad::SolidColorQuad(SK_ColorGREEN),
      test::Quad::SurfaceQuad(root_surface_.surface_id()),
      test::Quad::SolidColorQuad(SK_ColorMAGENTA)};
  test::Pass child_passes[] = {test::Pass(child_quads, arraysize(child_quads))};

  SubmitFrame(child_passes, arraysize(child_passes), &child_surface);

  // The child surface's reference to the root_surface_ will be dropped, so
  // we'll end up with:
  //   SK_ColorBLUE from the parent
  //   SK_ColorGREEN from the child
  //   SK_ColorMAGENTA from the child
  //   SK_ColorCYAN from the parent
  test::Quad expected_quads[] = {test::Quad::SolidColorQuad(SK_ColorBLUE),
                                 test::Quad::SolidColorQuad(SK_ColorGREEN),
                                 test::Quad::SolidColorQuad(SK_ColorMAGENTA),
                                 test::Quad::SolidColorQuad(SK_ColorCYAN)};
  test::Pass expected_passes[] = {
      test::Pass(expected_quads, arraysize(expected_quads))};
  AggregateAndVerify(expected_passes, arraysize(expected_passes));
}

// Tests that we map render pass IDs from different surfaces into a unified
// namespace and update RenderPassDrawQuad's id references to match.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassIdMapping) {
  gfx::Size surface_size(5, 5);

  Surface child_surface(&manager_, NULL, surface_size);

  RenderPass::Id child_pass_id[] = {RenderPass::Id(1, 1), RenderPass::Id(1, 2)};
  test::Quad child_quad[][1] = {{test::Quad::SolidColorQuad(SK_ColorGREEN)},
                                {test::Quad::RenderPassQuad(child_pass_id[0])}};
  test::Pass surface_passes[] = {
      test::Pass(child_quad[0], arraysize(child_quad[0]), child_pass_id[0]),
      test::Pass(child_quad[1], arraysize(child_quad[1]), child_pass_id[1])};

  SubmitFrame(surface_passes, arraysize(surface_passes), &child_surface);

  // Pass IDs from the parent surface may collide with ones from the child.
  RenderPass::Id parent_pass_id[] = {RenderPass::Id(2, 1),
                                     RenderPass::Id(1, 2)};
  test::Quad parent_quad[][1] = {
      {test::Quad::SurfaceQuad(child_surface.surface_id())},
      {test::Quad::RenderPassQuad(parent_pass_id[0])}};
  test::Pass parent_passes[] = {
      test::Pass(parent_quad[0], arraysize(parent_quad[0]), parent_pass_id[0]),
      test::Pass(parent_quad[1], arraysize(parent_quad[1]), parent_pass_id[1])};

  SubmitFrame(parent_passes, arraysize(parent_passes), &root_surface_);
  scoped_ptr<CompositorFrame> aggregated_frame =
      aggregator_.Aggregate(root_surface_.surface_id());

  ASSERT_TRUE(aggregated_frame);
  ASSERT_TRUE(aggregated_frame->delegated_frame_data);

  DelegatedFrameData* frame_data = aggregated_frame->delegated_frame_data.get();

  const RenderPassList& aggregated_pass_list = frame_data->render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());
  RenderPass::Id actual_pass_ids[] = {aggregated_pass_list[0]->id,
                                      aggregated_pass_list[1]->id,
                                      aggregated_pass_list[2]->id};
  // Make sure the aggregated frame's pass IDs are all unique.
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(actual_pass_ids[j], actual_pass_ids[i]) << "pass ids " << i
                                                        << " and " << j;
    }
  }

  // Make sure the render pass quads reference the remapped pass IDs.
  DrawQuad* render_pass_quads[] = {aggregated_pass_list[1]->quad_list[0],
                                   aggregated_pass_list[2]->quad_list[0]};
  ASSERT_EQ(render_pass_quads[0]->material, DrawQuad::RENDER_PASS);
  EXPECT_EQ(
      actual_pass_ids[0],
      RenderPassDrawQuad::MaterialCast(render_pass_quads[0])->render_pass_id);

  ASSERT_EQ(render_pass_quads[1]->material, DrawQuad::RENDER_PASS);
  EXPECT_EQ(
      actual_pass_ids[1],
      RenderPassDrawQuad::MaterialCast(render_pass_quads[1])->render_pass_id);
}

}  // namespace
}  // namespace cc
