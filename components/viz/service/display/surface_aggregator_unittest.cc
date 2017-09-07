
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/display_resource_provider.h"
#include "cc/test/fake_compositor_frame_sink_support_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_surface_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {
namespace {

constexpr FrameSinkId kArbitraryRootFrameSinkId(1, 1);
constexpr FrameSinkId kArbitraryFrameSinkId1(2, 2);
constexpr FrameSinkId kArbitraryFrameSinkId2(3, 3);
constexpr FrameSinkId kArbitraryMiddleFrameSinkId(4, 4);
constexpr FrameSinkId kArbitraryReservedFrameSinkId(5, 5);
constexpr FrameSinkId kArbitraryFrameSinkId3(6, 6);
const base::UnguessableToken kArbitraryToken = base::UnguessableToken::Create();
constexpr bool kRootIsRoot = true;
constexpr bool kChildIsRoot = false;
constexpr bool kNeedsSyncPoints = false;

SurfaceId InvalidSurfaceId() {
  static SurfaceId invalid(FrameSinkId(),
                           LocalSurfaceId(0xdeadbeef, kArbitraryToken));
  return invalid;
}

gfx::Size SurfaceSize() {
  static gfx::Size size(100, 100);
  return size;
}

class SurfaceAggregatorTest : public testing::Test {
 public:
  explicit SurfaceAggregatorTest(bool use_damage_rect)
      : observer_(false),
        support_(CompositorFrameSinkSupport::Create(&fake_client_,
                                                    &manager_,
                                                    kArbitraryRootFrameSinkId,
                                                    kRootIsRoot,
                                                    kNeedsSyncPoints)),
        aggregator_(manager_.surface_manager(), NULL, use_damage_rect) {
    manager_.surface_manager()->AddObserver(&observer_);
  }

  SurfaceAggregatorTest() : SurfaceAggregatorTest(false) {}

  void TearDown() override {
    observer_.Reset();
    support_->EvictCurrentSurface();
    testing::Test::TearDown();
  }

  struct Quad {
    static Quad SolidColorQuad(SkColor color) {
      Quad quad;
      quad.material = DrawQuad::SOLID_COLOR;
      quad.color = color;
      return quad;
    }

    static Quad SurfaceQuad(const SurfaceId& primary_surface_id,
                            const SurfaceId& fallback_surface_id,
                            float opacity) {
      Quad quad;
      quad.material = DrawQuad::SURFACE_CONTENT;
      quad.opacity = opacity;
      quad.primary_surface_id = primary_surface_id;
      quad.fallback_surface_id = fallback_surface_id;
      return quad;
    }

    static Quad RenderPassQuad(int id) {
      Quad quad;
      quad.material = DrawQuad::RENDER_PASS;
      quad.render_pass_id = id;
      return quad;
    }

    DrawQuad::Material material;
    // Set when material==DrawQuad::SURFACE_CONTENT.
    SurfaceId primary_surface_id;
    SurfaceId fallback_surface_id;
    float opacity;
    // Set when material==DrawQuad::SOLID_COLOR.
    SkColor color;
    // Set when material==DrawQuad::RENDER_PASS.
    cc::RenderPassId render_pass_id;

   private:
    Quad() : material(DrawQuad::INVALID), opacity(1.f), color(SK_ColorWHITE) {}
  };

  struct Pass {
    Pass(Quad* quads, size_t quad_count, int id)
        : quads(quads), quad_count(quad_count), id(id) {}
    Pass(Quad* quads, size_t quad_count)
        : quads(quads), quad_count(quad_count) {}

    Quad* quads;
    size_t quad_count;
    int id = 1;
  };

  static void AddQuadInPass(cc::RenderPass* pass, Quad desc) {
    switch (desc.material) {
      case DrawQuad::SOLID_COLOR:
        AddQuad(pass, gfx::Rect(0, 0, 5, 5), desc.color);
        break;
      case DrawQuad::SURFACE_CONTENT:
        AddSurfaceQuad(pass, gfx::Size(5, 5), desc.opacity,
                       desc.primary_surface_id, desc.fallback_surface_id);
        break;
      case DrawQuad::RENDER_PASS:
        AddRenderPassQuad(pass, desc.render_pass_id);
        break;
      default:
        NOTREACHED();
    }
  }

  static void AddPasses(cc::RenderPassList* pass_list,
                        const gfx::Rect& output_rect,
                        Pass* passes,
                        size_t pass_count) {
    gfx::Transform root_transform;
    for (size_t i = 0; i < pass_count; ++i) {
      Pass pass = passes[i];
      cc::RenderPass* test_pass =
          AddRenderPass(pass_list, pass.id, output_rect, root_transform,
                        cc::FilterOperations());
      for (size_t j = 0; j < pass.quad_count; ++j) {
        AddQuadInPass(test_pass, pass.quads[j]);
      }
    }
  }

  static void TestQuadMatchesExpectations(Quad expected_quad,
                                          const DrawQuad* quad) {
    switch (expected_quad.material) {
      case DrawQuad::SOLID_COLOR: {
        ASSERT_EQ(DrawQuad::SOLID_COLOR, quad->material);

        const auto* solid_color_quad =
            cc::SolidColorDrawQuad::MaterialCast(quad);

        EXPECT_EQ(expected_quad.color, solid_color_quad->color);
        break;
      }
      case DrawQuad::RENDER_PASS: {
        ASSERT_EQ(DrawQuad::RENDER_PASS, quad->material);

        const auto* render_pass_quad =
            cc::RenderPassDrawQuad::MaterialCast(quad);

        EXPECT_EQ(expected_quad.render_pass_id,
                  render_pass_quad->render_pass_id);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  static void TestPassMatchesExpectations(Pass expected_pass,
                                          const cc::RenderPass* pass) {
    ASSERT_EQ(expected_pass.quad_count, pass->quad_list.size());
    for (auto iter = pass->quad_list.cbegin(); iter != pass->quad_list.cend();
         ++iter) {
      SCOPED_TRACE(base::StringPrintf("Quad number %" PRIuS, iter.index()));
      TestQuadMatchesExpectations(expected_pass.quads[iter.index()], *iter);
    }
  }

  static void TestPassesMatchExpectations(Pass* expected_passes,
                                          size_t expected_pass_count,
                                          const cc::RenderPassList* passes) {
    ASSERT_EQ(expected_pass_count, passes->size());

    for (size_t i = 0; i < passes->size(); ++i) {
      SCOPED_TRACE(base::StringPrintf("Pass number %" PRIuS, i));
      cc::RenderPass* pass = (*passes)[i].get();
      TestPassMatchesExpectations(expected_passes[i], pass);
    }
  }

 private:
  static void AddSurfaceQuad(cc::RenderPass* pass,
                             const gfx::Size& surface_size,
                             float opacity,
                             const SurfaceId& primary_surface_id,
                             const SurfaceId& fallback_surface_id) {
    gfx::Transform layer_to_target_transform;
    gfx::Size layer_bounds = surface_size;
    gfx::Rect visible_layer_rect = gfx::Rect(surface_size);
    gfx::Rect clip_rect = gfx::Rect(surface_size);
    bool is_clipped = false;
    bool are_contents_opaque = false;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;

    auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(
        layer_to_target_transform, gfx::Rect(layer_bounds), visible_layer_rect,
        clip_rect, is_clipped, are_contents_opaque, opacity, blend_mode, 0);

    cc::SurfaceDrawQuad* surface_quad =
        pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    cc::SurfaceDrawQuad* fallback_surface_quad = nullptr;
    if (fallback_surface_id.is_valid())
      fallback_surface_quad =
          pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();

    gfx::Rect quad_rect = gfx::Rect(surface_size);
    surface_quad->SetNew(pass->shared_quad_state_list.back(), quad_rect,
                         quad_rect, primary_surface_id,
                         cc::SurfaceDrawQuadType::PRIMARY,
                         fallback_surface_quad);

    if (fallback_surface_quad) {
      fallback_surface_quad->SetNew(pass->shared_quad_state_list.back(),
                                    quad_rect, quad_rect, fallback_surface_id,
                                    cc::SurfaceDrawQuadType::FALLBACK, nullptr);
    }
  }

  static void AddRenderPassQuad(cc::RenderPass* pass,
                                cc::RenderPassId render_pass_id) {
    gfx::Rect output_rect = gfx::Rect(0, 0, 5, 5);
    auto* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), output_rect, output_rect,
                         output_rect, false, false, 1, SkBlendMode::kSrcOver,
                         0);
    auto* quad = pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
    quad->SetNew(shared_state, output_rect, output_rect, render_pass_id, 0,
                 gfx::RectF(), gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                 gfx::RectF());
  }

 protected:
  FrameSinkManagerImpl manager_;
  FakeSurfaceObserver observer_;
  cc::FakeCompositorFrameSinkSupportClient fake_client_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  SurfaceAggregator aggregator_;
};

class SurfaceAggregatorValidSurfaceTest : public SurfaceAggregatorTest {
 public:
  explicit SurfaceAggregatorValidSurfaceTest(bool use_damage_rect)
      : SurfaceAggregatorTest(use_damage_rect),
        child_support_(
            CompositorFrameSinkSupport::Create(nullptr,
                                               &manager_,
                                               kArbitraryReservedFrameSinkId,
                                               kChildIsRoot,

                                               kNeedsSyncPoints)) {}
  SurfaceAggregatorValidSurfaceTest()
      : SurfaceAggregatorValidSurfaceTest(false) {}

  void SetUp() override {
    SurfaceAggregatorTest::SetUp();
    root_local_surface_id_ = allocator_.GenerateId();
    root_surface_ = manager_.surface_manager()->GetSurfaceForId(
        SurfaceId(support_->frame_sink_id(), root_local_surface_id_));
  }

  void TearDown() override {
    child_support_->EvictCurrentSurface();
    SurfaceAggregatorTest::TearDown();
  }

  void AggregateAndVerify(Pass* expected_passes,
                          size_t expected_pass_count,
                          SurfaceId* surface_ids,
                          size_t expected_surface_count) {
    cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(
        SurfaceId(support_->frame_sink_id(), root_local_surface_id_));

    TestPassesMatchExpectations(expected_passes, expected_pass_count,
                                &aggregated_frame.render_pass_list);

    // Ensure no duplicate pass ids output.
    std::set<cc::RenderPassId> used_passes;
    for (const auto& pass : aggregated_frame.render_pass_list) {
      EXPECT_TRUE(used_passes.insert(pass->id).second);
    }

    EXPECT_EQ(expected_surface_count,
              aggregator_.previous_contained_surfaces().size());
    for (size_t i = 0; i < expected_surface_count; i++) {
      EXPECT_TRUE(
          aggregator_.previous_contained_surfaces().find(surface_ids[i]) !=
          aggregator_.previous_contained_surfaces().end());
    }
  }

  void SubmitPassListAsFrame(CompositorFrameSinkSupport* support,
                             const LocalSurfaceId& local_surface_id,
                             cc::RenderPassList* pass_list) {
    cc::CompositorFrame frame = test::MakeEmptyCompositorFrame();
    pass_list->swap(frame.render_pass_list);

    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  void SubmitCompositorFrame(CompositorFrameSinkSupport* support,
                             Pass* passes,
                             size_t pass_count,
                             const LocalSurfaceId& local_surface_id) {
    cc::RenderPassList pass_list;
    AddPasses(&pass_list, gfx::Rect(SurfaceSize()), passes, pass_count);
    SubmitPassListAsFrame(support, local_surface_id, &pass_list);
  }

  void QueuePassAsFrame(std::unique_ptr<cc::RenderPass> pass,
                        const LocalSurfaceId& local_surface_id,
                        CompositorFrameSinkSupport* support) {
    cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
    child_frame.render_pass_list.push_back(std::move(pass));

    support->SubmitCompositorFrame(local_surface_id, std::move(child_frame));
  }

 protected:
  LocalSurfaceId root_local_surface_id_;
  Surface* root_surface_;
  LocalSurfaceIdAllocator allocator_;
  std::unique_ptr<CompositorFrameSinkSupport> child_support_;
  LocalSurfaceIdAllocator child_allocator_;
};

// Tests that a very simple frame containing only two solid color quads makes it
// through the aggregator correctly.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleFrame) {
  Quad quads[] = {Quad::SolidColorQuad(SK_ColorRED),
                  Quad::SolidColorQuad(SK_ColorBLUE)};
  Pass passes[] = {Pass(quads, arraysize(quads))};

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SurfaceId ids[] = {root_surface_id};

  AggregateAndVerify(passes, arraysize(passes), ids, arraysize(ids));

  // Check that WillDrawSurface was called.
  EXPECT_EQ(gfx::Rect(SurfaceSize()), fake_client_.last_damage_rect());
  EXPECT_EQ(root_local_surface_id_, fake_client_.last_local_surface_id());

  // Check that SurfaceObserver::OnSurfaceWillDraw was called.
  EXPECT_TRUE(observer_.SurfaceWillDrawCalled(root_surface_id));
}

TEST_F(SurfaceAggregatorValidSurfaceTest, OpacityCopied) {
  auto embedded_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  Quad embedded_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN),
                           Quad::SolidColorQuad(SK_ColorBLUE)};
  Pass embedded_passes[] = {Pass(embedded_quads, arraysize(embedded_quads))};

  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        arraysize(embedded_passes), embedded_local_surface_id);

  Quad quads[] = {
      Quad::SurfaceQuad(embedded_surface_id, InvalidSurfaceId(), .5f)};
  Pass passes[] = {Pass(quads, arraysize(quads))};

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  auto& render_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, render_pass_list.size());
  auto& shared_quad_state_list = render_pass_list[0]->shared_quad_state_list;
  ASSERT_EQ(2u, shared_quad_state_list.size());
  EXPECT_EQ(1.f, shared_quad_state_list.ElementAt(0)->opacity);
  EXPECT_EQ(1.f, shared_quad_state_list.ElementAt(1)->opacity);

  auto& shared_quad_state_list2 = render_pass_list[1]->shared_quad_state_list;
  ASSERT_EQ(1u, shared_quad_state_list2.size());
  EXPECT_EQ(.5f, shared_quad_state_list2.ElementAt(0)->opacity);

  embedded_support->EvictCurrentSurface();
}

TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSimpleFrame) {
  Quad quads[][2] = {{Quad::SolidColorQuad(SK_ColorWHITE),
                      Quad::SolidColorQuad(SK_ColorLTGRAY)},
                     {Quad::SolidColorQuad(SK_ColorGRAY),
                      Quad::SolidColorQuad(SK_ColorDKGRAY)}};
  Pass passes[] = {Pass(quads[0], arraysize(quads[0]), 1),
                   Pass(quads[1], arraysize(quads[1]), 2)};

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SurfaceId ids[] = {root_surface_id};

  AggregateAndVerify(passes, arraysize(passes), ids, arraysize(ids));
}

// Ensure that the render pass ID map properly keeps and deletes entries.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassDeallocation) {
  Quad quads[][2] = {{Quad::SolidColorQuad(SK_ColorWHITE),
                      Quad::SolidColorQuad(SK_ColorLTGRAY)},
                     {Quad::SolidColorQuad(SK_ColorGRAY),
                      Quad::SolidColorQuad(SK_ColorDKGRAY)}};
  Pass passes[] = {Pass(quads[0], arraysize(quads[0]), 2),
                   Pass(quads[1], arraysize(quads[1]), 1)};

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  SurfaceId surface_id(support_->frame_sink_id(), root_local_surface_id_);

  cc::CompositorFrame aggregated_frame;
  aggregated_frame = aggregator_.Aggregate(surface_id);
  auto id0 = aggregated_frame.render_pass_list[0]->id;
  auto id1 = aggregated_frame.render_pass_list[1]->id;
  EXPECT_NE(id1, id0);

  // Aggregated cc::RenderPass ids should remain the same between frames.
  aggregated_frame = aggregator_.Aggregate(surface_id);
  EXPECT_EQ(id0, aggregated_frame.render_pass_list[0]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  Pass passes2[] = {Pass(quads[0], arraysize(quads[0]), 3),
                    Pass(quads[1], arraysize(quads[1]), 1)};

  SubmitCompositorFrame(support_.get(), passes2, arraysize(passes2),
                        root_local_surface_id_);

  // The cc::RenderPass that still exists should keep the same ID.
  aggregated_frame = aggregator_.Aggregate(surface_id);
  auto id2 = aggregated_frame.render_pass_list[0]->id;
  EXPECT_NE(id2, id1);
  EXPECT_NE(id2, id0);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  // |id1| didn't exist in the previous frame, so it should be
  // mapped to a new ID.
  aggregated_frame = aggregator_.Aggregate(surface_id);
  auto id3 = aggregated_frame.render_pass_list[0]->id;
  EXPECT_NE(id3, id2);
  EXPECT_NE(id3, id1);
  EXPECT_NE(id3, id0);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);
}

// This tests very simple embedding. root_surface has a frame containing a few
// solid color quads and a surface quad referencing embedded_surface.
// embedded_surface has a frame containing only a solid color quad. The solid
// color quad should be aggregated into the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleSurfaceReference) {
  auto embedded_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  Quad embedded_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass embedded_passes[] = {Pass(embedded_quads, arraysize(embedded_quads))};

  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        arraysize(embedded_passes), embedded_local_surface_id);

  Quad root_quads[] = {
      Quad::SolidColorQuad(SK_ColorWHITE),
      Quad::SurfaceQuad(embedded_surface_id, InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorBLACK)};
  Pass root_passes[] = {Pass(root_quads, arraysize(root_quads))};

  SubmitCompositorFrame(support_.get(), root_passes, arraysize(root_passes),
                        root_local_surface_id_);

  Quad expected_quads[] = {Quad::SolidColorQuad(SK_ColorWHITE),
                           Quad::SolidColorQuad(SK_ColorGREEN),
                           Quad::SolidColorQuad(SK_ColorBLACK)};
  Pass expected_passes[] = {Pass(expected_quads, arraysize(expected_quads))};
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SurfaceId ids[] = {root_surface_id, embedded_surface_id};
  AggregateAndVerify(expected_passes, arraysize(expected_passes), ids,
                     arraysize(ids));

  embedded_support->EvictCurrentSurface();
}

// This test verifies that in the absence of a primary Surface,
// SurfaceAggregator will embed a fallback Surface, if available. If the primary
// Surface is available, though, the fallback will not be used.
TEST_F(SurfaceAggregatorValidSurfaceTest, FallbackSurfaceReference) {
  auto primary_child_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId primary_child_local_surface_id = allocator_.GenerateId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  auto fallback_child_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId fallback_child_local_surface_id = allocator_.GenerateId();
  SurfaceId fallback_child_surface_id(fallback_child_support->frame_sink_id(),
                                      fallback_child_local_surface_id);

  Quad fallback_child_quads[] = {Quad::SolidColorQuad(SK_ColorRED)};
  Pass fallback_child_passes[] = {
      Pass(fallback_child_quads, arraysize(fallback_child_quads))};

  // Submit a cc::CompositorFrame to the fallback Surface containing a red
  // SolidColorDrawQuad.
  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,
                        arraysize(fallback_child_passes),
                        fallback_child_local_surface_id);

  // Try to embed |primary_child_surface_id| and if unavailabe, embed
  // |fallback_child_surface_id|.
  Quad root_quads[] = {Quad::SurfaceQuad(primary_child_surface_id,
                                         fallback_child_surface_id, 1.f)};
  Pass root_passes[] = {Pass(root_quads, arraysize(root_quads))};

  SubmitCompositorFrame(support_.get(), root_passes, arraysize(root_passes),
                        root_local_surface_id_);

  // There is no cc::CompositorFrame submitted to |primary_child_surface_id| and
  // so |fallback_child_surface_id| will be embedded and we should see a red
  // SolidColorDrawQuad.
  Quad expected_quads1[] = {Quad::SolidColorQuad(SK_ColorRED)};
  Pass expected_passes1[] = {Pass(expected_quads1, arraysize(expected_quads1))};

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SurfaceId ids[] = {root_surface_id, primary_child_surface_id,
                     fallback_child_surface_id};
  AggregateAndVerify(expected_passes1, arraysize(expected_passes1), ids,
                     arraysize(ids));

  // Check that SurfaceObserver::OnSurfaceWillDraw was called only
  // for the fallback surface.
  EXPECT_FALSE(observer_.SurfaceWillDrawCalled(primary_child_surface_id));
  EXPECT_TRUE(observer_.SurfaceWillDrawCalled(fallback_child_surface_id));

  observer_.Reset();

  Quad primary_child_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass primary_child_passes[] = {
      Pass(primary_child_quads, arraysize(primary_child_quads))};

  // Submit a cc::CompositorFrame to the primary Surface containing a green
  // SolidColorDrawQuad.
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,
                        arraysize(primary_child_passes),
                        primary_child_local_surface_id);

  // Now that the primary Surface has a cc::CompositorFrame, we expect
  // SurfaceAggregator to embed the primary Surface, and drop the fallback
  // Surface.
  Quad expected_quads2[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass expected_passes2[] = {Pass(expected_quads2, arraysize(expected_quads2))};
  AggregateAndVerify(expected_passes2, arraysize(expected_passes2), ids,
                     arraysize(ids));

  // Check that SurfaceObserver::OnSurfaceWillDraw was called only
  // for the primary surface.
  EXPECT_TRUE(observer_.SurfaceWillDrawCalled(primary_child_surface_id));
  EXPECT_FALSE(observer_.SurfaceWillDrawCalled(fallback_child_surface_id));

  primary_child_support->EvictCurrentSurface();
  fallback_child_support->EvictCurrentSurface();
}

// This test verifies that in the presence of both primary Surface and fallback
// Surface, the fallback will not be used.
TEST_F(SurfaceAggregatorValidSurfaceTest, FallbackSurfaceReferenceWithPrimary) {
  auto primary_child_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId primary_child_local_surface_id = allocator_.GenerateId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);
  Quad primary_child_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass primary_child_passes[] = {
      Pass(primary_child_quads, arraysize(primary_child_quads))};

  // Submit a cc::CompositorFrame to the primary Surface containing a green
  // SolidColorDrawQuad.
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,
                        arraysize(primary_child_passes),
                        primary_child_local_surface_id);

  auto fallback_child_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId fallback_child_local_surface_id = allocator_.GenerateId();
  SurfaceId fallback_child_surface_id(fallback_child_support->frame_sink_id(),
                                      fallback_child_local_surface_id);

  Quad fallback_child_quads[] = {Quad::SolidColorQuad(SK_ColorRED)};
  Pass fallback_child_passes[] = {
      Pass(fallback_child_quads, arraysize(fallback_child_quads))};

  // Submit a cc::CompositorFrame to the fallback Surface containing a red
  // SolidColorDrawQuad.
  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,
                        arraysize(fallback_child_passes),
                        fallback_child_local_surface_id);

  // Try to embed |primary_child_surface_id| and if unavailabe, embed
  // |fallback_child_surface_id|.
  Quad root_quads[] = {Quad::SurfaceQuad(primary_child_surface_id,
                                         fallback_child_surface_id, 1.f)};
  Pass root_passes[] = {Pass(root_quads, arraysize(root_quads))};

  SubmitCompositorFrame(support_.get(), root_passes, arraysize(root_passes),
                        root_local_surface_id_);

  // The cc::CompositorFrame is submitted to |primary_child_surface_id|, so
  // |fallback_child_surface_id| will not be used and we should see a green
  // SolidColorDrawQuad.
  Quad expected_quads1[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass expected_passes1[] = {Pass(expected_quads1, arraysize(expected_quads1))};

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SurfaceId ids[] = {root_surface_id, primary_child_surface_id,
                     fallback_child_surface_id};
  AggregateAndVerify(expected_passes1, arraysize(expected_passes1), ids,
                     arraysize(ids));

  primary_child_support->EvictCurrentSurface();
  fallback_child_support->EvictCurrentSurface();
}

TEST_F(SurfaceAggregatorValidSurfaceTest, CopyRequest) {
  auto embedded_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  Quad embedded_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass embedded_passes[] = {Pass(embedded_quads, arraysize(embedded_quads))};

  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        arraysize(embedded_passes), embedded_local_surface_id);
  auto copy_request = CopyOutputRequest::CreateStubForTesting();
  auto* copy_request_ptr = copy_request.get();
  embedded_support->RequestCopyOfSurface(std::move(copy_request));

  Quad root_quads[] = {
      Quad::SolidColorQuad(SK_ColorWHITE),
      Quad::SurfaceQuad(embedded_surface_id, InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorBLACK)};
  Pass root_passes[] = {Pass(root_quads, arraysize(root_quads))};

  SubmitCompositorFrame(support_.get(), root_passes, arraysize(root_passes),
                        root_local_surface_id_);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  Quad expected_quads[] = {
      Quad::SolidColorQuad(SK_ColorWHITE),
      Quad::RenderPassQuad(aggregated_frame.render_pass_list[0]->id),
      Quad::SolidColorQuad(SK_ColorBLACK)};
  Pass expected_passes[] = {Pass(embedded_quads, arraysize(embedded_quads)),
                            Pass(expected_quads, arraysize(expected_quads))};
  TestPassesMatchExpectations(expected_passes, arraysize(expected_passes),
                              &aggregated_frame.render_pass_list);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());

  SurfaceId surface_ids[] = {root_surface_id, embedded_surface_id};
  EXPECT_EQ(arraysize(surface_ids),
            aggregator_.previous_contained_surfaces().size());
  for (size_t i = 0; i < arraysize(surface_ids); i++) {
    EXPECT_TRUE(
        aggregator_.previous_contained_surfaces().find(surface_ids[i]) !=
        aggregator_.previous_contained_surfaces().end());
  }

  embedded_support->EvictCurrentSurface();
}

// Root surface may contain copy requests.
TEST_F(SurfaceAggregatorValidSurfaceTest, RootCopyRequest) {
  auto embedded_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  Quad embedded_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass embedded_passes[] = {Pass(embedded_quads, arraysize(embedded_quads))};

  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        arraysize(embedded_passes), embedded_local_surface_id);
  auto copy_request(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request_ptr = copy_request.get();
  auto copy_request2(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request2_ptr = copy_request2.get();

  Quad root_quads[] = {
      Quad::SolidColorQuad(SK_ColorWHITE),
      Quad::SurfaceQuad(embedded_surface_id, InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorBLACK)};
  Quad root_quads2[] = {Quad::SolidColorQuad(SK_ColorRED)};
  Pass root_passes[] = {Pass(root_quads, arraysize(root_quads), 1),
                        Pass(root_quads2, arraysize(root_quads2), 2)};
  {
    cc::CompositorFrame frame = test::MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));
    frame.render_pass_list[0]->copy_requests.push_back(std::move(copy_request));
    frame.render_pass_list[1]->copy_requests.push_back(
        std::move(copy_request2));

    support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  Quad expected_quads[] = {Quad::SolidColorQuad(SK_ColorWHITE),
                           Quad::SolidColorQuad(SK_ColorGREEN),
                           Quad::SolidColorQuad(SK_ColorBLACK)};
  Pass expected_passes[] = {Pass(expected_quads, arraysize(expected_quads)),
                            Pass(root_quads2, arraysize(root_quads2))};
  TestPassesMatchExpectations(expected_passes, arraysize(expected_passes),
                              &aggregated_frame.render_pass_list);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[1]->copy_requests.size());
  DCHECK_EQ(copy_request2_ptr,
            aggregated_frame.render_pass_list[1]->copy_requests[0].get());

  SurfaceId surface_ids[] = {root_surface_id, embedded_surface_id};
  EXPECT_EQ(arraysize(surface_ids),
            aggregator_.previous_contained_surfaces().size());
  for (size_t i = 0; i < arraysize(surface_ids); i++) {
    EXPECT_TRUE(
        aggregator_.previous_contained_surfaces().find(surface_ids[i]) !=
        aggregator_.previous_contained_surfaces().end());
  }

  // Ensure copy requests have been removed from root surface.
  const cc::CompositorFrame& original_frame =
      manager_.surface_manager()
          ->GetSurfaceForId(root_surface_id)
          ->GetActiveFrame();
  const auto& original_pass_list = original_frame.render_pass_list;
  ASSERT_EQ(2u, original_pass_list.size());
  DCHECK(original_pass_list[0]->copy_requests.empty());
  DCHECK(original_pass_list[1]->copy_requests.empty());

  embedded_support->EvictCurrentSurface();
}

TEST_F(SurfaceAggregatorValidSurfaceTest, UnreferencedSurface) {
  auto embedded_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  auto parent_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId2, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);
  SurfaceId nonexistent_surface_id(support_->frame_sink_id(),
                                   allocator_.GenerateId());

  Quad embedded_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN)};
  Pass embedded_passes[] = {Pass(embedded_quads, arraysize(embedded_quads))};

  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        arraysize(embedded_passes), embedded_local_surface_id);
  auto copy_request(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request_ptr = copy_request.get();
  embedded_support->RequestCopyOfSurface(std::move(copy_request));

  LocalSurfaceId parent_local_surface_id = allocator_.GenerateId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);

  Quad parent_quads[] = {
      Quad::SolidColorQuad(SK_ColorGRAY),
      Quad::SurfaceQuad(embedded_surface_id, InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorLTGRAY)};
  Pass parent_passes[] = {Pass(parent_quads, arraysize(parent_quads))};

  {
    cc::CompositorFrame frame = test::MakeEmptyCompositorFrame();

    AddPasses(&frame.render_pass_list, gfx::Rect(SurfaceSize()), parent_passes,
              arraysize(parent_passes));

    frame.metadata.referenced_surfaces.push_back(embedded_surface_id);

    parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                          std::move(frame));
  }

  Quad root_quads[] = {Quad::SolidColorQuad(SK_ColorWHITE),
                       Quad::SolidColorQuad(SK_ColorBLACK)};
  Pass root_passes[] = {Pass(root_quads, arraysize(root_quads))};

  {
    cc::CompositorFrame frame = test::MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    frame.metadata.referenced_surfaces.push_back(parent_surface_id);
    // Reference to Surface ID of a Surface that doesn't exist should be
    // included in previous_contained_surfaces, but otherwise ignored.
    frame.metadata.referenced_surfaces.push_back(nonexistent_surface_id);

    support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  // First pass should come from surface that had a copy request but was not
  // referenced directly. The second pass comes from the root surface.
  // parent_quad should be ignored because it is neither referenced through a
  // SurfaceDrawQuad nor has a copy request on it.
  Pass expected_passes[] = {Pass(embedded_quads, arraysize(embedded_quads)),
                            Pass(root_quads, arraysize(root_quads))};
  TestPassesMatchExpectations(expected_passes, arraysize(expected_passes),
                              &aggregated_frame.render_pass_list);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());

  SurfaceId surface_ids[] = {
      SurfaceId(support_->frame_sink_id(), root_local_surface_id_),
      parent_surface_id, embedded_surface_id, nonexistent_surface_id};
  EXPECT_EQ(arraysize(surface_ids),
            aggregator_.previous_contained_surfaces().size());
  for (size_t i = 0; i < arraysize(surface_ids); i++) {
    EXPECT_TRUE(
        aggregator_.previous_contained_surfaces().find(surface_ids[i]) !=
        aggregator_.previous_contained_surfaces().end());
  }

  embedded_support->EvictCurrentSurface();
  parent_support->EvictCurrentSurface();
}

// This tests referencing a surface that has multiple render passes.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSurfaceReference) {
  LocalSurfaceId embedded_local_surface_id = child_allocator_.GenerateId();
  SurfaceId embedded_surface_id(child_support_->frame_sink_id(),
                                embedded_local_surface_id);

  int pass_ids[] = {1, 2, 3};

  Quad embedded_quads[][2] = {
      {Quad::SolidColorQuad(1), Quad::SolidColorQuad(2)},
      {Quad::SolidColorQuad(3), Quad::RenderPassQuad(pass_ids[0])},
      {Quad::SolidColorQuad(4), Quad::RenderPassQuad(pass_ids[1])}};
  Pass embedded_passes[] = {
      Pass(embedded_quads[0], arraysize(embedded_quads[0]), pass_ids[0]),
      Pass(embedded_quads[1], arraysize(embedded_quads[1]), pass_ids[1]),
      Pass(embedded_quads[2], arraysize(embedded_quads[2]), pass_ids[2])};

  SubmitCompositorFrame(child_support_.get(), embedded_passes,
                        arraysize(embedded_passes), embedded_local_surface_id);

  Quad root_quads[][2] = {
      {Quad::SolidColorQuad(5), Quad::SolidColorQuad(6)},
      {Quad::SurfaceQuad(embedded_surface_id, InvalidSurfaceId(), 1.f),
       Quad::RenderPassQuad(pass_ids[0])},
      {Quad::SolidColorQuad(7), Quad::RenderPassQuad(pass_ids[1])}};
  Pass root_passes[] = {
      Pass(root_quads[0], arraysize(root_quads[0]), pass_ids[0]),
      Pass(root_quads[1], arraysize(root_quads[1]), pass_ids[1]),
      Pass(root_quads[2], arraysize(root_quads[2]), pass_ids[2])};

  SubmitCompositorFrame(support_.get(), root_passes, arraysize(root_passes),
                        root_local_surface_id_);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(5u, aggregated_pass_list.size());
  cc::RenderPassId actual_pass_ids[] = {
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
    TestPassMatchesExpectations(root_passes[0], aggregated_pass_list[0].get());
  }

  {
    SCOPED_TRACE("Second pass");
    // The next two passes will be from the embedded surface since we have to
    // draw those passes before they are referenced from the render pass draw
    // quad embedded into the root surface's second pass.
    // First, there's the first embedded pass which doesn't reference anything
    // else.
    TestPassMatchesExpectations(embedded_passes[0],
                                aggregated_pass_list[1].get());
  }

  {
    SCOPED_TRACE("Third pass");
    const auto& third_pass_quad_list = aggregated_pass_list[2]->quad_list;
    ASSERT_EQ(2u, third_pass_quad_list.size());
    TestQuadMatchesExpectations(embedded_quads[1][0],
                                third_pass_quad_list.ElementAt(0));

    // This render pass pass quad will reference the first pass from the
    // embedded surface, which is the second pass in the aggregated frame.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              third_pass_quad_list.ElementAt(1)->material);
    const auto* third_pass_render_pass_draw_quad =
        cc::RenderPassDrawQuad::MaterialCast(third_pass_quad_list.ElementAt(1));
    EXPECT_EQ(actual_pass_ids[1],
              third_pass_render_pass_draw_quad->render_pass_id);
  }

  {
    SCOPED_TRACE("Fourth pass");
    // The fourth pass will have aggregated quads from the root surface's second
    // pass and the embedded surface's first pass.
    const auto& fourth_pass_quad_list = aggregated_pass_list[3]->quad_list;
    ASSERT_EQ(3u, fourth_pass_quad_list.size());

    // The first quad will be the yellow quad from the embedded surface's last
    // pass.
    TestQuadMatchesExpectations(embedded_quads[2][0],
                                fourth_pass_quad_list.ElementAt(0));

    // The next quad will be a render pass quad referencing the second pass from
    // the embedded surface, which is the third pass in the aggregated frame.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              fourth_pass_quad_list.ElementAt(1)->material);
    const auto* fourth_pass_first_render_pass_draw_quad =
        cc::RenderPassDrawQuad::MaterialCast(
            fourth_pass_quad_list.ElementAt(1));
    EXPECT_EQ(actual_pass_ids[2],
              fourth_pass_first_render_pass_draw_quad->render_pass_id);

    // The last quad will be a render pass quad referencing the first pass from
    // the root surface, which is the first pass overall.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              fourth_pass_quad_list.ElementAt(2)->material);
    const auto* fourth_pass_second_render_pass_draw_quad =
        cc::RenderPassDrawQuad::MaterialCast(
            fourth_pass_quad_list.ElementAt(2));
    EXPECT_EQ(actual_pass_ids[0],
              fourth_pass_second_render_pass_draw_quad->render_pass_id);
  }

  {
    SCOPED_TRACE("Fifth pass");
    const auto& fifth_pass_quad_list = aggregated_pass_list[4]->quad_list;
    ASSERT_EQ(2u, fifth_pass_quad_list.size());

    TestQuadMatchesExpectations(root_quads[2][0],
                                fifth_pass_quad_list.ElementAt(0));

    // The last quad in the last pass will reference the second pass from the
    // root surface, which after aggregating is the fourth pass in the overall
    // list.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              fifth_pass_quad_list.ElementAt(1)->material);
    const auto* fifth_pass_render_pass_draw_quad =
        cc::RenderPassDrawQuad::MaterialCast(fifth_pass_quad_list.ElementAt(1));
    EXPECT_EQ(actual_pass_ids[3],
              fifth_pass_render_pass_draw_quad->render_pass_id);
  }
}

// Tests an invalid surface reference in a frame. The surface quad should just
// be dropped.
TEST_F(SurfaceAggregatorValidSurfaceTest, InvalidSurfaceReference) {
  Quad quads[] = {
      Quad::SolidColorQuad(SK_ColorGREEN),
      Quad::SurfaceQuad(InvalidSurfaceId(), InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorBLUE)};
  Pass passes[] = {Pass(quads, arraysize(quads))};

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  Quad expected_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN),
                           Quad::SolidColorQuad(SK_ColorBLUE)};
  Pass expected_passes[] = {Pass(expected_quads, arraysize(expected_quads))};
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SurfaceId ids[] = {root_surface_id, InvalidSurfaceId()};

  AggregateAndVerify(expected_passes, arraysize(expected_passes), ids,
                     arraysize(ids));
}

// Tests a reference to a valid surface with no submitted frame. This quad
// should also just be dropped.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidSurfaceReferenceWithNoFrame) {
  LocalSurfaceId empty_local_surface_id = allocator_.GenerateId();
  SurfaceId surface_with_no_frame_id(support_->frame_sink_id(),
                                     empty_local_surface_id);

  Quad quads[] = {
      Quad::SolidColorQuad(SK_ColorGREEN),
      Quad::SurfaceQuad(surface_with_no_frame_id, InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorBLUE)};
  Pass passes[] = {Pass(quads, arraysize(quads))};

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  Quad expected_quads[] = {Quad::SolidColorQuad(SK_ColorGREEN),
                           Quad::SolidColorQuad(SK_ColorBLUE)};
  Pass expected_passes[] = {Pass(expected_quads, arraysize(expected_quads))};
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SurfaceId ids[] = {root_surface_id, surface_with_no_frame_id};
  AggregateAndVerify(expected_passes, arraysize(expected_passes), ids,
                     arraysize(ids));
}

// Tests a surface quad referencing itself, generating a trivial cycle.
// The quad creating the cycle should be dropped from the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleCyclicalReference) {
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  Quad quads[] = {Quad::SurfaceQuad(root_surface_id, InvalidSurfaceId(), 1.f),
                  Quad::SolidColorQuad(SK_ColorYELLOW)};
  Pass passes[] = {Pass(quads, arraysize(quads))};

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  Quad expected_quads[] = {Quad::SolidColorQuad(SK_ColorYELLOW)};
  Pass expected_passes[] = {Pass(expected_quads, arraysize(expected_quads))};
  SurfaceId ids[] = {root_surface_id};
  AggregateAndVerify(expected_passes, arraysize(expected_passes), ids,
                     arraysize(ids));
}

// Tests a more complex cycle with one intermediate surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, TwoSurfaceCyclicalReference) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);

  Quad parent_quads[] = {
      Quad::SolidColorQuad(SK_ColorBLUE),
      Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorCYAN)};
  Pass parent_passes[] = {Pass(parent_quads, arraysize(parent_quads))};

  SubmitCompositorFrame(support_.get(), parent_passes, arraysize(parent_passes),
                        root_local_surface_id_);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  Quad child_quads[] = {
      Quad::SolidColorQuad(SK_ColorGREEN),
      Quad::SurfaceQuad(root_surface_id, InvalidSurfaceId(), 1.f),
      Quad::SolidColorQuad(SK_ColorMAGENTA)};
  Pass child_passes[] = {Pass(child_quads, arraysize(child_quads))};

  SubmitCompositorFrame(child_support_.get(), child_passes,
                        arraysize(child_passes), child_local_surface_id);

  // The child surface's reference to the root_surface_ will be dropped, so
  // we'll end up with:
  //   SK_ColorBLUE from the parent
  //   SK_ColorGREEN from the child
  //   SK_ColorMAGENTA from the child
  //   SK_ColorCYAN from the parent
  Quad expected_quads[] = {Quad::SolidColorQuad(SK_ColorBLUE),
                           Quad::SolidColorQuad(SK_ColorGREEN),
                           Quad::SolidColorQuad(SK_ColorMAGENTA),
                           Quad::SolidColorQuad(SK_ColorCYAN)};
  Pass expected_passes[] = {Pass(expected_quads, arraysize(expected_quads))};
  SurfaceId ids[] = {root_surface_id, child_surface_id};
  AggregateAndVerify(expected_passes, arraysize(expected_passes), ids,
                     arraysize(ids));
}

// Tests that we map render pass IDs from different surfaces into a unified
// namespace and update cc::RenderPassDrawQuad's id references to match.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassIdMapping) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);

  cc::RenderPassId child_pass_id[] = {1u, 2u};
  Quad child_quad[][1] = {{Quad::SolidColorQuad(SK_ColorGREEN)},
                          {Quad::RenderPassQuad(child_pass_id[0])}};
  Pass surface_passes[] = {
      Pass(child_quad[0], arraysize(child_quad[0]), child_pass_id[0]),
      Pass(child_quad[1], arraysize(child_quad[1]), child_pass_id[1])};

  SubmitCompositorFrame(child_support_.get(), surface_passes,
                        arraysize(surface_passes), child_local_surface_id);

  // Pass IDs from the parent surface may collide with ones from the child.
  cc::RenderPassId parent_pass_id[] = {3u, 2u};
  Quad parent_quad[][1] = {
      {Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)},
      {Quad::RenderPassQuad(parent_pass_id[0])}};
  Pass parent_passes[] = {
      Pass(parent_quad[0], arraysize(parent_quad[0]), parent_pass_id[0]),
      Pass(parent_quad[1], arraysize(parent_quad[1]), parent_pass_id[1])};

  SubmitCompositorFrame(support_.get(), parent_passes, arraysize(parent_passes),
                        root_local_surface_id_);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());
  cc::RenderPassId actual_pass_ids[] = {aggregated_pass_list[0]->id,
                                        aggregated_pass_list[1]->id,
                                        aggregated_pass_list[2]->id};
  // Make sure the aggregated frame's pass IDs are all unique.
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(actual_pass_ids[j], actual_pass_ids[i])
          << "pass ids " << i << " and " << j;
    }
  }

  // Make sure the render pass quads reference the remapped pass IDs.
  DrawQuad* render_pass_quads[] = {aggregated_pass_list[1]->quad_list.front(),
                                   aggregated_pass_list[2]->quad_list.front()};
  ASSERT_EQ(render_pass_quads[0]->material, DrawQuad::RENDER_PASS);
  EXPECT_EQ(actual_pass_ids[0],
            cc::RenderPassDrawQuad::MaterialCast(render_pass_quads[0])
                ->render_pass_id);

  ASSERT_EQ(render_pass_quads[1]->material, DrawQuad::RENDER_PASS);
  EXPECT_EQ(actual_pass_ids[1],
            cc::RenderPassDrawQuad::MaterialCast(render_pass_quads[1])
                ->render_pass_id);
}

void AddSolidColorQuadWithBlendMode(const gfx::Size& size,
                                    cc::RenderPass* pass,
                                    const SkBlendMode blend_mode) {
  const gfx::Transform layer_to_target_transform;
  const gfx::Rect layer_rect(size);
  const gfx::Rect visible_layer_rect(size);
  const gfx::Rect clip_rect(size);

  bool is_clipped = false;
  SkColor color = SK_ColorGREEN;
  bool are_contents_opaque = SkColorGetA(color) == 0xFF;
  float opacity = 1.f;

  bool force_anti_aliasing_off = false;
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(layer_to_target_transform, layer_rect, visible_layer_rect,
              clip_rect, is_clipped, are_contents_opaque, opacity, blend_mode,
              0);

  auto* color_quad = pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
  color_quad->SetNew(pass->shared_quad_state_list.back(), visible_layer_rect,
                     visible_layer_rect, color, force_anti_aliasing_off);
}

// This tests that we update shared quad state pointers correctly within
// aggregated passes.  The shared quad state list on the aggregated pass will
// include the shared quad states from each pass in one list so the quads will
// end up pointed to shared quad state objects at different offsets. This test
// uses the blend_mode value stored on the shared quad state to track the shared
// quad state, but anything saved on the shared quad state would work.
//
// This test has 4 surfaces in the following structure:
// root_surface -> quad with kClear_Mode,
//                 [child_one_surface],
//                 quad with kDstOver_Mode,
//                 [child_two_surface],
//                 quad with kDstIn_Mode
// child_one_surface -> quad with kSrc_Mode,
//                      [grandchild_surface],
//                      quad with kSrcOver_Mode
// child_two_surface -> quad with kSrcIn_Mode
// grandchild_surface -> quad with kDst_Mode
//
// Resulting in the following aggregated pass:
//  quad_root_0       - blend_mode kClear_Mode
//  quad_child_one_0  - blend_mode kSrc_Mode
//  quad_grandchild_0 - blend_mode kDst_Mode
//  quad_child_one_1  - blend_mode kSrcOver_Mode
//  quad_root_1       - blend_mode kDstOver_Mode
//  quad_child_two_0  - blend_mode kSrcIn_Mode
//  quad_root_2       - blend_mode kDstIn_Mode
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateSharedQuadStateProperties) {
  const SkBlendMode blend_modes[] = {
      SkBlendMode::kClear,    // 0
      SkBlendMode::kSrc,      // 1
      SkBlendMode::kDst,      // 2
      SkBlendMode::kSrcOver,  // 3
      SkBlendMode::kDstOver,  // 4
      SkBlendMode::kSrcIn,    // 5
      SkBlendMode::kDstIn,    // 6
  };
  auto grandchild_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  auto child_one_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);
  auto child_two_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId3, kChildIsRoot,
      kNeedsSyncPoints);
  int pass_id = 1;
  LocalSurfaceId grandchild_local_surface_id = allocator_.GenerateId();
  SurfaceId grandchild_surface_id(grandchild_support->frame_sink_id(),
                                  grandchild_local_surface_id);

  auto grandchild_pass = cc::RenderPass::Create();
  gfx::Rect output_rect(SurfaceSize());
  gfx::Rect damage_rect(SurfaceSize());
  gfx::Transform transform_to_root_target;
  grandchild_pass->SetNew(pass_id, output_rect, damage_rect,
                          transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), grandchild_pass.get(),
                                 blend_modes[2]);
  QueuePassAsFrame(std::move(grandchild_pass), grandchild_local_surface_id,
                   grandchild_support.get());

  LocalSurfaceId child_one_local_surface_id = allocator_.GenerateId();
  SurfaceId child_one_surface_id(child_one_support->frame_sink_id(),
                                 child_one_local_surface_id);

  auto child_one_pass = cc::RenderPass::Create();
  child_one_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_one_pass.get(),
                                 blend_modes[1]);
  auto* grandchild_surface_quad =
      child_one_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  grandchild_surface_quad->SetNew(
      child_one_pass->shared_quad_state_list.back(), gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()), grandchild_surface_id,
      cc::SurfaceDrawQuadType::PRIMARY, nullptr);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_one_pass.get(),
                                 blend_modes[3]);
  QueuePassAsFrame(std::move(child_one_pass), child_one_local_surface_id,
                   child_one_support.get());

  LocalSurfaceId child_two_local_surface_id = allocator_.GenerateId();
  SurfaceId child_two_surface_id(child_two_support->frame_sink_id(),
                                 child_two_local_surface_id);

  auto child_two_pass = cc::RenderPass::Create();
  child_two_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_two_pass.get(),
                                 blend_modes[5]);
  QueuePassAsFrame(std::move(child_two_pass), child_two_local_surface_id,
                   child_two_support.get());

  auto root_pass = cc::RenderPass::Create();
  root_pass->SetNew(pass_id, output_rect, damage_rect,
                    transform_to_root_target);

  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(),
                                 blend_modes[0]);
  auto* child_one_surface_quad =
      root_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  child_one_surface_quad->SetNew(root_pass->shared_quad_state_list.back(),
                                 gfx::Rect(SurfaceSize()),
                                 gfx::Rect(SurfaceSize()), child_one_surface_id,
                                 cc::SurfaceDrawQuadType::PRIMARY, nullptr);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(),
                                 blend_modes[4]);
  auto* child_two_surface_quad =
      root_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
  child_two_surface_quad->SetNew(root_pass->shared_quad_state_list.back(),
                                 gfx::Rect(SurfaceSize()),
                                 gfx::Rect(SurfaceSize()), child_two_surface_id,
                                 cc::SurfaceDrawQuadType::PRIMARY, nullptr);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(),
                                 blend_modes[6]);

  QueuePassAsFrame(std::move(root_pass), root_local_surface_id_,
                   support_.get());

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(1u, aggregated_pass_list.size());

  const auto& aggregated_quad_list = aggregated_pass_list[0]->quad_list;

  ASSERT_EQ(7u, aggregated_quad_list.size());

  for (auto iter = aggregated_quad_list.cbegin();
       iter != aggregated_quad_list.cend(); ++iter) {
    EXPECT_EQ(blend_modes[iter.index()], iter->shared_quad_state->blend_mode)
        << iter.index();
  }

  grandchild_support->EvictCurrentSurface();
  child_one_support->EvictCurrentSurface();
  child_two_support->EvictCurrentSurface();
}

// This tests that when aggregating a frame with multiple render passes that we
// map the transforms for the root pass but do not modify the transform on child
// passes.
//
// The root surface has one pass with a surface quad transformed by +10 in the y
// direction.
//
// The middle surface has one pass with a surface quad scaled by 2 in the x
// and 3 in the y directions.
//
// The child surface has two passes. The first pass has a quad with a transform
// of +5 in the x direction. The second pass has a reference to the first pass'
// pass id and a transform of +8 in the x direction.
//
// After aggregation, the child surface's root pass quad should have all
// transforms concatenated for a total transform of +23 x, +10 y. The
// contributing render pass' transform in the aggregate frame should not be
// affected.
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateMultiplePassWithTransform) {
  auto middle_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  // Innermost child surface.
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  {
    int child_pass_id[] = {1, 2};
    Quad child_quads[][1] = {
        {Quad::SolidColorQuad(SK_ColorGREEN)},
        {Quad::RenderPassQuad(child_pass_id[0])},
    };
    Pass child_passes[] = {
        Pass(child_quads[0], arraysize(child_quads[0]), child_pass_id[0]),
        Pass(child_quads[1], arraysize(child_quads[1]), child_pass_id[1])};

    cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              child_passes, arraysize(child_passes));

    auto* child_nonroot_pass = child_frame.render_pass_list[0].get();
    child_nonroot_pass->transform_to_root_target.Translate(8, 0);
    auto* child_nonroot_pass_sqs =
        child_nonroot_pass->shared_quad_state_list.front();
    child_nonroot_pass_sqs->quad_to_target_transform.Translate(5, 0);

    auto* child_root_pass = child_frame.render_pass_list[1].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);
    child_root_pass_sqs->is_clipped = true;
    child_root_pass_sqs->clip_rect = gfx::Rect(0, 0, 5, 5);

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));
  }

  // Middle child surface.
  LocalSurfaceId middle_local_surface_id = allocator_.GenerateId();
  SurfaceId middle_surface_id(middle_support->frame_sink_id(),
                              middle_local_surface_id);
  {
    Quad middle_quads[] = {
        Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
    Pass middle_passes[] = {
        Pass(middle_quads, arraysize(middle_quads)),
    };

    cc::CompositorFrame middle_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&middle_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              middle_passes, arraysize(middle_passes));

    auto* middle_root_pass = middle_frame.render_pass_list[0].get();
    middle_root_pass->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 1, 100, 7);
    auto* middle_root_pass_sqs =
        middle_root_pass->shared_quad_state_list.front();
    middle_root_pass_sqs->quad_to_target_transform.Scale(2, 3);

    middle_support->SubmitCompositorFrame(middle_local_surface_id,
                                          std::move(middle_frame));
  }

  // Root surface.
  Quad secondary_quads[] = {
      Quad::SolidColorQuad(1),
      Quad::SurfaceQuad(middle_surface_id, InvalidSurfaceId(), 1.f)};
  Quad root_quads[] = {Quad::SolidColorQuad(1)};
  Pass root_passes[] = {Pass(secondary_quads, arraysize(secondary_quads)),
                        Pass(root_quads, arraysize(root_quads))};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.Translate(0, 10);
  root_frame.render_pass_list[0]->quad_list.ElementAt(1)->visible_rect =
      gfx::Rect(0, 0, 8, 100);

  root_frame.render_pass_list[0]->transform_to_root_target.Translate(10, 5);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());

  ASSERT_EQ(1u, aggregated_pass_list[0]->shared_quad_state_list.size());

  // The first pass should have one shared quad state for the one solid color
  // quad.
  EXPECT_EQ(1u, aggregated_pass_list[0]->shared_quad_state_list.size());
  // The second pass should have just two shared quad states. We'll
  // verify the properties through the quads.
  EXPECT_EQ(2u, aggregated_pass_list[1]->shared_quad_state_list.size());

  EXPECT_EQ(1u, aggregated_pass_list[2]->shared_quad_state_list.size());

  auto* aggregated_first_pass_sqs =
      aggregated_pass_list[0]->shared_quad_state_list.front();

  // The first pass's transform should be unaffected by the embedding and still
  // be a translation by +5 in the x direction.
  gfx::Transform expected_aggregated_first_pass_sqs_transform;
  expected_aggregated_first_pass_sqs_transform.Translate(5, 0);
  EXPECT_EQ(expected_aggregated_first_pass_sqs_transform.ToString(),
            aggregated_first_pass_sqs->quad_to_target_transform.ToString());

  // The first pass's transform to the root target should include the aggregated
  // transform, including the transform from the child pass to the root.
  gfx::Transform expected_first_pass_transform_to_root_target;
  expected_first_pass_transform_to_root_target.Translate(10, 5);
  expected_first_pass_transform_to_root_target.Translate(0, 10);
  expected_first_pass_transform_to_root_target.Scale(2, 3);
  expected_first_pass_transform_to_root_target.Translate(8, 0);
  EXPECT_EQ(expected_first_pass_transform_to_root_target.ToString(),
            aggregated_pass_list[0]->transform_to_root_target.ToString());

  ASSERT_EQ(2u, aggregated_pass_list[1]->quad_list.size());

  gfx::Transform expected_root_pass_quad_transforms[2];
  // The first quad in the root pass is the solid color quad from the original
  // root surface. Its transform should be unaffected by the aggregation and
  // still be +7 in the y direction.
  expected_root_pass_quad_transforms[0].Translate(0, 7);
  // The second quad in the root pass is aggregated from the child surface so
  // its transform should be the combination of its original translation
  // (0, 10), the middle surface draw quad's scale of (2, 3), and the
  // child surface draw quad's translation (8, 0).
  expected_root_pass_quad_transforms[1].Translate(0, 10);
  expected_root_pass_quad_transforms[1].Scale(2, 3);
  expected_root_pass_quad_transforms[1].Translate(8, 0);

  for (auto iter = aggregated_pass_list[1]->quad_list.cbegin();
       iter != aggregated_pass_list[1]->quad_list.cend(); ++iter) {
    EXPECT_EQ(expected_root_pass_quad_transforms[iter.index()].ToString(),
              iter->shared_quad_state->quad_to_target_transform.ToString())
        << iter.index();
  }

  EXPECT_TRUE(
      aggregated_pass_list[1]->shared_quad_state_list.ElementAt(1)->is_clipped);

  // The second quad in the root pass is aggregated from the child, so its
  // clip rect must be transformed by the child's translation/scale and
  // clipped be the visible_rects for both children.
  EXPECT_EQ(gfx::Rect(0, 13, 8, 12).ToString(),
            aggregated_pass_list[1]
                ->shared_quad_state_list.ElementAt(1)
                ->clip_rect.ToString());

  middle_support->EvictCurrentSurface();
}

// Tests that damage rects are aggregated correctly when surfaces change.
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateDamageRect) {
  auto parent_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  Quad child_quads[] = {Quad::RenderPassQuad(1)};
  Pass child_passes[] = {Pass(child_quads, arraysize(child_quads), 1)};

  cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
            child_passes, arraysize(child_passes));

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  Quad parent_surface_quads[] = {
      Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
  Pass parent_surface_passes[] = {
      Pass(parent_surface_quads, arraysize(parent_surface_quads), 1)};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  cc::CompositorFrame parent_surface_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, gfx::Rect(SurfaceSize()),
            parent_surface_passes, arraysize(parent_surface_passes));

  LocalSurfaceId parent_local_surface_id = allocator_.GenerateId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  Quad root_surface_quads[] = {
      Quad::SurfaceQuad(parent_surface_id, InvalidSurfaceId(), 1.f)};
  Quad root_render_pass_quads[] = {Quad::RenderPassQuad(1)};

  Pass root_passes[] = {
      Pass(root_surface_quads, arraysize(root_surface_quads), 1),
      Pass(root_render_pass_quads, arraysize(root_render_pass_quads), 2)};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 10);
  root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 10, 10);
  root_frame.render_pass_list[1]->damage_rect = gfx::Rect(5, 5, 100, 100);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // Damage rect for first aggregation should contain entire root surface.
  EXPECT_TRUE(
      aggregated_pass_list[1]->damage_rect.Contains(gfx::Rect(SurfaceSize())));

  {
    cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              child_passes, arraysize(child_passes));

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);
    child_root_pass->damage_rect = gfx::Rect(10, 10, 10, 10);

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(2u, aggregated_pass_list.size());

    // Outer surface didn't change, so transformed inner damage rect should be
    // used.
    EXPECT_EQ(gfx::Rect(10, 20, 10, 10).ToString(),
              aggregated_pass_list[1]->damage_rect.ToString());
  }

  {
    cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              root_passes, arraysize(root_passes));

    root_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(0, 10);
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(0, 0, 1, 1);

    support_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));
  }

  {
    cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              root_passes, arraysize(root_passes));

    root_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(0, 10);
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(1, 1, 1, 1);

    support_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(2u, aggregated_pass_list.size());

    // The root surface was enqueued without being aggregated once, so it should
    // be treated as completely damaged.
    EXPECT_TRUE(aggregated_pass_list[1]->damage_rect.Contains(
        gfx::Rect(SurfaceSize())));
  }

  // No Surface changed, so no damage should be given.
  {
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(2u, aggregated_pass_list.size());

    EXPECT_TRUE(aggregated_pass_list[1]->damage_rect.IsEmpty());
  }

  // SetFullDamageRectForSurface should cause the entire output to be
  // marked as damaged.
  {
    aggregator_.SetFullDamageForSurface(root_surface_id);
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(2u, aggregated_pass_list.size());

    EXPECT_TRUE(aggregated_pass_list[1]->damage_rect.Contains(
        gfx::Rect(SurfaceSize())));
  }

  parent_support->EvictCurrentSurface();
}

// Check that damage is correctly calculated for surfaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, SwitchSurfaceDamage) {
  Quad root_render_pass_quads[] = {Quad::SolidColorQuad(1)};

  Pass root_passes[] = {
      Pass(root_render_pass_quads, arraysize(root_render_pass_quads), 2)};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 100, 100);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  {
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // Damage rect for first aggregation should contain entire root surface.
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.Contains(
        gfx::Rect(SurfaceSize())));
  }

  LocalSurfaceId second_root_local_surface_id = allocator_.GenerateId();
  SurfaceId second_root_surface_id(support_->frame_sink_id(),
                                   second_root_local_surface_id);
  {
    Quad root_render_pass_quads[] = {Quad::SolidColorQuad(1)};

    Pass root_passes[] = {
        Pass(root_render_pass_quads, arraysize(root_render_pass_quads), 2)};

    cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              root_passes, arraysize(root_passes));

    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(1, 2, 3, 4);

    support_->SubmitCompositorFrame(second_root_local_surface_id,
                                    std::move(root_frame));
  }
  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(second_root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(1, 2, 3, 4), aggregated_pass_list[0]->damage_rect);
  }
  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(second_root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // No new frame, so no new damage.
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.IsEmpty());
  }
}

class SurfaceAggregatorPartialSwapTest
    : public SurfaceAggregatorValidSurfaceTest {
 public:
  SurfaceAggregatorPartialSwapTest()
      : SurfaceAggregatorValidSurfaceTest(true) {}
};

// Tests that quads outside the damage rect are ignored.
TEST_F(SurfaceAggregatorPartialSwapTest, IgnoreOutside) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  // The child surface has three quads, one with a visible rect of 13,13 4x4 and
  // the other other with a visible rect of 10,10 2x2 (relative to root target
  // space), and one with a non-invertible transform.
  {
    int child_pass_id = 1;
    Quad child_quads1[] = {Quad::RenderPassQuad(child_pass_id)};
    Quad child_quads2[] = {Quad::RenderPassQuad(child_pass_id)};
    Quad child_quads3[] = {Quad::RenderPassQuad(child_pass_id)};
    Pass child_passes[] = {
        Pass(child_quads1, arraysize(child_quads1), child_pass_id),
        Pass(child_quads2, arraysize(child_quads2), child_pass_id),
        Pass(child_quads3, arraysize(child_quads2), child_pass_id)};

    cc::RenderPassList child_pass_list;
    AddPasses(&child_pass_list, gfx::Rect(SurfaceSize()), child_passes,
              arraysize(child_passes));

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 2, 2);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(1, 1);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    auto* child_noninvertible_sqs =
        child_pass_list[2]->shared_quad_state_list.ElementAt(0u);
    child_noninvertible_sqs->quad_to_target_transform.matrix().setDouble(0, 0,
                                                                         0.0);
    EXPECT_FALSE(
        child_noninvertible_sqs->quad_to_target_transform.IsInvertible());
    child_pass_list[2]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    SubmitPassListAsFrame(child_support_.get(), child_local_surface_id,
                          &child_pass_list);
  }

  {
    Quad root_quads[] = {
        Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};

    Pass root_passes[] = {Pass(root_quads, arraysize(root_quads))};

    cc::RenderPassList root_pass_list;
    AddPasses(&root_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    auto* root_pass = root_pass_list[0].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(0, 0, 1, 1);

    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list);
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());

  // Damage rect for first aggregation should contain entire root surface.
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
  EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());

  // Create a root surface with a smaller damage rect.
  {
    Quad root_quads[] = {
        Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};

    Pass root_passes[] = {Pass(root_quads, arraysize(root_quads))};

    cc::RenderPassList root_pass_list;
    AddPasses(&root_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    auto* root_pass = root_pass_list[0].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list);
  }

  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Only first quad from surface is inside damage rect and should be
    // included.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2),
              aggregated_pass_list[1]->quad_list.back()->visible_rect);
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }

  // New child frame has same content and no damage, but has a
  // CopyOutputRequest.
  {
    int child_pass_ids[] = {1, 2};
    Quad child_quads1[] = {Quad::SolidColorQuad(1)};
    Quad child_quads2[] = {Quad::RenderPassQuad(child_pass_ids[0])};
    Pass child_passes[] = {
        Pass(child_quads1, arraysize(child_quads1), child_pass_ids[0]),
        Pass(child_quads2, arraysize(child_quads2), child_pass_ids[1])};

    cc::RenderPassList child_pass_list;
    AddPasses(&child_pass_list, gfx::Rect(SurfaceSize()), child_passes,
              arraysize(child_passes));

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 2, 2);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(1, 1);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    auto* child_root_pass = child_pass_list[1].get();

    child_root_pass->copy_requests.push_back(
        CopyOutputRequest::CreateStubForTesting());
    child_root_pass->damage_rect = gfx::Rect();
    SubmitPassListAsFrame(child_support_.get(), child_local_surface_id,
                          &child_pass_list);
  }

  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Output frame should have no damage, but all quads included.
    ASSERT_EQ(3u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_TRUE(aggregated_pass_list[2]->damage_rect.IsEmpty());
    ASSERT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    ASSERT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(gfx::Rect(1, 1, 2, 2),
              aggregated_pass_list[0]->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2),
              aggregated_pass_list[1]->quad_list.ElementAt(0)->visible_rect);
    ASSERT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }

  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    // There were no changes since last aggregation, so output should be empty
    // and have no damage.
    ASSERT_EQ(1u, aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.IsEmpty());
    ASSERT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
  }

  // Root surface has smaller damage rect, but filter on render pass means all
  // of it and its descendant passes should be aggregated.
  {
    int root_pass_ids[] = {1, 2, 3};
    Quad root_quads1[] = {
        Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
    Quad root_quads2[] = {Quad::RenderPassQuad(root_pass_ids[0])};
    Quad root_quads3[] = {Quad::RenderPassQuad(root_pass_ids[1])};
    Pass root_passes[] = {
        Pass(root_quads1, arraysize(root_quads1), root_pass_ids[0]),
        Pass(root_quads2, arraysize(root_quads2), root_pass_ids[1]),
        Pass(root_quads3, arraysize(root_quads3), root_pass_ids[2])};

    cc::RenderPassList root_pass_list;
    AddPasses(&root_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    auto* filter_pass = root_pass_list[1].get();
    filter_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    auto* root_pass = root_pass_list[2].get();
    filter_pass->filters.Append(cc::FilterOperation::CreateBlurFilter(2));
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list);
  }

  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(4u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[3]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
    // First render pass draw quad is outside damage rect, so shouldn't be
    // drawn.
    EXPECT_EQ(0u, aggregated_pass_list[3]->quad_list.size());
  }

  // Root surface has smaller damage rect. Opacity filter on render pass
  // means Surface quad under it should be aggregated.
  {
    int root_pass_ids[] = {1, 2};
    Quad root_quads1[] = {
        Quad::SolidColorQuad(1),
    };
    Quad root_quads2[] = {
        Quad::RenderPassQuad(root_pass_ids[0]),
        Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
    Pass root_passes[] = {
        Pass(root_quads1, arraysize(root_quads1), root_pass_ids[0]),
        Pass(root_quads2, arraysize(root_quads2), root_pass_ids[1])};

    cc::RenderPassList root_pass_list;
    AddPasses(&root_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    auto* pass = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(10, 10);
    pass->background_filters.Append(
        cc::FilterOperation::CreateOpacityFilter(0.5f));
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list);
  }

  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 is solid color quad from root, but outside damage rect.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad is outside damage rect, so shouldn't be
    // drawn. SurfaceDrawQuad is after opacity filter, so corresponding
    // cc::RenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }

  // TODO(wutao): Partial swap does not work with pixel moving background
  // filter. See https://crbug.com/737255.
  // Has background filter on render pass will make the whole output rect as
  // damaged.
  {
    int root_pass_ids[] = {1, 2};
    Quad root_quads1[] = {
        Quad::SolidColorQuad(1),
    };
    Quad root_quads2[] = {
        Quad::RenderPassQuad(root_pass_ids[0]),
        Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
    Pass root_passes[] = {
        Pass(root_quads1, arraysize(root_quads1), root_pass_ids[0]),
        Pass(root_quads2, arraysize(root_quads2), root_pass_ids[1])};

    cc::RenderPassList root_pass_list;
    AddPasses(&root_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    auto* pass = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(10, 10);
    pass->background_filters.Append(cc::FilterOperation::CreateBlurFilter(2));
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list);
  }

  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 has background blur filter, so should be drawn.
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad is outside damage rect but has background
    // filter, so should be drawn. SurfaceDrawQuad is after background filter,
    // so corresponding cc::RenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(2u, aggregated_pass_list[2]->quad_list.size());
  }
}

class SurfaceAggregatorWithResourcesTest : public testing::Test {
 public:
  void SetUp() override {
    shared_bitmap_manager_ = base::MakeUnique<cc::TestSharedBitmapManager>();
    resource_provider_ =
        cc::FakeResourceProvider::Create<cc::DisplayResourceProvider>(
            nullptr, shared_bitmap_manager_.get());

    aggregator_ = base::MakeUnique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), false);
    aggregator_->set_output_is_secure(true);
  }

 protected:
  FrameSinkManagerImpl manager_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<cc::DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
};

void SubmitCompositorFrameWithResources(ResourceId* resource_ids,
                                        size_t num_resource_ids,
                                        bool valid,
                                        SurfaceId child_id,
                                        CompositorFrameSinkSupport* support,
                                        SurfaceId surface_id) {
  cc::CompositorFrame frame = test::MakeEmptyCompositorFrame();
  auto pass = cc::RenderPass::Create();
  pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->opacity = 1.f;
  if (child_id.is_valid()) {
    auto* surface_quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                         child_id, cc::SurfaceDrawQuadType::PRIMARY, nullptr);
  }

  for (size_t i = 0u; i < num_resource_ids; ++i) {
    TransferableResource resource;
    resource.id = resource_ids[i];
    // ResourceProvider is software, so only software resources are valid.
    resource.is_software = valid;
    frame.resource_list.push_back(resource);
    auto* quad = pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
    const gfx::Rect rect;
    const gfx::Rect visible_rect;
    bool needs_blending = false;
    bool premultiplied_alpha = false;
    const gfx::PointF uv_top_left;
    const gfx::PointF uv_bottom_right;
    SkColor background_color = SK_ColorGREEN;
    const float vertex_opacity[4] = {0.f, 0.f, 1.f, 1.f};
    bool flipped = false;
    bool nearest_neighbor = false;
    bool secure_output_only = true;
    quad->SetAll(sqs, rect, visible_rect, needs_blending, resource_ids[i],
                 gfx::Size(), premultiplied_alpha, uv_top_left, uv_bottom_right,
                 background_color, vertex_opacity, flipped, nearest_neighbor,
                 secure_output_only);
  }
  frame.render_pass_list.push_back(std::move(pass));
  support->SubmitCompositorFrame(surface_id.local_surface_id(),
                                 std::move(frame));
}

TEST_F(SurfaceAggregatorWithResourcesTest, TakeResourcesOneSurface) {
  cc::FakeCompositorFrameSinkSupportClient client;
  auto support = CompositorFrameSinkSupport::Create(
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(support->frame_sink_id(), local_surface_id);

  ResourceId ids[] = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, arraysize(ids), true, SurfaceId(),
                                     support.get(), surface_id);

  cc::CompositorFrame frame = aggregator_->Aggregate(surface_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  SubmitCompositorFrameWithResources(NULL, 0u, true, SurfaceId(), support.get(),
                                     surface_id);

  frame = aggregator_->Aggregate(surface_id);

  ASSERT_EQ(3u, client.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = client.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));

  support->EvictCurrentSurface();
}

// This test verifies that when a CompositorFrame is submitted to a new surface
// ID, and a new display frame is generated, then the resources of the old
// surface are returned to the appropriate client.
TEST_F(SurfaceAggregatorWithResourcesTest, ReturnResourcesAsSurfacesChange) {
  cc::FakeCompositorFrameSinkSupportClient client;
  auto support = CompositorFrameSinkSupport::Create(
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId local_surface_id1(7u, base::UnguessableToken::Create());
  LocalSurfaceId local_surface_id2(8u, base::UnguessableToken::Create());
  SurfaceId surface_id1(support->frame_sink_id(), local_surface_id1);
  SurfaceId surface_id2(support->frame_sink_id(), local_surface_id2);

  ResourceId ids[] = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, arraysize(ids), true, SurfaceId(),
                                     support.get(), surface_id1);

  cc::CompositorFrame frame = aggregator_->Aggregate(surface_id1);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  // Submitting a CompositorFrame to |surface_id2| should cause the surface
  // associated with |surface_id1| to get garbage collected.
  SubmitCompositorFrameWithResources(NULL, 0u, true, SurfaceId(), support.get(),
                                     surface_id2);

  frame = aggregator_->Aggregate(surface_id2);

  ASSERT_EQ(3u, client.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = client.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));

  support->EvictCurrentSurface();
}

TEST_F(SurfaceAggregatorWithResourcesTest, TakeInvalidResources) {
  cc::FakeCompositorFrameSinkSupportClient client;
  auto support = CompositorFrameSinkSupport::Create(
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(support->frame_sink_id(), local_surface_id);

  cc::CompositorFrame frame = test::MakeCompositorFrame();
  TransferableResource resource;
  resource.id = 11;
  // ResourceProvider is software but resource is not, so it should be
  // ignored.
  resource.is_software = false;
  frame.resource_list.push_back(resource);
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));

  cc::CompositorFrame returned_frame = aggregator_->Aggregate(surface_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  SubmitCompositorFrameWithResources(NULL, 0, true, SurfaceId(), support.get(),
                                     surface_id);
  ASSERT_EQ(1u, client.returned_resources().size());
  EXPECT_EQ(11u, client.returned_resources()[0].id);

  support->EvictCurrentSurface();
}

TEST_F(SurfaceAggregatorWithResourcesTest, TwoSurfaces) {
  cc::FakeCompositorFrameSinkSupportClient client;
  auto support1 = CompositorFrameSinkSupport::Create(
      &client, &manager_, FrameSinkId(1, 1), kChildIsRoot, kNeedsSyncPoints);
  auto support2 = CompositorFrameSinkSupport::Create(
      &client, &manager_, FrameSinkId(2, 2), kChildIsRoot, kNeedsSyncPoints);
  LocalSurfaceId local_frame1_id(7u, base::UnguessableToken::Create());
  SurfaceId surface1_id(support1->frame_sink_id(), local_frame1_id);

  LocalSurfaceId local_frame2_id(8u, base::UnguessableToken::Create());
  SurfaceId surface2_id(support2->frame_sink_id(), local_frame2_id);

  ResourceId ids[] = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, arraysize(ids), true, SurfaceId(),
                                     support1.get(), surface1_id);
  ResourceId ids2[] = {14, 15, 16};
  SubmitCompositorFrameWithResources(ids2, arraysize(ids2), true, SurfaceId(),
                                     support2.get(), surface2_id);

  cc::CompositorFrame frame = aggregator_->Aggregate(surface1_id);

  SubmitCompositorFrameWithResources(NULL, 0, true, SurfaceId(), support1.get(),
                                     surface1_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  frame = aggregator_->Aggregate(surface2_id);

  // surface1_id wasn't referenced, so its resources should be returned.
  ASSERT_EQ(3u, client.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = client.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));
  EXPECT_EQ(3u, resource_provider_->num_resources());

  support1->EvictCurrentSurface();
  support2->EvictCurrentSurface();
}

// Ensure that aggregator completely ignores Surfaces that reference invalid
// resources.
TEST_F(SurfaceAggregatorWithResourcesTest, InvalidChildSurface) {
  auto root_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  auto middle_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  auto child_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId root_local_surface_id(7u, kArbitraryToken);
  SurfaceId root_surface_id(root_support->frame_sink_id(),
                            root_local_surface_id);
  LocalSurfaceId middle_local_surface_id(8u, kArbitraryToken);
  SurfaceId middle_surface_id(middle_support->frame_sink_id(),
                              middle_local_surface_id);
  LocalSurfaceId child_local_surface_id(9u, kArbitraryToken);
  SurfaceId child_surface_id(child_support->frame_sink_id(),
                             child_local_surface_id);

  ResourceId ids[] = {14, 15, 16};
  SubmitCompositorFrameWithResources(ids, arraysize(ids), true, SurfaceId(),
                                     child_support.get(), child_surface_id);

  ResourceId ids2[] = {17, 18, 19};
  SubmitCompositorFrameWithResources(ids2, arraysize(ids2), false,
                                     child_surface_id, middle_support.get(),
                                     middle_surface_id);

  ResourceId ids3[] = {20, 21, 22};
  SubmitCompositorFrameWithResources(ids3, arraysize(ids3), true,
                                     middle_surface_id, root_support.get(),
                                     root_surface_id);

  cc::CompositorFrame frame;
  frame = aggregator_->Aggregate(root_surface_id);

  auto* pass_list = &frame.render_pass_list;
  ASSERT_EQ(1u, pass_list->size());
  EXPECT_EQ(1u, pass_list->back()->shared_quad_state_list.size());
  EXPECT_EQ(3u, pass_list->back()->quad_list.size());
  SubmitCompositorFrameWithResources(ids2, arraysize(ids), true,
                                     child_surface_id, middle_support.get(),
                                     middle_surface_id);

  frame = aggregator_->Aggregate(root_surface_id);

  pass_list = &frame.render_pass_list;
  ASSERT_EQ(1u, pass_list->size());
  EXPECT_EQ(3u, pass_list->back()->shared_quad_state_list.size());
  EXPECT_EQ(9u, pass_list->back()->quad_list.size());

  root_support->EvictCurrentSurface();
  middle_support->EvictCurrentSurface();
  child_support->EvictCurrentSurface();
}

TEST_F(SurfaceAggregatorWithResourcesTest, SecureOutputTexture) {
  auto support1 = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, FrameSinkId(1, 1), kChildIsRoot, kNeedsSyncPoints);
  auto support2 = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, FrameSinkId(2, 2), kChildIsRoot, kNeedsSyncPoints);
  LocalSurfaceId local_frame1_id(7u, base::UnguessableToken::Create());
  SurfaceId surface1_id(support1->frame_sink_id(), local_frame1_id);

  LocalSurfaceId local_frame2_id(8u, base::UnguessableToken::Create());
  SurfaceId surface2_id(support2->frame_sink_id(), local_frame2_id);

  ResourceId ids[] = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, arraysize(ids), true, SurfaceId(),
                                     support1.get(), surface1_id);

  cc::CompositorFrame frame = aggregator_->Aggregate(surface1_id);

  auto* render_pass = frame.render_pass_list.back().get();

  EXPECT_EQ(DrawQuad::TEXTURE_CONTENT, render_pass->quad_list.back()->material);

  {
    auto pass = cc::RenderPass::Create();
    pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;
    auto* surface_quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();

    surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                         surface1_id, cc::SurfaceDrawQuadType::PRIMARY,
                         nullptr);
    pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

    cc::CompositorFrame frame = test::MakeEmptyCompositorFrame();
    frame.render_pass_list.push_back(std::move(pass));

    support2->SubmitCompositorFrame(local_frame2_id, std::move(frame));
  }

  frame = aggregator_->Aggregate(surface2_id);
  EXPECT_EQ(1u, frame.render_pass_list.size());
  render_pass = frame.render_pass_list.front().get();

  // Parent has copy request, so texture should not be drawn.
  EXPECT_EQ(DrawQuad::SOLID_COLOR, render_pass->quad_list.back()->material);

  frame = aggregator_->Aggregate(surface2_id);
  EXPECT_EQ(1u, frame.render_pass_list.size());
  render_pass = frame.render_pass_list.front().get();

  // Copy request has been executed earlier, so texture should be drawn.
  EXPECT_EQ(DrawQuad::TEXTURE_CONTENT,
            render_pass->quad_list.front()->material);

  aggregator_->set_output_is_secure(false);

  frame = aggregator_->Aggregate(surface2_id);
  render_pass = frame.render_pass_list.back().get();

  // Output is insecure, so texture should be drawn.
  EXPECT_EQ(DrawQuad::SOLID_COLOR, render_pass->quad_list.back()->material);

  support1->EvictCurrentSurface();
  support2->EvictCurrentSurface();
}

// Ensure that the render passes have correct color spaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, ColorSpaceTest) {
  Quad quads[][2] = {{Quad::SolidColorQuad(SK_ColorWHITE),
                      Quad::SolidColorQuad(SK_ColorLTGRAY)},
                     {Quad::SolidColorQuad(SK_ColorGRAY),
                      Quad::SolidColorQuad(SK_ColorDKGRAY)}};
  Pass passes[] = {Pass(quads[0], arraysize(quads[0]), 2),
                   Pass(quads[1], arraysize(quads[1]), 1)};
  gfx::ColorSpace color_space1 = gfx::ColorSpace::CreateXYZD50();
  gfx::ColorSpace color_space2 = gfx::ColorSpace::CreateSRGB();
  gfx::ColorSpace color_space3 = gfx::ColorSpace::CreateSCRGBLinear();

  SubmitCompositorFrame(support_.get(), passes, arraysize(passes),
                        root_local_surface_id_);

  SurfaceId surface_id(support_->frame_sink_id(), root_local_surface_id_);

  cc::CompositorFrame aggregated_frame;
  aggregator_.SetOutputColorSpace(color_space1, color_space1);
  aggregated_frame = aggregator_.Aggregate(surface_id);
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[0]->color_space);
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[1]->color_space);

  aggregator_.SetOutputColorSpace(color_space2, color_space2);
  aggregated_frame = aggregator_.Aggregate(surface_id);
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
  EXPECT_EQ(color_space2, aggregated_frame.render_pass_list[0]->color_space);
  EXPECT_EQ(color_space2, aggregated_frame.render_pass_list[1]->color_space);

  aggregator_.SetOutputColorSpace(color_space1, color_space3);
  aggregated_frame = aggregator_.Aggregate(surface_id);
  EXPECT_EQ(3u, aggregated_frame.render_pass_list.size());
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[0]->color_space);
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[1]->color_space);
  EXPECT_EQ(color_space3, aggregated_frame.render_pass_list[2]->color_space);
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// child surface quads.
TEST_F(SurfaceAggregatorValidSurfaceTest, HasDamageByChangingChildSurface) {
  Quad child_surface_quads[] = {Quad::RenderPassQuad(1)};
  Pass child_surface_passes[] = {
      Pass(child_surface_quads, arraysize(child_surface_quads), 1)};

  cc::CompositorFrame child_surface_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, gfx::Rect(SurfaceSize()),
            child_surface_passes, arraysize(child_surface_passes));

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_surface_frame));

  Quad root_surface_quads[] = {
      Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
  Pass root_passes[] = {
      Pass(root_surface_quads, arraysize(root_surface_quads), 1)};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  // No Surface changed, so no damage should be given.
  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Change child_frame with damage should set the flag.
  {
    cc::CompositorFrame child_surface_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              child_surface_passes, arraysize(child_surface_passes));
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_surface_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    // True for new child_frame with damage.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // Change child_frame without damage should not set the flag.
  {
    cc::CompositorFrame child_surface_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              child_surface_passes, arraysize(child_surface_passes));
    child_surface_frame.render_pass_list[0]->damage_rect = gfx::Rect();
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_surface_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    // False for new child_frame without damage.
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// grand child surface quads.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       HasDamageByChangingGrandChildSurface) {
  auto grand_child_support = CompositorFrameSinkSupport::Create(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);

  Quad child_surface_quads[] = {Quad::RenderPassQuad(1)};
  Pass child_surface_passes[] = {
      Pass(child_surface_quads, arraysize(child_surface_quads), 1)};

  cc::CompositorFrame child_surface_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, gfx::Rect(SurfaceSize()),
            child_surface_passes, arraysize(child_surface_passes));

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_surface_frame));

  Quad root_surface_quads[] = {
      Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
  Pass root_passes[] = {
      Pass(root_surface_quads, arraysize(root_surface_quads), 1)};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  // No Surface changed, so no damage should be given.
  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Add a grand_child_frame should cause damage.
  Quad grand_child_quads[] = {Quad::RenderPassQuad(1)};
  Pass grand_child_passes[] = {
      Pass(grand_child_quads, arraysize(grand_child_quads), 1)};
  LocalSurfaceId grand_child_local_surface_id = allocator_.GenerateId();
  SurfaceId grand_child_surface_id(grand_child_support->frame_sink_id(),
                                   grand_child_local_surface_id);
  {
    cc::CompositorFrame grand_child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              grand_child_passes, arraysize(grand_child_passes));

    grand_child_support->SubmitCompositorFrame(grand_child_local_surface_id,
                                               std::move(grand_child_frame));

    Quad new_child_surface_quads[] = {
        child_surface_quads[0],
        Quad::SurfaceQuad(grand_child_surface_id, InvalidSurfaceId(), 1.f)};
    Pass new_child_surface_passes[] = {
        Pass(new_child_surface_quads, arraysize(new_child_surface_quads), 1)};
    child_surface_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              new_child_surface_passes, arraysize(new_child_surface_passes));
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_surface_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    // True for new grand_child_frame.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // No Surface changed, so no damage should be given.
  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Change grand_child_frame with damage should set the flag.
  {
    cc::CompositorFrame grand_child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              grand_child_passes, arraysize(grand_child_passes));
    grand_child_support->SubmitCompositorFrame(grand_child_local_surface_id,
                                               std::move(grand_child_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    // True for new grand_child_frame with damage.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // Change grand_child_frame without damage should not set the flag.
  {
    cc::CompositorFrame grand_child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              grand_child_passes, arraysize(grand_child_passes));
    grand_child_frame.render_pass_list[0]->damage_rect = gfx::Rect();
    grand_child_support->SubmitCompositorFrame(grand_child_local_surface_id,
                                               std::move(grand_child_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    // False for new grand_child_frame without damage.
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  grand_child_support->EvictCurrentSurface();
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// render pass quads.
TEST_F(SurfaceAggregatorValidSurfaceTest, HasDamageFromRenderPassQuads) {
  Quad child_quads[] = {Quad::RenderPassQuad(1)};
  Pass child_passes[] = {Pass(child_quads, arraysize(child_quads), 1)};

  cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
            child_passes, arraysize(child_passes));

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  Quad root_surface_quads[] = {
      Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};
  Quad root_render_pass_quads[] = {Quad::RenderPassQuad(1)};

  Pass root_passes[] = {
      Pass(root_surface_quads, arraysize(root_surface_quads), 1),
      Pass(root_render_pass_quads, arraysize(root_render_pass_quads), 2)};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // No Surface changed, so no damage should be given.
  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
    EXPECT_FALSE(aggregated_frame.render_pass_list[1]
                     ->has_damage_from_contributing_content);
  }

  // Changing child_frame should damage both render_pass.
  {
    cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              child_passes, arraysize(child_passes));
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    // True for new child_frame.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
    EXPECT_TRUE(aggregated_frame.render_pass_list[1]
                    ->has_damage_from_contributing_content);
  }
}

// Tests that the first frame damage_rect of a cached render pass should be
// fully damaged.
TEST_F(SurfaceAggregatorValidSurfaceTest, DamageRectOfCachedRenderPass) {
  int pass_id[] = {1, 2};
  Quad root_quads[][1] = {
      {Quad::SolidColorQuad(SK_ColorGREEN)}, {Quad::RenderPassQuad(pass_id[0])},
  };
  Pass root_passes[] = {
      Pass(root_quads[0], arraysize(root_quads[0]), pass_id[0]),
      Pass(root_quads[1], arraysize(root_quads[1]), pass_id[1])};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // The root surface was enqueued without being aggregated once, so it should
  // be treated as completely damaged.
  EXPECT_TRUE(
      aggregated_pass_list[0]->damage_rect.Contains(gfx::Rect(SurfaceSize())));
  EXPECT_TRUE(
      aggregated_pass_list[1]->damage_rect.Contains(gfx::Rect(SurfaceSize())));

  // For offscreen render pass, only the visible area is damaged.
  {
    cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              root_passes, arraysize(root_passes));

    auto* nonroot_pass = root_frame.render_pass_list[0].get();
    nonroot_pass->transform_to_root_target.Translate(8, 0);

    gfx::Rect root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* root_pass = root_frame.render_pass_list[1].get();
    root_pass->damage_rect = root_pass_damage;
    auto* root_pass_sqs = root_pass->shared_quad_state_list.front();
    root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    support_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Only the visible area is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 2, 10), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }

  // For offscreen cached render pass, should have full damage.
  {
    cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              root_passes, arraysize(root_passes));

    auto* nonroot_pass = root_frame.render_pass_list[0].get();
    nonroot_pass->transform_to_root_target.Translate(8, 0);
    nonroot_pass->cache_render_pass = true;

    gfx::Rect root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* root_pass = root_frame.render_pass_list[1].get();
    root_pass->damage_rect = root_pass_damage;
    auto* root_pass_sqs = root_pass->shared_quad_state_list.front();
    root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    support_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Should have full damage.
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }
}

// Tests that the first frame damage_rect of cached render pass of a child
// surface should be fully damaged.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       DamageRectOfCachedRenderPassInChildSurface) {
  int pass_id[] = {1, 2};
  Quad child_quads[][1] = {
      {Quad::SolidColorQuad(SK_ColorGREEN)}, {Quad::RenderPassQuad(pass_id[0])},
  };
  Pass child_passes[] = {
      Pass(child_quads[0], arraysize(child_quads[0]), pass_id[0]),
      Pass(child_quads[1], arraysize(child_quads[1]), pass_id[1])};

  cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
            child_passes, arraysize(child_passes));

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  Quad root_surface_quads[] = {
      Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)};

  Pass root_passes[] = {
      Pass(root_surface_quads, arraysize(root_surface_quads), 1)};

  cc::CompositorFrame root_frame = test::MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, gfx::Rect(SurfaceSize()), root_passes,
            arraysize(root_passes));

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // The root surface was enqueued without being aggregated once, so it should
  // be treated as completely damaged.
  EXPECT_TRUE(
      aggregated_pass_list[0]->damage_rect.Contains(gfx::Rect(SurfaceSize())));
  EXPECT_TRUE(
      aggregated_pass_list[1]->damage_rect.Contains(gfx::Rect(SurfaceSize())));

  // For offscreen render pass, only the visible area is damaged.
  {
    cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              child_passes, arraysize(child_passes));

    auto* child_nonroot_pass = child_frame.render_pass_list[0].get();
    child_nonroot_pass->transform_to_root_target.Translate(8, 0);

    gfx::Rect child_root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* child_root_pass = child_frame.render_pass_list[1].get();
    child_root_pass->damage_rect = child_root_pass_damage;
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Only the visible area is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 2, 10), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(child_root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }

  // For offscreen cached render pass, should have full damage.
  {
    cc::CompositorFrame child_frame = test::MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, gfx::Rect(SurfaceSize()),
              child_passes, arraysize(child_passes));

    auto* child_nonroot_pass = child_frame.render_pass_list[0].get();
    child_nonroot_pass->transform_to_root_target.Translate(8, 0);
    child_nonroot_pass->cache_render_pass = true;

    gfx::Rect child_root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* child_root_pass = child_frame.render_pass_list[1].get();
    child_root_pass->damage_rect = child_root_pass_damage;
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Should have full damage.
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(child_root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }
}

// Tests that quads outside the damage rect are not ignored for cached render
// pass.
TEST_F(SurfaceAggregatorPartialSwapTest, NotIgnoreOutsideForCachedRenderPass) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  // The child surface has two quads, one with a visible rect of 15,15 6x6 and
  // the other other with a visible rect of 10,10 2x2 (relative to root target
  // space).
  {
    int pass_id[] = {1, 2};
    Quad child_quads[][1] = {
        {Quad::SolidColorQuad(SK_ColorGREEN)},
        {Quad::RenderPassQuad(pass_id[0])},
    };
    Pass child_passes[] = {
        Pass(child_quads[0], arraysize(child_quads[0]), pass_id[0]),
        Pass(child_quads[1], arraysize(child_quads[1]), pass_id[1])};

    cc::RenderPassList child_pass_list;
    AddPasses(&child_pass_list, gfx::Rect(SurfaceSize()), child_passes,
              arraysize(child_passes));

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 3, 3);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(3, 3);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[0]->cache_render_pass = true;

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    SubmitPassListAsFrame(child_support_.get(), child_local_surface_id,
                          &child_pass_list);
  }

  {
    int pass_id[] = {1, 2};
    Quad root_quads[][1] = {
        {Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)},
        {Quad::RenderPassQuad(pass_id[0])},
    };
    Pass root_passes[] = {
        Pass(root_quads[0], arraysize(root_quads[0]), pass_id[0]),
        Pass(root_quads[1], arraysize(root_quads[1]), pass_id[1])};

    cc::RenderPassList root_pass_list;
    AddPasses(&root_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(0, 0, 1, 1);

    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list);
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  cc::CompositorFrame aggregated_frame = aggregator_.Aggregate(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());

  // Damage rect for first aggregation should contain entire root surface.
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
  EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());

  // Test should not ignore outside for cached render pass.
  // Create a root surface with a smaller damage rect.
  {
    int pass_id[] = {1, 2};
    Quad root_quads[][1] = {
        {Quad::SurfaceQuad(child_surface_id, InvalidSurfaceId(), 1.f)},
        {Quad::RenderPassQuad(pass_id[0])},
    };
    Pass root_passes[] = {
        Pass(root_quads[0], arraysize(root_quads[0]), pass_id[0]),
        Pass(root_quads[1], arraysize(root_quads[1]), pass_id[1])};

    cc::RenderPassList root_pass_list;
    AddPasses(&root_pass_list, gfx::Rect(SurfaceSize()), root_passes,
              arraysize(root_passes));

    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list);
  }

  {
    cc::CompositorFrame aggregated_frame =
        aggregator_.Aggregate(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // The first quad is a cached render pass, should be included and fully
    // damaged.
    EXPECT_EQ(gfx::Rect(1, 1, 3, 3),
              aggregated_pass_list[0]->quad_list.back()->visible_rect);
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2),
              aggregated_pass_list[1]->quad_list.back()->visible_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }
}

}  // namespace
}  // namespace viz
