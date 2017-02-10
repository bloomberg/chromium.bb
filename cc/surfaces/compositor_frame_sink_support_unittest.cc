// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/compositor_frame_sink_support.h"

#include "base/macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

namespace cc {
namespace test {
namespace {

constexpr FrameSinkId kParentFrameSink(2, 1);
constexpr FrameSinkId kChildFrameSink1(65563, 1);
constexpr FrameSinkId kChildFrameSink2(65564, 1);
constexpr FrameSinkId kArbitraryFrameSink(1337, 7331);

std::vector<SurfaceId> empty_surface_ids() {
  return std::vector<SurfaceId>();
}

SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t local_id) {
  return SurfaceId(
      frame_sink_id,
      LocalSurfaceId(local_id, base::UnguessableToken::Deserialize(0, 1u)));
}

CompositorFrame MakeCompositorFrame(
    std::vector<SurfaceId> referenced_surfaces) {
  CompositorFrame compositor_frame;
  compositor_frame.metadata.referenced_surfaces =
      std::move(referenced_surfaces);
  return compositor_frame;
}

}  // namespace

class CompositorFrameSinkSupportTest : public testing::Test,
                                       public CompositorFrameSinkSupportClient {
 public:
  CompositorFrameSinkSupportTest()
      : surface_manager_(SurfaceManager::LifetimeType::REFERENCES) {}
  ~CompositorFrameSinkSupportTest() override {}

  CompositorFrameSinkSupport& parent_support() { return *supports_[0]; }
  Surface* parent_surface() {
    return parent_support().current_surface_for_testing();
  }
  const ReferencedSurfaceTracker& parent_reference_tracker() {
    return parent_support().ReferenceTrackerForTesting();
  }

  CompositorFrameSinkSupport& child_support1() { return *supports_[1]; }
  Surface* child_surface1() {
    return child_support1().current_surface_for_testing();
  }

  CompositorFrameSinkSupport& child_support2() { return *supports_[2]; }
  Surface* child_surface2() {
    return child_support2().current_surface_for_testing();
  }

  CompositorFrameSinkSupport& support(int index) { return *supports_[index]; }
  Surface* surface(int index) {
    return support(index).current_surface_for_testing();
  }

  SurfaceManager& surface_manager() { return surface_manager_; }

  SurfaceDependencyTracker& dependency_tracker() {
    return *surface_manager_.dependency_tracker();
  }

  FakeExternalBeginFrameSource* begin_frame_source() {
    return begin_frame_source_.get();
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    begin_frame_source_ =
        base::MakeUnique<FakeExternalBeginFrameSource>(0.f, false);
    std::unique_ptr<SurfaceDependencyTracker> dependency_tracker(
        new SurfaceDependencyTracker(&surface_manager_,
                                     begin_frame_source_.get()));
    surface_manager_.SetDependencyTracker(std::move(dependency_tracker));
    supports_.push_back(base::MakeUnique<CompositorFrameSinkSupport>(
        this, &surface_manager_, kParentFrameSink, nullptr /* display */,
        nullptr /* display_begin_frame_source */));
    supports_.push_back(base::MakeUnique<CompositorFrameSinkSupport>(
        this, &surface_manager_, kChildFrameSink1, nullptr /* display */,
        nullptr /* display_begin_frame_source */));
    supports_.push_back(base::MakeUnique<CompositorFrameSinkSupport>(
        this, &surface_manager_, kChildFrameSink2, nullptr /* display */,
        nullptr /* display_begin_frame_source */));
  }

  void TearDown() override {
    surface_manager_.SetDependencyTracker(nullptr);

    // SurfaceDependencyTracker depends on this BeginFrameSource and so it must
    // be destroyed AFTER the dependency tracker is destroyed.
    begin_frame_source_.reset();

    supports_.clear();
  }

  // CompositorFrameSinkSupportClient implementation.
  void DidReceiveCompositorFrameAck() override {}
  void OnBeginFrame(const BeginFrameArgs& args) override {}
  void ReclaimResources(const ReturnedResourceArray& resources) override {}
  void WillDrawSurface() override {}

 private:
  SurfaceManager surface_manager_;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source_;
  std::vector<std::unique_ptr<CompositorFrameSinkSupport>> supports_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkSupportTest);
};

// The parent Surface is blocked on |child_id1| and |child_id2|.
TEST_F(CompositorFrameSinkSupportTest, DisplayCompositorLockingBlockedOnTwo) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1, child_id2}));

  // parent_support is blocked on |child_id1| and |child_id2|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id1, child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id1|.
  // parent_support should now only be blocked on |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_FALSE(dependency_tracker().has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(), IsEmpty());
}

// The parent Surface is blocked on |child_id2| which is blocked on |child_id3|.
TEST_F(CompositorFrameSinkSupportTest, DisplayCompositorLockingBlockedChain) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id1}));

  // parent_support is blocked on |child_id1|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id1));

  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  // child_support1 should now be blocked on |child_id2|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id1| because it's pending.
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id1));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_FALSE(dependency_tracker().has_deadline());

  // child_surface1 should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(), IsEmpty());

  // parent_surface should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(), IsEmpty());
}

// parent_surface and child_surface1 are blocked on |child_id2|.
TEST_F(CompositorFrameSinkSupportTest,
       DisplayCompositorLockingTwoBlockedOnOne) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  // parent_support is blocked on |child_id2|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id2));

  // child_support1 should now be blocked on |child_id2|.
  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id2|.
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_FALSE(dependency_tracker().has_deadline());

  // child_surface1 should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(), IsEmpty());

  // parent_surface should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(), IsEmpty());
}

// parent_surface is blocked on |child_id1|, and child_surface2 is blocked on
// |child_id2| until the deadline hits.
TEST_F(CompositorFrameSinkSupportTest, DisplayCompositorLockingDeadlineHits) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id1}));

  // parent_support is blocked on |child_id1|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id1));

  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  // child_support1 should now be blocked on |child_id2|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id1| because it's pending.
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(child_id1));

  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  for (int i = 0; i < 3; ++i) {
    begin_frame_source()->TestOnBeginFrame(args);
    // There is still a looming deadline! Eeek!
    EXPECT_TRUE(dependency_tracker().has_deadline());

    // parent_support is still blocked on |child_id1|.
    EXPECT_FALSE(parent_surface()->HasActiveFrame());
    EXPECT_TRUE(parent_surface()->HasPendingFrame());
    EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
                UnorderedElementsAre(child_id1));

    // child_support1 is still blocked on |child_id2|.
    EXPECT_FALSE(child_surface1()->HasActiveFrame());
    EXPECT_TRUE(child_surface1()->HasPendingFrame());
    EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(),
                UnorderedElementsAre(child_id2));
  }

  begin_frame_source()->TestOnBeginFrame(args);

  // The deadline has passed.
  EXPECT_FALSE(dependency_tracker().has_deadline());

  // parent_surface has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(), IsEmpty());

  // child_surface1 has been activated.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(), IsEmpty());
}

// Verifies that the deadline does not reset if we submit CompositorFrames
// to new Surfaces with unresolved dependencies.
TEST_F(CompositorFrameSinkSupportTest,
       DisplayCompositorLockingFramesSubmittedAfterDeadlineSet) {
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  for (int i = 0; i < 3; ++i) {
    LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
    support(i).SubmitCompositorFrame(local_surface_id,
                                     MakeCompositorFrame({arbitrary_id}));
    // The deadline has been set.
    EXPECT_TRUE(dependency_tracker().has_deadline());

    // support(i) should be blocked on arbitrary_id.
    EXPECT_FALSE(surface(i)->HasActiveFrame());
    EXPECT_TRUE(surface(i)->HasPendingFrame());
    EXPECT_THAT(surface(i)->blocking_surfaces_for_testing(),
                UnorderedElementsAre(arbitrary_id));

    // Issue a BeginFrame to get closer to the deadline.
    begin_frame_source()->TestOnBeginFrame(args);
  }

  // The deadline hits and all the Surfaces should activate.
  begin_frame_source()->TestOnBeginFrame(args);
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(surface(i)->HasActiveFrame());
    EXPECT_FALSE(surface(i)->HasPendingFrame());
    EXPECT_THAT(surface(i)->blocking_surfaces_for_testing(), IsEmpty());
  }
}

// This test verifies at the Surface activates once a CompositorFrame is
// submitted that has no unresolved dependencies.
TEST_F(CompositorFrameSinkSupportTest,
       DisplayCompositorLockingNewFrameOverridesOldDependencies) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  // Submit a CompositorFrame that depends on |arbitrary_id|.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({arbitrary_id}));

  // Verify that the CompositorFrame is blocked on |arbitrary_id|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(arbitrary_id));

  // Submit a CompositorFrame that has no dependencies.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  // Verify that the CompositorFrame has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(), IsEmpty());
}

// This test verifies that the set of references from a Surface includes both
// the pending and active CompositorFrames.
TEST_F(CompositorFrameSinkSupportTest,
       DisplayCompositorLockingReferencesFromPendingAndActiveFrames) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);
  const SurfaceReference parent_child_reference(parent_id, child_id);
  const SurfaceReference parent_arbitrary_reference(parent_id, arbitrary_id);

  // child_support1 submits a CompositorFrame without any dependencies.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  // Verify that the child surface is not blocked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces_for_testing(), IsEmpty());

  // parent_support submits a CompositorFrame that depends on |child_id1|
  // which is already active. Thus, this CompositorFrame should activate
  // immediately.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id}));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(), IsEmpty());
  // Verify that the parent will add a reference to the |child_id|.
  EXPECT_THAT(parent_reference_tracker().references_to_add(),
              UnorderedElementsAre(parent_child_reference));
  EXPECT_THAT(parent_reference_tracker().references_to_remove(), IsEmpty());

  // parent_support now submits another CompositorFrame to the same surface
  // but depends on arbitrary_id. The parent surface should now have both
  // a pending and active CompositorFrame.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({arbitrary_id}));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces_for_testing(),
              UnorderedElementsAre(arbitrary_id));
  // Verify that the parent will add a reference to |arbitrary_id| and will not
  // remove a reference to |child_id|.
  EXPECT_THAT(parent_reference_tracker().references_to_add(),
              UnorderedElementsAre(parent_arbitrary_reference));
  EXPECT_THAT(parent_reference_tracker().references_to_remove(), IsEmpty());
}

}  // namespace test
}  // namespace cc
