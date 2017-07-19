// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "cc/output/compositor_frame.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_hittest.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/compositor_frame_helpers.h"
#include "cc/test/fake_compositor_frame_sink_support_client.h"
#include "cc/test/surface_hittest_test_helpers.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

namespace {

constexpr bool kIsRoot = true;
constexpr bool kIsChildRoot = false;
constexpr bool kHandlesFrameSinkIdInvalidation = true;
constexpr bool kNeedsSyncPoints = true;
constexpr viz::FrameSinkId kRootFrameSink(2, 0);
constexpr viz::FrameSinkId kChildFrameSink(65563, 0);
constexpr viz::FrameSinkId kArbitraryFrameSink(1337, 7331);

struct TestCase {
  viz::SurfaceId input_surface_id;
  gfx::Point input_point;
  viz::SurfaceId expected_layer_tree_frame_sink_id;
  gfx::Point expected_output_point;
};

void RunTests(SurfaceHittestDelegate* delegate,
              SurfaceManager* manager,
              TestCase* tests,
              size_t test_count) {
  SurfaceHittest hittest(delegate, manager);
  for (size_t i = 0; i < test_count; ++i) {
    const TestCase& test = tests[i];
    gfx::Point point(test.input_point);
    gfx::Transform transform;
    EXPECT_EQ(test.expected_layer_tree_frame_sink_id,
              hittest.GetTargetSurfaceAtPoint(test.input_surface_id, point,
                                              &transform));
    transform.TransformPoint(&point);
    EXPECT_EQ(test.expected_output_point, point);

    // Verify that GetTransformToTargetSurface returns true and returns the same
    // transform as returned by GetTargetSurfaceAtPoint.
    gfx::Transform target_transform;
    EXPECT_TRUE(hittest.GetTransformToTargetSurface(
        test.input_surface_id, test.expected_layer_tree_frame_sink_id,
        &target_transform));
    EXPECT_EQ(transform, target_transform);
  }
}

}  // namespace

using namespace test;

class SurfaceHittestTest : public testing::Test {
 public:
  SurfaceHittestTest()
      : frame_sink_manager_(nullptr /* display_provider */,
                            SurfaceManager::LifetimeType::REFERENCES) {}

  ~SurfaceHittestTest() override {}

  viz::CompositorFrameSinkSupport& root_support() { return *supports_[0]; }

  viz::CompositorFrameSinkSupport& child_support() { return *supports_[1]; }

  SurfaceManager* surface_manager() {
    return frame_sink_manager_.surface_manager();
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    supports_.push_back(viz::CompositorFrameSinkSupport::Create(
        &support_client_, &frame_sink_manager_, kRootFrameSink, kIsRoot,
        kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints));
    supports_.push_back(viz::CompositorFrameSinkSupport::Create(
        &support_client_, &frame_sink_manager_, kChildFrameSink, kIsChildRoot,
        kHandlesFrameSinkIdInvalidation, kNeedsSyncPoints));
  }

  void TearDown() override { supports_.clear(); }

 private:
  viz::FrameSinkManagerImpl frame_sink_manager_;
  std::vector<std::unique_ptr<viz::CompositorFrameSinkSupport>> supports_;
  FakeCompositorFrameSinkSupportClient support_client_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceHittestTest);
};

// This test verifies that hit testing on a surface that does not exist does
// not crash.
TEST_F(SurfaceHittestTest, Hittest_BadCompositorFrameDoesNotCrash) {
  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  CompositorFrame root_frame = CreateCompositorFrame(root_rect, &root_pass);

  // Add a reference to a non-existant child surface on the root surface.
  viz::SurfaceId child_surface_id(
      kArbitraryFrameSink,
      viz::LocalSurfaceId(0xdeadbeef, base::UnguessableToken::Create()));
  gfx::Rect child_rect(200, 200);
  CreateSurfaceDrawQuad(root_pass,
                        gfx::Transform(),
                        root_rect,
                        child_rect,
                        child_surface_id);

  // Submit the root frame.
  viz::LocalSurfaceIdAllocator root_allocator;
  viz::LocalSurfaceId root_local_surface_id = root_allocator.GenerateId();
  viz::SurfaceId root_surface_id(kRootFrameSink, root_local_surface_id);
  root_support().SubmitCompositorFrame(root_local_surface_id,
                                       std::move(root_frame));

  {
    SurfaceHittest hittest(nullptr, surface_manager());
    // It is expected this test will complete without crashes.
    gfx::Transform transform;
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(
                  root_surface_id, gfx::Point(100, 100), &transform));
  }

  root_support().EvictCurrentSurface();
}

TEST_F(SurfaceHittestTest, Hittest_SingleSurface) {
  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  CompositorFrame root_frame = CreateCompositorFrame(root_rect, &root_pass);

  // Submit the root frame.
  viz::LocalSurfaceIdAllocator root_allocator;
  viz::LocalSurfaceId root_local_surface_id = root_allocator.GenerateId();
  viz::SurfaceId root_surface_id(kRootFrameSink, root_local_surface_id);
  root_support().SubmitCompositorFrame(root_local_surface_id,
                                       std::move(root_frame));
  TestCase tests[] = {
    {
      root_surface_id,
      gfx::Point(100, 100),
      root_surface_id,
      gfx::Point(100, 100)
    },
  };

  RunTests(nullptr, surface_manager(), tests, arraysize(tests));

  root_support().EvictCurrentSurface();
}

TEST_F(SurfaceHittestTest, Hittest_ChildSurface) {
  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  CompositorFrame root_frame = CreateCompositorFrame(root_rect, &root_pass);

  // Add a reference to the child surface on the root surface.
  viz::LocalSurfaceIdAllocator child_allocator;
  viz::LocalSurfaceId child_local_surface_id = child_allocator.GenerateId();
  viz::SurfaceId child_surface_id(kChildFrameSink, child_local_surface_id);
  gfx::Rect child_rect(200, 200);
  CreateSurfaceDrawQuad(root_pass,
                        gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                        0.0f, 1.0f, 0.0f, 50.0f,
                                        0.0f, 0.0f, 1.0f, 0.0f,
                                        0.0f, 0.0f, 0.0f, 1.0f),
                        root_rect,
                        child_rect,
                        child_surface_id);

  // Submit the root frame.
  viz::LocalSurfaceIdAllocator root_allocator;
  viz::LocalSurfaceId root_local_surface_id = root_allocator.GenerateId();
  viz::SurfaceId root_surface_id(kRootFrameSink, root_local_surface_id);
  root_support().SubmitCompositorFrame(root_local_surface_id,
                                       std::move(root_frame));

  // Creates a child surface.
  RenderPass* child_pass = nullptr;
  CompositorFrame child_frame = CreateCompositorFrame(child_rect, &child_pass);

  // Add a solid quad in the child surface.
  gfx::Rect child_solid_quad_rect(100, 100);
  CreateSolidColorDrawQuad(
      child_pass,
      gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                     0.0f, 1.0f, 0.0f, 50.0f,
                     0.0f, 0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f),
      root_rect, child_solid_quad_rect);

  // Submit the frame.
  child_support().SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  TestCase tests[] = {
    {
      root_surface_id,
      gfx::Point(10, 10),
      root_surface_id,
      gfx::Point(10, 10)
    },
    {
      root_surface_id,
      gfx::Point(99, 99),
      root_surface_id,
      gfx::Point(99, 99)
    },
    {
      root_surface_id,
      gfx::Point(100, 100),
      child_surface_id,
      gfx::Point(50, 50)
    },
    {
      root_surface_id,
      gfx::Point(199, 199),
      child_surface_id,
      gfx::Point(149, 149)
    },
    {
      root_surface_id,
      gfx::Point(200, 200),
      root_surface_id,
      gfx::Point(200, 200)
    },
    {
      root_surface_id,
      gfx::Point(290, 290),
      root_surface_id,
      gfx::Point(290, 290)
    }
  };

  RunTests(nullptr, surface_manager(), tests, arraysize(tests));

  // Submit another root frame, with a slightly perturbed child Surface.
  root_frame = CreateCompositorFrame(root_rect, &root_pass);
  CreateSurfaceDrawQuad(root_pass,
                        gfx::Transform(1.0f, 0.0f, 0.0f, 75.0f,
                                        0.0f, 1.0f, 0.0f, 75.0f,
                                        0.0f, 0.0f, 1.0f, 0.0f,
                                        0.0f, 0.0f, 0.0f, 1.0f),
                        root_rect,
                        child_rect,
                        child_surface_id);
  root_support().SubmitCompositorFrame(root_local_surface_id,
                                       std::move(root_frame));

  // Verify that point (100, 100) no longer falls on the child surface.
  // Verify that the transform to the child surface's space has also shifted.
  {
    SurfaceHittest hittest(nullptr, surface_manager());

    gfx::Point point(100, 100);
    gfx::Transform transform;
    EXPECT_EQ(root_surface_id,
              hittest.GetTargetSurfaceAtPoint(root_surface_id, point,
                                              &transform));
    transform.TransformPoint(&point);
    EXPECT_EQ(gfx::Point(100, 100), point);

    gfx::Point point_in_target_space(100, 100);
    gfx::Transform target_transform;
    EXPECT_TRUE(hittest.GetTransformToTargetSurface(
        root_surface_id, child_surface_id, &target_transform));
    target_transform.TransformPoint(&point_in_target_space);
    EXPECT_NE(transform, target_transform);
    EXPECT_EQ(gfx::Point(25, 25), point_in_target_space);
  }

  root_support().EvictCurrentSurface();
  child_support().EvictCurrentSurface();
}

// This test verifies that hit testing will progress to the next quad if it
// encounters an invalid RenderPassDrawQuad for whatever reason.
TEST_F(SurfaceHittestTest, Hittest_InvalidRenderPassDrawQuad) {
  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  CompositorFrame root_frame = CreateCompositorFrame(root_rect, &root_pass);

  // Create a RenderPassDrawQuad to a non-existant RenderPass.
  int invalid_render_pass_id = 1337;
  CreateRenderPassDrawQuad(root_pass, gfx::Transform(), root_rect, root_rect,
                           invalid_render_pass_id);

  // Add a reference to the child surface on the root surface.
  viz::LocalSurfaceIdAllocator child_allocator;
  viz::LocalSurfaceId child_local_surface_id = child_allocator.GenerateId();
  viz::SurfaceId child_surface_id(kChildFrameSink, child_local_surface_id);
  gfx::Rect child_rect(200, 200);
  CreateSurfaceDrawQuad(root_pass,
                        gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                       0.0f, 1.0f, 0.0f, 50.0f,
                                       0.0f, 0.0f, 1.0f, 0.0f,
                                       0.0f, 0.0f, 0.0f, 1.0f),
                        root_rect,
                        child_rect,
                        child_surface_id);

  // Submit the root frame.
  viz::LocalSurfaceIdAllocator root_allocator;
  viz::LocalSurfaceId root_local_surface_id = root_allocator.GenerateId();
  viz::SurfaceId root_surface_id(kRootFrameSink, root_local_surface_id);
  root_support().SubmitCompositorFrame(root_local_surface_id,
                                       std::move(root_frame));

  // Creates a child surface.
  RenderPass* child_pass = nullptr;
  CompositorFrame child_frame = CreateCompositorFrame(child_rect, &child_pass);

  // Add a solid quad in the child surface.
  gfx::Rect child_solid_quad_rect(100, 100);
  CreateSolidColorDrawQuad(child_pass,
                           gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                                         0.0f, 1.0f, 0.0f, 50.0f,
                                         0.0f, 0.0f, 1.0f, 0.0f,
                                         0.0f, 0.0f, 0.0f, 1.0f),
                           root_rect,
                           child_solid_quad_rect);

  // Submit the frame.
  child_support().SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  TestCase tests[] = {
    {
      root_surface_id,
      gfx::Point(10, 10),
      root_surface_id,
      gfx::Point(10, 10)
    },
    {
      root_surface_id,
      gfx::Point(99, 99),
      root_surface_id,
      gfx::Point(99, 99)
    },
    {
      root_surface_id,
      gfx::Point(100, 100),
      child_surface_id,
      gfx::Point(50, 50)
    },
    {
      root_surface_id,
      gfx::Point(199, 199),
      child_surface_id,
      gfx::Point(149, 149)
    },
    {
      root_surface_id,
      gfx::Point(200, 200),
      root_surface_id,
      gfx::Point(200, 200)
    },
    {
      root_surface_id,
      gfx::Point(290, 290),
      root_surface_id,
      gfx::Point(290, 290)
    }
  };

  RunTests(nullptr, surface_manager(), tests, arraysize(tests));

  root_support().EvictCurrentSurface();
  child_support().EvictCurrentSurface();
}

TEST_F(SurfaceHittestTest, Hittest_RenderPassDrawQuad) {
  // Create a CompostiorFrame with two RenderPasses.
  gfx::Rect root_rect(300, 300);
  CompositorFrame root_frame = test::MakeCompositorFrame();
  RenderPassList& render_pass_list = root_frame.render_pass_list;

  // Create a child RenderPass.
  int child_render_pass_id = 3;
  gfx::Transform transform_to_root_target(1.0f, 0.0f, 0.0f, 50.0f,
                                          0.0f, 1.0f, 0.0f, 50.0f,
                                          0.0f, 0.0f, 1.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f, 1.0f);
  CreateRenderPass(child_render_pass_id,
                   gfx::Rect(100, 100),
                   transform_to_root_target,
                   &render_pass_list);

  // Create the root RenderPass.
  int root_render_pass_id = 2;
  CreateRenderPass(root_render_pass_id, root_rect, gfx::Transform(),
                   &render_pass_list);

  RenderPass* root_pass = nullptr;
  root_pass = root_frame.render_pass_list.back().get();

  // Create a RenderPassDrawQuad.
  gfx::Rect render_pass_quad_rect(100, 100);
  CreateRenderPassDrawQuad(root_pass,
                           transform_to_root_target,
                           root_rect,
                           render_pass_quad_rect,
                           child_render_pass_id);

  // Add a solid quad in the child render pass.
  RenderPass* child_render_pass = root_frame.render_pass_list.front().get();
  gfx::Rect child_solid_quad_rect(100, 100);
  CreateSolidColorDrawQuad(child_render_pass,
                           gfx::Transform(),
                           gfx::Rect(100, 100),
                           child_solid_quad_rect);

  // Submit the root frame.
  viz::LocalSurfaceIdAllocator root_allocator;
  viz::LocalSurfaceId root_local_surface_id = root_allocator.GenerateId();
  viz::SurfaceId root_surface_id(kRootFrameSink, root_local_surface_id);
  root_support().SubmitCompositorFrame(root_local_surface_id,
                                       std::move(root_frame));

  TestCase tests[] = {
    // These tests just miss the RenderPassDrawQuad.
    {
      root_surface_id,
      gfx::Point(49, 49),
      root_surface_id,
      gfx::Point(49, 49)
    },
    {
      root_surface_id,
      gfx::Point(150, 150),
      root_surface_id,
      gfx::Point(150, 150)
    },
    // These tests just hit the boundaries of the
    // RenderPassDrawQuad.
    {
      root_surface_id,
      gfx::Point(50, 50),
      root_surface_id,
      gfx::Point(50, 50)
    },
    {
      root_surface_id,
      gfx::Point(149, 149),
      root_surface_id,
      gfx::Point(149, 149)
    },
    // These tests fall somewhere in the center of the
    // RenderPassDrawQuad.
    {
      root_surface_id,
      gfx::Point(99, 99),
      root_surface_id,
      gfx::Point(99, 99)
    },
    {
      root_surface_id,
      gfx::Point(100, 100),
      root_surface_id,
      gfx::Point(100, 100)
    }
  };

  RunTests(nullptr, surface_manager(), tests, arraysize(tests));

  root_support().EvictCurrentSurface();
}

TEST_F(SurfaceHittestTest, Hittest_SingleSurface_WithInsetsDelegate) {
  // Creates a root surface.
  gfx::Rect root_rect(300, 300);
  RenderPass* root_pass = nullptr;
  CompositorFrame root_frame = CreateCompositorFrame(root_rect, &root_pass);

  // Add a reference to the child surface on the root surface.
  viz::LocalSurfaceIdAllocator child_allocator;
  viz::LocalSurfaceId child_local_surface_id = child_allocator.GenerateId();
  viz::SurfaceId child_surface_id(kChildFrameSink, child_local_surface_id);
  gfx::Rect child_rect(200, 200);
  CreateSurfaceDrawQuad(
      root_pass,
      gfx::Transform(1.0f, 0.0f, 0.0f, 50.0f,
                     0.0f, 1.0f, 0.0f, 50.0f,
                     0.0f, 0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f),
      root_rect, child_rect, child_surface_id);

  // Submit the root frame.
  viz::LocalSurfaceIdAllocator root_allocator;
  viz::LocalSurfaceId root_local_surface_id = root_allocator.GenerateId();
  viz::SurfaceId root_surface_id(kRootFrameSink, root_local_surface_id);
  root_support().SubmitCompositorFrame(root_local_surface_id,
                                       std::move(root_frame));

  // Creates a child surface.
  RenderPass* child_pass = nullptr;
  CompositorFrame child_frame = CreateCompositorFrame(child_rect, &child_pass);

  // Add a solid quad in the child surface.
  gfx::Rect child_solid_quad_rect(190, 190);
  CreateSolidColorDrawQuad(
      child_pass,
      gfx::Transform(1.0f, 0.0f, 0.0f, 5.0f, 0.0f, 1.0f, 0.0f, 5.0f, 0.0f, 0.0f,
                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f),
      root_rect, child_solid_quad_rect);

  // Submit the frame.
  child_support().SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  TestCase test_expectations_without_insets[] = {
      {root_surface_id, gfx::Point(55, 55), child_surface_id, gfx::Point(5, 5)},
      {root_surface_id, gfx::Point(60, 60), child_surface_id,
       gfx::Point(10, 10)},
      {root_surface_id, gfx::Point(239, 239), child_surface_id,
       gfx::Point(189, 189)},
      {root_surface_id, gfx::Point(244, 244), child_surface_id,
       gfx::Point(194, 194)},
      {root_surface_id, gfx::Point(50, 50), root_surface_id,
       gfx::Point(50, 50)},
      {root_surface_id, gfx::Point(249, 249), root_surface_id,
       gfx::Point(249, 249)},
  };

  TestSurfaceHittestDelegate empty_delegate;
  RunTests(&empty_delegate, surface_manager(), test_expectations_without_insets,
           arraysize(test_expectations_without_insets));

  // Verify that insets have NOT affected hit targeting.
  EXPECT_EQ(0, empty_delegate.reject_target_overrides());
  EXPECT_EQ(0, empty_delegate.accept_target_overrides());

  TestCase test_expectations_with_reject_insets[] = {
      // Point (55, 55) falls outside the child surface due to the insets
      // introduced above.
      {root_surface_id, gfx::Point(55, 55), root_surface_id,
       gfx::Point(55, 55)},
      // These two points still fall within the child surface.
      {root_surface_id, gfx::Point(60, 60), child_surface_id,
       gfx::Point(10, 10)},
      {root_surface_id, gfx::Point(239, 239), child_surface_id,
       gfx::Point(189, 189)},
      // Point (244, 244) falls outside the child surface due to the insets
      // introduced above.
      {root_surface_id, gfx::Point(244, 244), root_surface_id,
       gfx::Point(244, 244)},
      // Next two points also fall within within the insets indroduced above.
      {root_surface_id, gfx::Point(50, 50), root_surface_id,
       gfx::Point(50, 50)},
      {root_surface_id, gfx::Point(249, 249), root_surface_id,
       gfx::Point(249, 249)},
  };

  TestSurfaceHittestDelegate reject_delegate;
  reject_delegate.AddInsetsForRejectSurface(child_surface_id,
                                            gfx::Insets(10, 10, 10, 10));
  RunTests(&reject_delegate, surface_manager(),
           test_expectations_with_reject_insets,
           arraysize(test_expectations_with_reject_insets));

  // Verify that insets have affected hit targeting.
  EXPECT_EQ(4, reject_delegate.reject_target_overrides());
  EXPECT_EQ(0, reject_delegate.accept_target_overrides());

  TestCase test_expectations_with_accept_insets[] = {
      {root_surface_id, gfx::Point(55, 55), child_surface_id, gfx::Point(5, 5)},
      {root_surface_id, gfx::Point(60, 60), child_surface_id,
       gfx::Point(10, 10)},
      {root_surface_id, gfx::Point(239, 239), child_surface_id,
       gfx::Point(189, 189)},
      {root_surface_id, gfx::Point(244, 244), child_surface_id,
       gfx::Point(194, 194)},
      // Next two points fall within within the insets indroduced above.
      {root_surface_id, gfx::Point(50, 50), child_surface_id, gfx::Point(0, 0)},
      {root_surface_id, gfx::Point(249, 249), child_surface_id,
       gfx::Point(199, 199)},
  };

  TestSurfaceHittestDelegate accept_delegate;
  accept_delegate.AddInsetsForAcceptSurface(child_surface_id,
                                            gfx::Insets(5, 5, 5, 5));
  RunTests(&accept_delegate, surface_manager(),
           test_expectations_with_accept_insets,
           arraysize(test_expectations_with_accept_insets));

  // Verify that insets have affected hit targeting.
  EXPECT_EQ(0, accept_delegate.reject_target_overrides());
  EXPECT_EQ(2, accept_delegate.accept_target_overrides());

  root_support().EvictCurrentSurface();
  child_support().EvictCurrentSurface();
}

}  // namespace cc
