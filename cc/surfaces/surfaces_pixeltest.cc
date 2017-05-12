// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/compositor_frame_sink_support.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/compositor_frame_helpers.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

constexpr FrameSinkId kArbitraryRootFrameSinkId(1, 1);
constexpr FrameSinkId kArbitraryChildFrameSinkId(2, 2);
constexpr FrameSinkId kArbitraryLeftFrameSinkId(3, 3);
constexpr FrameSinkId kArbitraryRightFrameSinkId(4, 4);
constexpr bool kIsRoot = true;
constexpr bool kIsChildRoot = false;
constexpr bool kHandlesFrameSinkIdInvalidation = true;
constexpr bool kNeedsSyncPoints = true;

class SurfacesPixelTest : public RendererPixelTest<GLRenderer> {
 public:
  SurfacesPixelTest()
      : support_(
            CompositorFrameSinkSupport::Create(nullptr,
                                               &manager_,
                                               kArbitraryRootFrameSinkId,
                                               kIsRoot,
                                               kHandlesFrameSinkIdInvalidation,
                                               kNeedsSyncPoints)) {}
  ~SurfacesPixelTest() override { support_->EvictCurrentSurface(); }

 protected:
  SurfaceManager manager_;
  LocalSurfaceIdAllocator allocator_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
};

SharedQuadState* CreateAndAppendTestSharedQuadState(
    RenderPass* render_pass,
    const gfx::Transform& transform,
    const gfx::Size& size) {
  const gfx::Rect layer_rect = gfx::Rect(size);
  const gfx::Rect visible_layer_rect = gfx::Rect(size);
  const gfx::Rect clip_rect = gfx::Rect(size);
  bool is_clipped = false;
  float opacity = 1.f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(transform, layer_rect, visible_layer_rect, clip_rect,
                       is_clipped, opacity, blend_mode, 0);
  return shared_state;
}

// Draws a very simple frame with no surface references.
TEST_F(SurfacesPixelTest, DrawSimpleFrame) {
  gfx::Rect rect(device_viewport_size_);
  int id = 1;
  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(id, rect, rect, gfx::Transform());

  CreateAndAppendTestSharedQuadState(
      pass.get(), gfx::Transform(), device_viewport_size_);

  SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bool force_anti_aliasing_off = false;
  color_quad->SetNew(pass->shared_quad_state_list.back(),
                     rect,
                     rect,
                     SK_ColorGREEN,
                     force_anti_aliasing_off);

  CompositorFrame root_frame = test::MakeCompositorFrame();
  root_frame.render_pass_list.push_back(std::move(pass));

  LocalSurfaceId root_local_surface_id = allocator_.GenerateId();
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id);
  support_->SubmitCompositorFrame(root_local_surface_id, std::move(root_frame));

  SurfaceAggregator aggregator(&manager_, resource_provider_.get(), true);
  CompositorFrame aggregated_frame = aggregator.Aggregate(root_surface_id);

  bool discard_alpha = false;
  ExactPixelComparator pixel_comparator(discard_alpha);
  RenderPassList* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(RunPixelTest(pass_list,
                           base::FilePath(FILE_PATH_LITERAL("green.png")),
                           pixel_comparator));
}

// Draws a frame with simple surface embedding.
TEST_F(SurfacesPixelTest, DrawSimpleAggregatedFrame) {
  gfx::Size child_size(200, 100);
  std::unique_ptr<CompositorFrameSinkSupport> child_support =
      CompositorFrameSinkSupport::Create(
          nullptr, &manager_, kArbitraryChildFrameSinkId, kIsChildRoot,
          kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support->frame_sink_id(),
                             child_local_surface_id);
  LocalSurfaceId root_local_surface_id = allocator_.GenerateId();
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id);

  {
    gfx::Rect rect(device_viewport_size_);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), device_viewport_size_);

    SurfaceDrawQuad* surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetNew(pass->shared_quad_state_list.back(),
                         gfx::Rect(child_size), gfx::Rect(child_size),
                         child_surface_id, SurfaceDrawQuadType::PRIMARY,
                         nullptr);

    SolidColorDrawQuad* color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    color_quad->SetNew(pass->shared_quad_state_list.back(),
                       rect,
                       rect,
                       SK_ColorYELLOW,
                       force_anti_aliasing_off);

    CompositorFrame root_frame = test::MakeCompositorFrame();
    root_frame.render_pass_list.push_back(std::move(pass));

    support_->SubmitCompositorFrame(root_local_surface_id,
                                    std::move(root_frame));
  }

  {
    gfx::Rect rect(child_size);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), child_size);

    SolidColorDrawQuad* color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    color_quad->SetNew(pass->shared_quad_state_list.back(),
                       rect,
                       rect,
                       SK_ColorBLUE,
                       force_anti_aliasing_off);

    CompositorFrame child_frame = test::MakeCompositorFrame();
    child_frame.render_pass_list.push_back(std::move(pass));

    child_support->SubmitCompositorFrame(child_local_surface_id,
                                         std::move(child_frame));
  }

  SurfaceAggregator aggregator(&manager_, resource_provider_.get(), true);
  CompositorFrame aggregated_frame = aggregator.Aggregate(root_surface_id);

  bool discard_alpha = false;
  ExactPixelComparator pixel_comparator(discard_alpha);
  RenderPassList* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(RunPixelTest(pass_list,
                           base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
                           pixel_comparator));

  child_support->EvictCurrentSurface();
}

// Tests a surface quad that has a non-identity transform into its pass.
TEST_F(SurfacesPixelTest, DrawAggregatedFrameWithSurfaceTransforms) {
  gfx::Size child_size(100, 200);
  gfx::Size quad_size(100, 100);
  // Structure:
  // root (200x200) -> left_child (100x200 @ 0x0,
  //                   right_child (100x200 @ 0x100)
  //   left_child -> top_green_quad (100x100 @ 0x0),
  //                 bottom_blue_quad (100x100 @ 0x100)
  //   right_child -> top_blue_quad (100x100 @ 0x0),
  //                  bottom_green_quad (100x100 @ 0x100)
  std::unique_ptr<CompositorFrameSinkSupport> left_support =
      CompositorFrameSinkSupport::Create(
          nullptr, &manager_, kArbitraryLeftFrameSinkId, kIsChildRoot,
          kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  std::unique_ptr<CompositorFrameSinkSupport> right_support =
      CompositorFrameSinkSupport::Create(
          nullptr, &manager_, kArbitraryRightFrameSinkId, kIsChildRoot,
          kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints);
  LocalSurfaceId left_child_local_id = allocator_.GenerateId();
  SurfaceId left_child_id(left_support->frame_sink_id(), left_child_local_id);
  LocalSurfaceId right_child_local_id = allocator_.GenerateId();
  SurfaceId right_child_id(right_support->frame_sink_id(),
                           right_child_local_id);
  LocalSurfaceId root_local_surface_id = allocator_.GenerateId();
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id);

  {
    gfx::Rect rect(device_viewport_size_);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    gfx::Transform surface_transform;
    CreateAndAppendTestSharedQuadState(
        pass.get(), surface_transform, device_viewport_size_);

    SurfaceDrawQuad* left_surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    left_surface_quad->SetNew(pass->shared_quad_state_list.back(),
                              gfx::Rect(child_size), gfx::Rect(child_size),
                              left_child_id, SurfaceDrawQuadType::PRIMARY,
                              nullptr);

    surface_transform.Translate(100, 0);
    CreateAndAppendTestSharedQuadState(
        pass.get(), surface_transform, device_viewport_size_);

    SurfaceDrawQuad* right_surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    right_surface_quad->SetNew(pass->shared_quad_state_list.back(),
                               gfx::Rect(child_size), gfx::Rect(child_size),
                               right_child_id, SurfaceDrawQuadType::PRIMARY,
                               nullptr);

    CompositorFrame root_frame = test::MakeCompositorFrame();
    root_frame.render_pass_list.push_back(std::move(pass));

    support_->SubmitCompositorFrame(root_local_surface_id,
                                    std::move(root_frame));
  }

  {
    gfx::Rect rect(child_size);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), child_size);

    SolidColorDrawQuad* top_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    top_color_quad->SetNew(pass->shared_quad_state_list.back(),
                           gfx::Rect(quad_size),
                           gfx::Rect(quad_size),
                           SK_ColorGREEN,
                           force_anti_aliasing_off);

    SolidColorDrawQuad* bottom_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_color_quad->SetNew(pass->shared_quad_state_list.back(),
                              gfx::Rect(0, 100, 100, 100),
                              gfx::Rect(0, 100, 100, 100),
                              SK_ColorBLUE,
                              force_anti_aliasing_off);

    CompositorFrame child_frame = test::MakeCompositorFrame();
    child_frame.render_pass_list.push_back(std::move(pass));

    left_support->SubmitCompositorFrame(left_child_local_id,
                                        std::move(child_frame));
  }

  {
    gfx::Rect rect(child_size);
    int id = 1;
    std::unique_ptr<RenderPass> pass = RenderPass::Create();
    pass->SetNew(id, rect, rect, gfx::Transform());

    CreateAndAppendTestSharedQuadState(
        pass.get(), gfx::Transform(), child_size);

    SolidColorDrawQuad* top_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bool force_anti_aliasing_off = false;
    top_color_quad->SetNew(pass->shared_quad_state_list.back(),
                           gfx::Rect(quad_size),
                           gfx::Rect(quad_size),
                           SK_ColorBLUE,
                           force_anti_aliasing_off);

    SolidColorDrawQuad* bottom_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    bottom_color_quad->SetNew(pass->shared_quad_state_list.back(),
                              gfx::Rect(0, 100, 100, 100),
                              gfx::Rect(0, 100, 100, 100),
                              SK_ColorGREEN,
                              force_anti_aliasing_off);

    CompositorFrame child_frame = test::MakeCompositorFrame();
    child_frame.render_pass_list.push_back(std::move(pass));

    right_support->SubmitCompositorFrame(right_child_local_id,
                                         std::move(child_frame));
  }

  SurfaceAggregator aggregator(&manager_, resource_provider_.get(), true);
  CompositorFrame aggregated_frame = aggregator.Aggregate(root_surface_id);

  bool discard_alpha = false;
  ExactPixelComparator pixel_comparator(discard_alpha);
  RenderPassList* pass_list = &aggregated_frame.render_pass_list;
  EXPECT_TRUE(RunPixelTest(
      pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      pixel_comparator));

  left_support->EvictCurrentSurface();
  right_support->EvictCurrentSurface();
}

}  // namespace
}  // namespace cc

#endif  // !defined(OS_ANDROID)
