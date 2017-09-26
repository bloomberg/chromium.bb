// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface.h"
#include "base/memory/ptr_util.h"
#include "cc/test/scheduler_test_common.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/local_surface_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_dependency_tracker.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
namespace {

constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);
constexpr bool kIsRoot = true;
constexpr bool kNeedsSyncPoints = true;

TEST(SurfaceTest, SurfaceLifetime) {
  FrameSinkManagerImpl frame_sink_manager(
      SurfaceManager::LifetimeType::SEQUENCES);
  SurfaceManager* surface_manager = frame_sink_manager.surface_manager();
  auto support = CompositorFrameSinkSupport::Create(
      nullptr, &frame_sink_manager, kArbitraryFrameSinkId, kIsRoot,
      kNeedsSyncPoints);

  LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  support->SubmitCompositorFrame(local_surface_id, test::MakeCompositorFrame());
  EXPECT_TRUE(surface_manager->GetSurfaceForId(surface_id));
  support->EvictCurrentSurface();

  EXPECT_EQ(NULL, surface_manager->GetSurfaceForId(surface_id));
}

TEST(SurfaceTest, SurfaceIds) {
  for (size_t i = 0; i < 3; ++i) {
    LocalSurfaceIdAllocator allocator;
    LocalSurfaceId id1 = allocator.GenerateId();
    LocalSurfaceId id2 = allocator.GenerateId();
    EXPECT_NE(id1, id2);
  }
}

void TestCopyResultCallback(bool* called,
                            std::unique_ptr<CopyOutputResult> result) {
  *called = true;
}

// Test that CopyOutputRequests can outlive the current frame and be
// aggregated on the next frame.
TEST(SurfaceTest, CopyRequestLifetime) {
  FrameSinkManagerImpl frame_sink_manager;
  SurfaceManager* surface_manager = frame_sink_manager.surface_manager();
  auto support = CompositorFrameSinkSupport::Create(
      nullptr, &frame_sink_manager, kArbitraryFrameSinkId, kIsRoot,
      kNeedsSyncPoints);

  LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  CompositorFrame frame = test::MakeCompositorFrame();
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  Surface* surface = surface_manager->GetSurfaceForId(surface_id);
  ASSERT_TRUE(!!surface);

  bool copy_called = false;
  support->RequestCopyOfSurface(std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA_BITMAP,
      base::BindOnce(&TestCopyResultCallback, &copy_called)));
  EXPECT_TRUE(surface_manager->GetSurfaceForId(surface_id));
  EXPECT_FALSE(copy_called);

  int max_frame = 3, start_id = 200;
  for (int i = 0; i < max_frame; ++i) {
    CompositorFrame frame = test::MakeEmptyCompositorFrame();
    frame.render_pass_list.push_back(RenderPass::Create());
    frame.render_pass_list.back()->id = i * 3 + start_id;
    frame.render_pass_list.push_back(RenderPass::Create());
    frame.render_pass_list.back()->id = i * 3 + start_id + 1;
    frame.render_pass_list.push_back(RenderPass::Create());
    frame.render_pass_list.back()->SetNew(i * 3 + start_id + 2,
                                          gfx::Rect(0, 0, 20, 20), gfx::Rect(),
                                          gfx::Transform());
    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  int last_pass_id = (max_frame - 1) * 3 + start_id + 2;
  // The copy request should stay on the Surface until TakeCopyOutputRequests
  // is called.
  EXPECT_FALSE(copy_called);
  EXPECT_EQ(
      1u,
      surface->GetActiveFrame().render_pass_list.back()->copy_requests.size());

  Surface::CopyRequestsMap copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);
  EXPECT_EQ(1u, copy_requests.size());
  // Last (root) pass should receive copy request.
  ASSERT_EQ(1u, copy_requests.count(last_pass_id));
  EXPECT_FALSE(copy_called);
  copy_requests.clear();  // Deleted requests will auto-send an empty result.
  EXPECT_TRUE(copy_called);

  support->EvictCurrentSurface();
}

}  // namespace
}  // namespace viz
