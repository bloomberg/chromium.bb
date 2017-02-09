// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface.h"
#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface_dependency_tracker.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "cc/test/scheduler_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

static constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);

CompositorFrame MakeCompositorFrame(
    std::vector<SurfaceId> referenced_surfaces) {
  CompositorFrame compositor_frame;
  compositor_frame.metadata.referenced_surfaces =
      std::move(referenced_surfaces);
  return compositor_frame;
}

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

// Surface 1 is blocked on Surface 2 and Surface 3.
TEST(SurfaceTest, DisplayCompositorLockingBlockedOnTwo) {
  SurfaceManager manager;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source(
      new FakeExternalBeginFrameSource(0.f, false));

  std::unique_ptr<SurfaceDependencyTracker> dependency_tracker(
      new SurfaceDependencyTracker(&manager, begin_frame_source.get()));
  manager.SetDependencyTracker(std::move(dependency_tracker));

  LocalSurfaceId local_surface_id1(6, base::UnguessableToken::Create());
  SurfaceId surface_id1(FrameSinkId(1, 1), local_surface_id1);

  LocalSurfaceId local_surface_id2(7, base::UnguessableToken::Create());
  SurfaceId surface_id2(FrameSinkId(2, 2), local_surface_id2);

  LocalSurfaceId local_surface_id3(8, base::UnguessableToken::Create());
  SurfaceId surface_id3(FrameSinkId(3, 3), local_surface_id3);

  FakeSurfaceFactoryClient surface_factory_client1;
  SurfaceFactory factory1(FrameSinkId(1, 1), &manager,
                          &surface_factory_client1);
  factory1.SubmitCompositorFrame(
      local_surface_id1, MakeCompositorFrame({surface_id2, surface_id3}),
      SurfaceFactory::DrawCallback());
  EXPECT_TRUE(manager.dependency_tracker()->has_deadline());

  // |factory1| is blocked on |surface_id2| and |surface_id3|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

  FakeSurfaceFactoryClient surface_factory_client2;
  SurfaceFactory factory2(FrameSinkId(2, 2), &manager,
                          &surface_factory_client2);
  factory2.SubmitCompositorFrame(local_surface_id2,
                                 MakeCompositorFrame(std::vector<SurfaceId>()),
                                 SurfaceFactory::DrawCallback());

  EXPECT_TRUE(manager.dependency_tracker()->has_deadline());
  EXPECT_TRUE(factory2.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory2.current_surface_for_testing()->HasPendingFrame());

  // |factory1| is blocked on just |surface_id3|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

  FakeSurfaceFactoryClient surface_factory_client3;
  SurfaceFactory factory3(FrameSinkId(3, 3), &manager,
                          &surface_factory_client3);
  factory3.SubmitCompositorFrame(local_surface_id3,
                                 MakeCompositorFrame(std::vector<SurfaceId>()),
                                 SurfaceFactory::DrawCallback());
  EXPECT_FALSE(manager.dependency_tracker()->has_deadline());

  // |factory1|'s Frame is now active.
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasPendingFrame());

  factory1.EvictSurface();
  factory2.EvictSurface();
  factory3.EvictSurface();

  // Destroy the SurfaceDependencyTracker before we destroy the
  // BeginFrameSource.
  manager.SetDependencyTracker(nullptr);
}

// Surface 1 is blocked on Surface 2 which is blocked on Surface 3.
TEST(SurfaceTest, DisplayCompositorLockingBlockedChain) {
  SurfaceManager manager;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source(
      new FakeExternalBeginFrameSource(0.f, false));

  std::unique_ptr<SurfaceDependencyTracker> dependency_tracker(
      new SurfaceDependencyTracker(&manager, begin_frame_source.get()));
  manager.SetDependencyTracker(std::move(dependency_tracker));

  LocalSurfaceId local_surface_id1(6, base::UnguessableToken::Create());
  SurfaceId surface_id1(FrameSinkId(1, 1), local_surface_id1);

  LocalSurfaceId local_surface_id2(7, base::UnguessableToken::Create());
  SurfaceId surface_id2(FrameSinkId(2, 2), local_surface_id2);

  LocalSurfaceId local_surface_id3(8, base::UnguessableToken::Create());
  SurfaceId surface_id3(FrameSinkId(3, 3), local_surface_id3);

  FakeSurfaceFactoryClient surface_factory_client1;
  SurfaceFactory factory1(FrameSinkId(1, 1), &manager,
                          &surface_factory_client1);
  factory1.SubmitCompositorFrame(local_surface_id1,
                                 MakeCompositorFrame({surface_id2}),
                                 SurfaceFactory::DrawCallback());

  // |factory1| is blocked on |surface_id2|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

  FakeSurfaceFactoryClient surface_factory_client2;
  SurfaceFactory factory2(FrameSinkId(2, 2), &manager,
                          &surface_factory_client2);
  factory2.SubmitCompositorFrame(local_surface_id2,
                                 MakeCompositorFrame({surface_id3}),
                                 SurfaceFactory::DrawCallback());

  // |factory2| is blocked on |surface_id3|.
  EXPECT_FALSE(factory2.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory2.current_surface_for_testing()->HasPendingFrame());

  // |factory1| is still blocked on just |surface_id2|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

  FakeSurfaceFactoryClient surface_factory_client3;
  SurfaceFactory factory3(FrameSinkId(3, 3), &manager,
                          &surface_factory_client3);
  CompositorFrame frame3;
  factory3.SubmitCompositorFrame(local_surface_id3,
                                 MakeCompositorFrame(std::vector<SurfaceId>()),
                                 SurfaceFactory::DrawCallback());

  // |factory1|'s Frame is now active.
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasPendingFrame());

  // |factory2|'s Frame is now active.
  EXPECT_TRUE(factory2.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory2.current_surface_for_testing()->HasPendingFrame());

  factory1.EvictSurface();
  factory2.EvictSurface();
  factory3.EvictSurface();

  // Destroy the SurfaceDependencyTracker before we destroy the
  // BeginFrameSource.
  manager.SetDependencyTracker(nullptr);
}

// Surface 1 and Surface 2 are blocked on Surface 3.
TEST(SurfaceTest, DisplayCompositorLockingTwoBlockedOnOne) {
  SurfaceManager manager;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source(
      new FakeExternalBeginFrameSource(0.f, false));

  std::unique_ptr<SurfaceDependencyTracker> dependency_tracker(
      new SurfaceDependencyTracker(&manager, begin_frame_source.get()));
  manager.SetDependencyTracker(std::move(dependency_tracker));

  LocalSurfaceId local_surface_id1(6, base::UnguessableToken::Create());
  SurfaceId surface_id1(FrameSinkId(1, 1), local_surface_id1);

  LocalSurfaceId local_surface_id2(7, base::UnguessableToken::Create());
  SurfaceId surface_id2(FrameSinkId(2, 2), local_surface_id2);

  LocalSurfaceId local_surface_id3(8, base::UnguessableToken::Create());
  SurfaceId surface_id3(FrameSinkId(3, 3), local_surface_id3);

  FakeSurfaceFactoryClient surface_factory_client1;
  SurfaceFactory factory1(FrameSinkId(1, 1), &manager,
                          &surface_factory_client1);
  factory1.SubmitCompositorFrame(local_surface_id1,
                                 MakeCompositorFrame({surface_id3}),
                                 SurfaceFactory::DrawCallback());

  // |factory1| is blocked on |surface_id3|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

  FakeSurfaceFactoryClient surface_factory_client2;
  SurfaceFactory factory2(FrameSinkId(2, 2), &manager,
                          &surface_factory_client2);
  factory2.SubmitCompositorFrame(local_surface_id2,
                                 MakeCompositorFrame({surface_id3}),
                                 SurfaceFactory::DrawCallback());

  // |factory2| is blocked on |surface_id3|.
  EXPECT_FALSE(factory2.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory2.current_surface_for_testing()->HasPendingFrame());

  // |factory1| is still blocked on |surface_id3|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

  FakeSurfaceFactoryClient surface_factory_client3;
  SurfaceFactory factory3(FrameSinkId(3, 3), &manager,
                          &surface_factory_client3);
  factory3.SubmitCompositorFrame(local_surface_id3,
                                 MakeCompositorFrame(std::vector<SurfaceId>()),
                                 SurfaceFactory::DrawCallback());

  // |factory1|'s Frame is now active.
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasPendingFrame());

  // |factory2|'s Frame is now active.
  EXPECT_TRUE(factory2.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory2.current_surface_for_testing()->HasPendingFrame());

  factory1.EvictSurface();
  factory2.EvictSurface();
  factory3.EvictSurface();

  // Destroy the SurfaceDependencyTracker before we destroy the
  // BeginFrameSource.
  manager.SetDependencyTracker(nullptr);
}

// |factory1| is blocked on |surface_id2| but the deadline hits.
TEST(SurfaceTest, DisplayCompositorLockingDeadlineHits) {
  SurfaceManager manager;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source(
      new FakeExternalBeginFrameSource(0.f, false));

  std::unique_ptr<SurfaceDependencyTracker> dependency_tracker(
      new SurfaceDependencyTracker(&manager, begin_frame_source.get()));
  manager.SetDependencyTracker(std::move(dependency_tracker));

  LocalSurfaceId local_surface_id1(6, base::UnguessableToken::Create());
  SurfaceId surface_id1(FrameSinkId(1, 1), local_surface_id1);

  LocalSurfaceId local_surface_id2(7, base::UnguessableToken::Create());
  SurfaceId surface_id2(FrameSinkId(2, 2), local_surface_id2);

  LocalSurfaceId local_surface_id3(8, base::UnguessableToken::Create());
  SurfaceId surface_id3(FrameSinkId(3, 3), local_surface_id3);

  FakeSurfaceFactoryClient surface_factory_client1;
  SurfaceFactory factory1(FrameSinkId(1, 1), &manager,
                          &surface_factory_client1);
  CompositorFrame frame;
  factory1.SubmitCompositorFrame(local_surface_id1,
                                 MakeCompositorFrame({surface_id2}),
                                 SurfaceFactory::DrawCallback());
  EXPECT_TRUE(manager.dependency_tracker()->has_deadline());

  // |factory1| is blocked on |surface_id2|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

  FakeSurfaceFactoryClient surface_factory_client2;
  SurfaceFactory factory2(FrameSinkId(2, 2), &manager,
                          &surface_factory_client2);
  factory2.SubmitCompositorFrame(local_surface_id2,
                                 MakeCompositorFrame({surface_id3}),
                                 SurfaceFactory::DrawCallback());
  EXPECT_TRUE(manager.dependency_tracker()->has_deadline());

  // |factory2| is blocked on |surface_id3|.
  EXPECT_FALSE(factory2.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory2.current_surface_for_testing()->HasPendingFrame());

  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  for (int i = 0; i < 3; ++i) {
    begin_frame_source->TestOnBeginFrame(args);
    // |factory1| is still blocked on |surface_id2|.
    EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
    EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());

    // |factory2| is still blcoked on |surface_id3|.
    EXPECT_FALSE(factory2.current_surface_for_testing()->HasActiveFrame());
    EXPECT_TRUE(factory2.current_surface_for_testing()->HasPendingFrame());
  }

  begin_frame_source->TestOnBeginFrame(args);

  // |factory1| and |factory2| are no longer blocked.
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasPendingFrame());
  EXPECT_TRUE(factory2.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory2.current_surface_for_testing()->HasPendingFrame());
  EXPECT_FALSE(manager.dependency_tracker()->has_deadline());

  factory1.EvictSurface();
  factory2.EvictSurface();

  // Destroy the SurfaceDependencyTracker before we destroy the
  // BeginFrameSource.
  manager.SetDependencyTracker(nullptr);
}

// Verifies that the deadline does not reset if we submit CompositorFrames
// to new Surfaces with unresolved dependencies.
TEST(SurfaceTest, DisplayCompositorLockingFramesSubmittedAfterDeadlineSet) {
  SurfaceManager manager;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source(
      new FakeExternalBeginFrameSource(0.f, false));

  std::unique_ptr<SurfaceDependencyTracker> dependency_tracker(
      new SurfaceDependencyTracker(&manager, begin_frame_source.get()));
  manager.SetDependencyTracker(std::move(dependency_tracker));

  LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
  SurfaceId surface_id(FrameSinkId(1, 1), local_surface_id);

  std::vector<std::unique_ptr<SurfaceFactoryClient>> surface_factory_clients;
  std::vector<std::unique_ptr<SurfaceFactory>> surface_factories;

  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Issue BeginFrames and create new surfaces with dependencies.
  for (int i = 0; i < 3; ++i) {
    LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
    std::unique_ptr<SurfaceFactoryClient> surface_factory_client(
        base::MakeUnique<FakeSurfaceFactoryClient>());
    std::unique_ptr<SurfaceFactory> surface_factory(
        base::MakeUnique<SurfaceFactory>(FrameSinkId(2 + i, 2 + i), &manager,
                                         surface_factory_client.get()));
    surface_factory->SubmitCompositorFrame(local_surface_id,
                                           MakeCompositorFrame({surface_id}),
                                           SurfaceFactory::DrawCallback());
    // The deadline has been set.
    EXPECT_TRUE(manager.dependency_tracker()->has_deadline());

    // |surface_factory| is blocked on |surface_id2|.
    EXPECT_FALSE(
        surface_factory->current_surface_for_testing()->HasActiveFrame());
    EXPECT_TRUE(
        surface_factory->current_surface_for_testing()->HasPendingFrame());

    // Issue a BeginFrame.
    begin_frame_source->TestOnBeginFrame(args);

    surface_factory_clients.push_back(std::move(surface_factory_client));
    surface_factories.push_back(std::move(surface_factory));
  }

  // This BeginFrame should cause all the Surfaces to activate.
  begin_frame_source->TestOnBeginFrame(args);

  // Verify that all the Surfaces have activated.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(
        surface_factories[i]->current_surface_for_testing()->HasActiveFrame());
    EXPECT_FALSE(
        surface_factories[i]->current_surface_for_testing()->HasPendingFrame());
    // We must evict the surface before we destroy the factories.
    surface_factories[i]->EvictSurface();
  }

  // Destroy all the SurfaceFactories and their clients.
  surface_factories.clear();
  surface_factory_clients.clear();

  // Destroy the SurfaceDependencyTracker before we destroy the
  // BeginFrameSource.
  manager.SetDependencyTracker(nullptr);
}

// Frame activates once a new frame is submitted.
TEST(SurfaceTest, DisplayCompositorLockingNewFrameOverridesOldDependencies) {
  SurfaceManager manager;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source(
      new FakeExternalBeginFrameSource(0.f, false));

  manager.SetDependencyTracker(base::MakeUnique<SurfaceDependencyTracker>(
      &manager, begin_frame_source.get()));

  LocalSurfaceId local_surface_id1(6, base::UnguessableToken::Create());
  SurfaceId surface_id1(FrameSinkId(1, 1), local_surface_id1);

  LocalSurfaceId local_surface_id2(7, base::UnguessableToken::Create());
  SurfaceId surface_id2(FrameSinkId(2, 2), local_surface_id2);

  FakeSurfaceFactoryClient surface_factory_client1;
  SurfaceFactory factory1(FrameSinkId(1, 1), &manager,
                          &surface_factory_client1);
  factory1.SubmitCompositorFrame(local_surface_id1,
                                 MakeCompositorFrame({surface_id2}),
                                 SurfaceFactory::DrawCallback());

  // |factory1|'s Frame is blocked on |surface_id2|.
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasPendingFrame());
  EXPECT_TRUE(manager.dependency_tracker()->has_deadline());

  // Another frame is submitted to |factory1| that has no dependencies.
  factory1.SubmitCompositorFrame(local_surface_id1,
                                 MakeCompositorFrame(std::vector<SurfaceId>()),
                                 SurfaceFactory::DrawCallback());
  EXPECT_TRUE(factory1.current_surface_for_testing()->HasActiveFrame());
  EXPECT_FALSE(factory1.current_surface_for_testing()->HasPendingFrame());
  EXPECT_FALSE(manager.dependency_tracker()->has_deadline());

  factory1.EvictSurface();

  // Destroy the SurfaceDependencyTracker before we destroy the
  // BeginFrameSource.
  manager.SetDependencyTracker(nullptr);
}

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
    SurfaceIdAllocator allocator;
    LocalSurfaceId id1 = allocator.GenerateId();
    LocalSurfaceId id2 = allocator.GenerateId();
    EXPECT_NE(id1, id2);
  }
}

}  // namespace
}  // namespace cc
