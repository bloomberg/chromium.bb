// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "cc/base/lap_timer.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/compositor_frame_helpers.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_context_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace viz {
namespace {

constexpr bool kIsRoot = true;
constexpr bool kIsChildRoot = false;
constexpr bool kHandlesFrameSinkIdInvalidation = true;
constexpr bool kNeedsSyncPoints = true;

const base::UnguessableToken kArbitraryToken = base::UnguessableToken::Create();

class SurfaceAggregatorPerfTest : public testing::Test {
 public:
  SurfaceAggregatorPerfTest() {
    context_provider_ = cc::TestContextProvider::Create();
    context_provider_->BindToCurrentThread();
    shared_bitmap_manager_ = base::MakeUnique<cc::TestSharedBitmapManager>();

    resource_provider_ = cc::FakeResourceProvider::Create(
        context_provider_.get(), shared_bitmap_manager_.get());
  }

  void RunTest(int num_surfaces,
               int num_textures,
               float opacity,
               bool optimize_damage,
               bool full_damage,
               const std::string& name) {
    std::vector<std::unique_ptr<CompositorFrameSinkSupport>> child_supports(
        num_surfaces);
    for (int i = 0; i < num_surfaces; i++) {
      child_supports[i] = CompositorFrameSinkSupport::Create(
          nullptr, &manager_, FrameSinkId(1, i + 1), kIsChildRoot,
          kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
    }
    aggregator_ = base::MakeUnique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), optimize_damage);
    for (int i = 0; i < num_surfaces; i++) {
      LocalSurfaceId local_surface_id(i + 1, kArbitraryToken);

      auto pass = cc::RenderPass::Create();
      pass->output_rect = gfx::Rect(0, 0, 1, 2);

      cc::CompositorFrame frame = cc::test::MakeEmptyCompositorFrame();

      auto* sqs = pass->CreateAndAppendSharedQuadState();
      for (int j = 0; j < num_textures; j++) {
        cc::TransferableResource resource;
        resource.id = j;
        resource.is_software = true;
        frame.resource_list.push_back(resource);

        auto* quad = pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
        const gfx::Rect rect(0, 0, 1, 2);
        const gfx::Rect opaque_rect;
        // Half of rects should be visible with partial damage.
        gfx::Rect visible_rect =
            j % 2 == 0 ? gfx::Rect(0, 0, 1, 2) : gfx::Rect(0, 1, 1, 1);
        bool needs_blending = false;
        bool premultiplied_alpha = false;
        const gfx::PointF uv_top_left;
        const gfx::PointF uv_bottom_right;
        SkColor background_color = SK_ColorGREEN;
        const float vertex_opacity[4] = {0.f, 0.f, 1.f, 1.f};
        bool flipped = false;
        bool nearest_neighbor = false;
        quad->SetAll(sqs, rect, opaque_rect, visible_rect, needs_blending, j,
                     gfx::Size(), premultiplied_alpha, uv_top_left,
                     uv_bottom_right, background_color, vertex_opacity, flipped,
                     nearest_neighbor, false);
      }
      sqs = pass->CreateAndAppendSharedQuadState();
      sqs->opacity = opacity;
      if (i >= 1) {
        auto* surface_quad =
            pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
        surface_quad->SetNew(
            sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
            SurfaceId(FrameSinkId(1, i), LocalSurfaceId(i, kArbitraryToken)),
            cc::SurfaceDrawQuadType::PRIMARY, nullptr);
      }

      frame.render_pass_list.push_back(std::move(pass));
      child_supports[i]->SubmitCompositorFrame(local_surface_id,
                                               std::move(frame));
    }

    auto root_support = CompositorFrameSinkSupport::Create(
        nullptr, &manager_, FrameSinkId(1, num_surfaces + 1), kIsRoot,
        kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
    timer_.Reset();
    do {
      auto pass = cc::RenderPass::Create();
      cc::CompositorFrame frame = cc::test::MakeEmptyCompositorFrame();

      auto* sqs = pass->CreateAndAppendSharedQuadState();
      auto* surface_quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
      surface_quad->SetNew(
          sqs, gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 0, 100, 100),
          SurfaceId(FrameSinkId(1, num_surfaces),
                    LocalSurfaceId(num_surfaces, kArbitraryToken)),
          cc::SurfaceDrawQuadType::PRIMARY, nullptr);

      pass->output_rect = gfx::Rect(0, 0, 100, 100);

      if (full_damage)
        pass->damage_rect = gfx::Rect(0, 0, 100, 100);
      else
        pass->damage_rect = gfx::Rect(0, 0, 1, 1);

      frame.render_pass_list.push_back(std::move(pass));

      root_support->SubmitCompositorFrame(
          LocalSurfaceId(num_surfaces + 1, kArbitraryToken), std::move(frame));

      cc::CompositorFrame aggregated = aggregator_->Aggregate(
          SurfaceId(FrameSinkId(1, num_surfaces + 1),
                    LocalSurfaceId(num_surfaces + 1, kArbitraryToken)));
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PrintResult("aggregator_speed", "", name, timer_.LapsPerSecond(),
                           "runs/s", true);
    for (int i = 0; i < num_surfaces; i++)
      child_supports[i]->EvictCurrentSurface();
    root_support->EvictCurrentSurface();
  }

 protected:
  FrameSinkManagerImpl manager_;
  scoped_refptr<cc::TestContextProvider> context_provider_;
  std::unique_ptr<SharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<cc::ResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
  cc::LapTimer timer_;
};

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaque) {
  RunTest(20, 100, 1.f, false, true, "many_surfaces_opaque");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparent) {
  RunTest(20, 100, .5f, false, true, "many_surfaces_transparent");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfaces) {
  RunTest(3, 1000, 1.f, false, true, "few_surfaces");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesOpaqueDamageCalc) {
  RunTest(20, 100, 1.f, true, true, "many_surfaces_opaque_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, ManySurfacesTransparentDamageCalc) {
  RunTest(20, 100, .5f, true, true, "many_surfaces_transparent_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesDamageCalc) {
  RunTest(3, 1000, 1.f, true, true, "few_surfaces_damage_calc");
}

TEST_F(SurfaceAggregatorPerfTest, FewSurfacesAggregateDamaged) {
  RunTest(3, 1000, 1.f, true, false, "few_surfaces_aggregate_damaged");
}

}  // namespace
}  // namespace viz
