// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"
#include "base/memory/ptr_util.h"
#include "cc/output/copy_output_result.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface_dependency_tracker.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

class FakeSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  FakeSurfaceFactoryClient() : begin_frame_source_(nullptr) {}

  void ReturnResources(const ReturnedResourceArray& resources) override {}

  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {
    begin_frame_source_ = begin_frame_source;
  }

  BeginFrameSource* begin_frame_source() { return begin_frame_source_; }

 private:
  BeginFrameSource* begin_frame_source_;
};

TEST(SurfaceTest, SurfaceLifetime) {
  SurfaceManager manager;
  FakeSurfaceFactoryClient surface_factory_client;
  SurfaceFactory factory(kArbitraryFrameSinkId, &manager,
                         &surface_factory_client);

  LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  factory.SubmitCompositorFrame(local_surface_id, CompositorFrame(),
                                SurfaceFactory::DrawCallback());
  EXPECT_TRUE(manager.GetSurfaceForId(surface_id));
  factory.EvictSurface();

  EXPECT_EQ(NULL, manager.GetSurfaceForId(surface_id));
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
  SurfaceManager manager;
  FakeSurfaceFactoryClient surface_factory_client;
  SurfaceFactory factory(kArbitraryFrameSinkId, &manager,
                         &surface_factory_client);

  LocalSurfaceId local_surface_id(6, base::UnguessableToken::Create());
  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);
  CompositorFrame frame;
  frame.render_pass_list.push_back(RenderPass::Create());
  factory.SubmitCompositorFrame(local_surface_id, std::move(frame),
                                SurfaceFactory::DrawCallback());
  Surface* surface = manager.GetSurfaceForId(surface_id);
  ASSERT_TRUE(!!surface);

  bool copy_called = false;
  factory.RequestCopyOfSurface(CopyOutputRequest::CreateRequest(
      base::Bind(&TestCopyResultCallback, &copy_called)));
  EXPECT_TRUE(manager.GetSurfaceForId(surface_id));
  EXPECT_FALSE(copy_called);

  CompositorFrame frame2;
  frame2.render_pass_list.push_back(RenderPass::Create());
  frame2.render_pass_list.back()->id = 1;
  frame2.render_pass_list.push_back(RenderPass::Create());
  frame2.render_pass_list.back()->id = 2;
  factory.SubmitCompositorFrame(local_surface_id, std::move(frame2),
                                SurfaceFactory::DrawCallback());

  // The copy request should stay on the Surface after a new frame is created.
  EXPECT_FALSE(copy_called);
  EXPECT_EQ(
      1u,
      surface->GetActiveFrame().render_pass_list.back()->copy_requests.size());

  std::multimap<int, std::unique_ptr<CopyOutputRequest>> copy_requests;
  surface->TakeCopyOutputRequests(&copy_requests);
  EXPECT_EQ(1u, copy_requests.size());
  // Last (root) pass should receive copy request.
  ASSERT_EQ(1u, copy_requests.count(2));
  EXPECT_FALSE(copy_called);
  copy_requests.find(2)->second->SendEmptyResult();
  EXPECT_TRUE(copy_called);

  factory.EvictSurface();
}

}  // namespace
}  // namespace cc
