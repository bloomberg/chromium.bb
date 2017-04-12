// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/compositor_frame_sink_support.h"

#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/surfaces/compositor_frame_sink_support_client.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/begin_frame_args_test.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::Invoke;
using testing::_;
using testing::Eq;

namespace cc {
namespace test {
namespace {

constexpr FrameSinkId kDisplayFrameSink(2, 0);
constexpr FrameSinkId kParentFrameSink(3, 0);
constexpr FrameSinkId kChildFrameSink1(65563, 0);
constexpr FrameSinkId kChildFrameSink2(65564, 0);
constexpr FrameSinkId kArbitraryFrameSink(1337, 7331);

class MockCompositorFrameSinkSupportClient
    : public CompositorFrameSinkSupportClient {
 public:
  MockCompositorFrameSinkSupportClient() {
    ON_CALL(*this, ReclaimResources(_))
        .WillByDefault(Invoke(
            this,
            &MockCompositorFrameSinkSupportClient::ReclaimResourcesInternal));
    ON_CALL(*this, DidReceiveCompositorFrameAck(_))
        .WillByDefault(Invoke(
            this,
            &MockCompositorFrameSinkSupportClient::ReclaimResourcesInternal));
  }

  ReturnedResourceArray& last_returned_resources() {
    return last_returned_resources_;
  }

  // CompositorFrameSinkSupportClient implementation.
  MOCK_METHOD1(DidReceiveCompositorFrameAck,
               void(const ReturnedResourceArray&));
  MOCK_METHOD1(OnBeginFrame, void(const BeginFrameArgs&));
  MOCK_METHOD1(ReclaimResources, void(const ReturnedResourceArray&));
  MOCK_METHOD2(WillDrawSurface, void(const LocalSurfaceId&, const gfx::Rect&));

 private:
  void ReclaimResourcesInternal(const ReturnedResourceArray& resources) {
    last_returned_resources_ = resources;
  }

  ReturnedResourceArray last_returned_resources_;
};

std::vector<SurfaceId> empty_surface_ids() {
  return std::vector<SurfaceId>();
}

SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t local_id) {
  return SurfaceId(
      frame_sink_id,
      LocalSurfaceId(local_id, base::UnguessableToken::Deserialize(0, 1u)));
}

CompositorFrame MakeCompositorFrame(std::vector<SurfaceId> embedded_surfaces,
                                    std::vector<SurfaceId> referenced_surfaces,
                                    TransferableResourceArray resource_list) {
  CompositorFrame compositor_frame;
  compositor_frame.metadata.begin_frame_ack = BeginFrameAck(0, 1, 1, true);
  compositor_frame.metadata.embedded_surfaces = std::move(embedded_surfaces);
  compositor_frame.metadata.referenced_surfaces =
      std::move(referenced_surfaces);
  compositor_frame.resource_list = std::move(resource_list);
  return compositor_frame;
}

CompositorFrame MakeCompositorFrame() {
  return MakeCompositorFrame(empty_surface_ids(), empty_surface_ids(),
                             TransferableResourceArray());
}

CompositorFrame MakeCompositorFrame(std::vector<SurfaceId> embedded_surfaces) {
  return MakeCompositorFrame(embedded_surfaces, embedded_surfaces,
                             TransferableResourceArray());
}

CompositorFrame MakeCompositorFrame(
    std::vector<SurfaceId> embedded_surfaces,
    std::vector<SurfaceId> referenced_surfaces) {
  return MakeCompositorFrame(std::move(embedded_surfaces),
                             std::move(referenced_surfaces),
                             TransferableResourceArray());
}

CompositorFrame MakeCompositorFrameWithResources(
    std::vector<SurfaceId> embedded_surfaces,
    TransferableResourceArray resource_list) {
  return MakeCompositorFrame(embedded_surfaces, embedded_surfaces,
                             std::move(resource_list));
}

TransferableResource MakeResource(ResourceId id,
                                  ResourceFormat format,
                                  uint32_t filter,
                                  const gfx::Size& size) {
  TransferableResource resource;
  resource.id = id;
  resource.format = format;
  resource.filter = filter;
  resource.size = size;
  return resource;
}

}  // namespace

class CompositorFrameSinkSupportTest : public testing::Test {
 public:
  CompositorFrameSinkSupportTest()
      : surface_manager_(SurfaceManager::LifetimeType::REFERENCES) {}
  ~CompositorFrameSinkSupportTest() override {}

  CompositorFrameSinkSupport& display_support() { return *supports_[0]; }
  Surface* display_surface() {
    return display_support().current_surface_for_testing();
  }

  CompositorFrameSinkSupport& parent_support() { return *supports_[1]; }
  Surface* parent_surface() {
    return parent_support().current_surface_for_testing();
  }
  const ReferencedSurfaceTracker& parent_reference_tracker() {
    return parent_support().ReferenceTrackerForTesting();
  }

  CompositorFrameSinkSupport& child_support1() { return *supports_[2]; }
  Surface* child_surface1() {
    return child_support1().current_surface_for_testing();
  }

  CompositorFrameSinkSupport& child_support2() { return *supports_[3]; }
  Surface* child_surface2() {
    return child_support2().current_surface_for_testing();
  }

  CompositorFrameSinkSupport& support(int index) { return *supports_[index]; }
  Surface* surface(int index) {
    return support(index).current_surface_for_testing();
  }

  SurfaceManager& surface_manager() { return surface_manager_; }

  // Returns all the references where |surface_id| is the parent.
  const SurfaceManager::SurfaceIdSet& GetChildReferences(
      const SurfaceId& surface_id) {
    return surface_manager().parent_to_child_refs_[surface_id];
  }

  // Returns true if there is a temporary reference for |surface_id|.
  bool HasTemporaryReference(const SurfaceId& surface_id) {
    return surface_manager().HasTemporaryReference(surface_id);
  }

  SurfaceDependencyTracker& dependency_tracker() {
    return *surface_manager_.dependency_tracker();
  }

  FakeExternalBeginFrameSource* begin_frame_source() {
    return begin_frame_source_.get();
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    constexpr bool is_root = true;
    constexpr bool is_child_root = false;
    constexpr bool handles_frame_sink_id_invalidation = true;
    constexpr bool needs_sync_points = true;
    begin_frame_source_ =
        base::MakeUnique<FakeExternalBeginFrameSource>(0.f, false);
    surface_manager_.SetDependencyTracker(
        base::MakeUnique<SurfaceDependencyTracker>(&surface_manager_,
                                                   begin_frame_source_.get()));
    supports_.push_back(CompositorFrameSinkSupport::Create(
        &support_client_, &surface_manager_, kDisplayFrameSink, is_root,
        handles_frame_sink_id_invalidation, needs_sync_points));
    supports_.push_back(CompositorFrameSinkSupport::Create(
        &support_client_, &surface_manager_, kParentFrameSink, is_child_root,
        handles_frame_sink_id_invalidation, needs_sync_points));
    supports_.push_back(CompositorFrameSinkSupport::Create(
        &support_client_, &surface_manager_, kChildFrameSink1, is_child_root,
        handles_frame_sink_id_invalidation, needs_sync_points));
    supports_.push_back(CompositorFrameSinkSupport::Create(
        &support_client_, &surface_manager_, kChildFrameSink2, is_child_root,
        handles_frame_sink_id_invalidation, needs_sync_points));

    // Normally, the BeginFrameSource would be registered by the Display. We
    // register it here so that BeginFrames are received by the display support,
    // for use in the PassesOnBeginFrameAcks test. Other supports do not receive
    // BeginFrames, since the frame sink hierarchy is not set up in this test.
    surface_manager_.RegisterBeginFrameSource(begin_frame_source_.get(),
                                              kDisplayFrameSink);
  }

  void TearDown() override {
    surface_manager_.SetDependencyTracker(nullptr);
    surface_manager_.UnregisterBeginFrameSource(begin_frame_source_.get());

    // SurfaceDependencyTracker depends on this BeginFrameSource and so it must
    // be destroyed AFTER the dependency tracker is destroyed.
    begin_frame_source_.reset();

    supports_.clear();
  }

 protected:
  testing::NiceMock<MockCompositorFrameSinkSupportClient> support_client_;

 private:
  SurfaceManager surface_manager_;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source_;
  std::vector<std::unique_ptr<CompositorFrameSinkSupport>> supports_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSinkSupportTest);
};

// The display root surface should have a surface reference from the top-level
// root added/removed when a CompositorFrame is submitted with a new SurfaceId.
TEST_F(CompositorFrameSinkSupportTest, RootSurfaceReceivesReferences) {
  const SurfaceId display_id_first = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId display_id_second = MakeSurfaceId(kDisplayFrameSink, 2);

  // Submit a CompositorFrame for the first display root surface.
  display_support().SubmitCompositorFrame(display_id_first.local_surface_id(),
                                          MakeCompositorFrame());

  // A surface reference from the top-level root is added and there shouldn't be
  // a temporary reference.
  EXPECT_FALSE(HasTemporaryReference(display_id_first));
  EXPECT_THAT(GetChildReferences(surface_manager().GetRootSurfaceId()),
              UnorderedElementsAre(display_id_first));

  // Submit a CompositorFrame for the second display root surface.
  display_support().SubmitCompositorFrame(display_id_second.local_surface_id(),
                                          MakeCompositorFrame());

  // A surface reference from the top-level root to |display_id_second| should
  // be added and the reference to |display_root_first| removed.
  EXPECT_FALSE(HasTemporaryReference(display_id_second));
  EXPECT_THAT(GetChildReferences(surface_manager().GetRootSurfaceId()),
              UnorderedElementsAre(display_id_second));

  // Surface |display_id_first| is unreachable and should get deleted.
  EXPECT_EQ(nullptr, surface_manager().GetSurfaceForId(display_id_first));
}

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
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id1, child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id1|.
  // parent_support should now only be blocked on |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_FALSE(dependency_tracker().has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
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
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id1));

  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  // child_support1 should now be blocked on |child_id2|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id1| because it's pending.
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id1));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_FALSE(dependency_tracker().has_deadline());

  // child_surface1 should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(), IsEmpty());

  // parent_surface should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
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
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));

  // child_support1 should now be blocked on |child_id2|.
  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id2|.
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  EXPECT_FALSE(dependency_tracker().has_deadline());

  // child_surface1 should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(), IsEmpty());

  // parent_surface should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
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
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id1));

  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  // child_support1 should now be blocked on |child_id2|.
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id1| because it's pending.
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
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
    EXPECT_THAT(parent_surface()->blocking_surfaces(),
                UnorderedElementsAre(child_id1));

    // child_support1 is still blocked on |child_id2|.
    EXPECT_FALSE(child_surface1()->HasActiveFrame());
    EXPECT_TRUE(child_surface1()->HasPendingFrame());
    EXPECT_THAT(child_surface1()->blocking_surfaces(),
                UnorderedElementsAre(child_id2));
  }

  begin_frame_source()->TestOnBeginFrame(args);

  // The deadline has passed.
  EXPECT_FALSE(dependency_tracker().has_deadline());

  // parent_surface has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());

  // child_surface1 has been activated.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(), IsEmpty());
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
    EXPECT_THAT(surface(i)->blocking_surfaces(),
                UnorderedElementsAre(arbitrary_id));

    // Issue a BeginFrame to get closer to the deadline.
    begin_frame_source()->TestOnBeginFrame(args);
  }

  // The deadline hits and all the Surfaces should activate.
  begin_frame_source()->TestOnBeginFrame(args);
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(surface(i)->HasActiveFrame());
    EXPECT_FALSE(surface(i)->HasPendingFrame());
    EXPECT_THAT(surface(i)->blocking_surfaces(), IsEmpty());
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
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(arbitrary_id));

  // Submit a CompositorFrame that has no dependencies.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  // Verify that the CompositorFrame has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
}

// This test verifies that a pending CompositorFrame does not affect surface
// references. A new surface from a child will continue to exist as a temporary
// reference until the parent's frame activates.
TEST_F(CompositorFrameSinkSupportTest,
       OnlyActiveFramesAffectSurfaceReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  // child_support1 submits a CompositorFrame without any dependencies.
  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame());

  // Verify that the child surface is not blocked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(), IsEmpty());

  // Verify that there's a temporary reference for |child_id1|.
  EXPECT_TRUE(HasTemporaryReference(child_id1));

  // parent_support submits a CompositorFrame that depends on |child_id1|
  // (which is already active) and |child_id2|. Thus, the parent should not
  // activate immediately.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1, child_id2}));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));
  EXPECT_THAT(GetChildReferences(parent_id), IsEmpty());

  // Verify that there's a temporary reference for |child_id1| that still
  // exists.
  EXPECT_TRUE(HasTemporaryReference(child_id1));

  // child_support2 submits a CompositorFrame without any dependencies.
  child_support2().SubmitCompositorFrame(child_id2.local_surface_id(),
                                         MakeCompositorFrame());

  // Verify that the child surface is not blocked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(), IsEmpty());

  // Verify that the parent surface's CompositorFrame has activated and that the
  // temporary reference has been replaced by a permanent one.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
  EXPECT_FALSE(HasTemporaryReference(child_id1));
  EXPECT_THAT(GetChildReferences(parent_id),
              UnorderedElementsAre(child_id1, child_id2));
}

// This test verifies that we do not double count returned resources when a
// CompositorFrame starts out as pending, then becomes active, and then is
// replaced with another active CompositorFrame.
TEST_F(CompositorFrameSinkSupportTest,
       DisplayCompositorLockingResourcesOnlyReturnedOnce) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  // The parent submits a CompositorFrame that depends on |child_id| before the
  // child submits a CompositorFrame. The CompositorFrame also has resources in
  // its resource list.
  TransferableResource resource =
      MakeResource(1337 /* id */, ALPHA_8 /* format */, 1234 /* filter */,
                   gfx::Size(1234, 5678));
  TransferableResourceArray resource_list = {resource};
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrameWithResources({child_id}, resource_list));

  // Verify that the CompositorFrame is blocked on |child_id|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id));

  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  // Verify that the child CompositorFrame activates immediately.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(), IsEmpty());

  // Verify that the parent has activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());

  // The parent submits a CompositorFrame without any dependencies. That frame
  // should activate immediately, replacing the earlier frame. The resource from
  // the earlier frame should be returned to the client.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(), MakeCompositorFrame({empty_surface_ids()}));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
  ReturnedResource returned_resource = resource.ToReturnedResource();
  EXPECT_THAT(support_client_.last_returned_resources(),
              UnorderedElementsAre(returned_resource));
}

// The parent Surface is blocked on |child_id2| which is blocked on |child_id3|.
// child_support1 evicts its blocked Surface. The parent surface should
// activate.
TEST_F(CompositorFrameSinkSupportTest, EvictSurfaceWithPendingFrame) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  // Submit a CompositorFrame that depends on |child_id1|.
  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id1}));

  // Verify that the CompositorFrame is blocked on |child_id1|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id1));

  // Submit a CompositorFrame that depends on |child_id2|.
  child_support1().SubmitCompositorFrame(child_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  // Verify that the CompositorFrame is blocked on |child_id2|.
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));

  // Evict child_support1's current Surface.
  // TODO(fsamuel): EvictFrame => EvictCurrentSurface.
  child_support1().EvictFrame();

  // The parent Surface should immediately activate.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
  EXPECT_FALSE(dependency_tracker().has_deadline());
}

// This test verifies that if a surface has both a pending and active
// CompositorFrame and the pending CompositorFrame activates, replacing the
// existing active CompositorFrame, then the surface reference hierarchy will be
// updated allowing garbage collection of surfaces that are no longer
// referenced.
TEST_F(CompositorFrameSinkSupportTest, DropStaleReferencesAfterActivation) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  // The parent submits a CompositorFrame that depends on |child_id1| before the
  // child submits a CompositorFrame.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id1}));

  // Verify that the CompositorFrame is blocked on |child_id|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id1));

  // Verify that no references are added while the CompositorFrame is pending.
  EXPECT_THAT(GetChildReferences(parent_id), IsEmpty());

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  // Verify that the child CompositorFrame activates immediately.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->blocking_surfaces(), IsEmpty());

  // Verify that the parent Surface has activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());

  // Verify that there is no temporary reference for the child and that
  // the reference from the parent to the child still exists.
  EXPECT_FALSE(HasTemporaryReference(child_id1));
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id1));

  // The parent submits another CompositorFrame that depends on |child_id2|.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id2}));

  // The parent surface should now have both a pending and activate
  // CompositorFrame. Verify that the set of child references from
  // |parent_id| are only from the active CompositorFrame.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(),
              UnorderedElementsAre(child_id2));
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id1));

  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));

  // Verify that the parent Surface has activated and no longer has a pending
  // CompositorFrame. Also verify that |child_id1| is no longer a child
  // reference of |parent_id|.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->blocking_surfaces(), IsEmpty());
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id2));
}

// Checks whether the latency info are moved to the new surface from the old
// one when LocalSurfaceId changes. No frame has unresolved dependencies.
TEST_F(CompositorFrameSinkSupportTest,
       LatencyInfoCarriedOverOnResize_NoUnresolvedDependencies) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const ui::LatencyComponentType latency_type1 =
      ui::BROWSER_SNAPSHOT_FRAME_NUMBER_COMPONENT;
  const int64_t latency_id1 = 234;
  const int64_t latency_sequence_number1 = 5645432;
  const ui::LatencyComponentType latency_type2 = ui::TAB_SHOW_COMPONENT;
  const int64_t latency_id2 = 31434351;
  const int64_t latency_sequence_number2 = 663788;

  // Submit a frame with latency info
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1, latency_id1, latency_sequence_number1);

  CompositorFrame frame = MakeCompositorFrame();
  frame.metadata.latency_info.push_back(info);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame));

  // Verify that the old surface has an active frame and no pending frame.
  Surface* old_surface = surface_manager().GetSurfaceForId(parent_id1);
  ASSERT_NE(nullptr, old_surface);
  EXPECT_TRUE(old_surface->HasActiveFrame());
  EXPECT_FALSE(old_surface->HasPendingFrame());

  // Submit another frame with some other latency info and a different
  // LocalSurfaceId.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2, latency_id2, latency_sequence_number2);

  CompositorFrame frame2 = MakeCompositorFrame();
  frame2.metadata.latency_info.push_back(info2);

  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         std::move(frame2));

  // Verify that the new surface has an active frame and no pending frames.
  Surface* surface = surface_manager().GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Verify that the new surface has both latency info elements.
  std::vector<ui::LatencyInfo> info_list;
  surface->TakeLatencyInfo(&info_list);
  EXPECT_EQ(2u, info_list.size());

  ui::LatencyInfo aggregated_latency_info = info_list[0];
  aggregated_latency_info.AddNewLatencyFrom(info_list[1]);
  EXPECT_EQ(2u, aggregated_latency_info.latency_components().size());

  ui::LatencyInfo::LatencyComponent comp1;
  EXPECT_TRUE(
      aggregated_latency_info.FindLatency(latency_type1, latency_id1, &comp1));
  EXPECT_EQ(latency_sequence_number1, comp1.sequence_number);
}

// Checks whether the latency info are moved to the new surface from the old
// one when LocalSurfaceId changes. Old surface has unresolved dependencies.
TEST_F(CompositorFrameSinkSupportTest,
       LatencyInfoCarriedOverOnResize_OldSurfaceHasPendingAndActiveFrame) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  const ui::LatencyComponentType latency_type1 =
      ui::BROWSER_SNAPSHOT_FRAME_NUMBER_COMPONENT;
  const int64_t latency_id1 = 234;
  const int64_t latency_sequence_number1 = 5645432;
  const ui::LatencyComponentType latency_type2 = ui::TAB_SHOW_COMPONENT;
  const int64_t latency_id2 = 31434351;
  const int64_t latency_sequence_number2 = 663788;

  // Submit a frame with no unresolved dependecy.
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1, latency_id1, latency_sequence_number1);

  CompositorFrame frame = MakeCompositorFrame();
  frame.metadata.latency_info.push_back(info);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame));

  // Submit a frame with unresolved dependencies.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2, latency_id2, latency_sequence_number2);

  CompositorFrame frame2 = MakeCompositorFrame({child_id});
  frame2.metadata.latency_info.push_back(info2);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame2));

  // Verify that the old surface has both an active and a pending frame.
  Surface* old_surface = surface_manager().GetSurfaceForId(parent_id1);
  ASSERT_NE(nullptr, old_surface);
  EXPECT_TRUE(old_surface->HasActiveFrame());
  EXPECT_TRUE(old_surface->HasPendingFrame());

  // Submit a frame with a new local surface id.
  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         MakeCompositorFrame());

  // Verify that the new surface has an active frame only.
  Surface* surface = surface_manager().GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Verify that the new surface has latency info from both active and pending
  // frame of the old surface.
  std::vector<ui::LatencyInfo> info_list;
  surface->TakeLatencyInfo(&info_list);
  EXPECT_EQ(2u, info_list.size());

  ui::LatencyInfo aggregated_latency_info = info_list[0];
  aggregated_latency_info.AddNewLatencyFrom(info_list[1]);
  EXPECT_EQ(2u, aggregated_latency_info.latency_components().size());

  ui::LatencyInfo::LatencyComponent comp1;
  EXPECT_TRUE(
      aggregated_latency_info.FindLatency(latency_type1, latency_id1, &comp1));
  EXPECT_EQ(latency_sequence_number1, comp1.sequence_number);
}

// Checks whether the latency info are moved to the new surface from the old
// one when LocalSurfaceId changes. The new surface has unresolved dependencies.
TEST_F(CompositorFrameSinkSupportTest,
       LatencyInfoCarriedOverOnResize_NewSurfaceHasPendingFrame) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  const ui::LatencyComponentType latency_type1 =
      ui::BROWSER_SNAPSHOT_FRAME_NUMBER_COMPONENT;
  const int64_t latency_id1 = 234;
  const int64_t latency_sequence_number1 = 5645432;
  const ui::LatencyComponentType latency_type2 = ui::TAB_SHOW_COMPONENT;
  const int64_t latency_id2 = 31434351;
  const int64_t latency_sequence_number2 = 663788;

  // Submit a frame with no unresolved dependencies.
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1, latency_id1, latency_sequence_number1);

  CompositorFrame frame = MakeCompositorFrame();
  frame.metadata.latency_info.push_back(info);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame));

  // Verify that the old surface has an active frame only.
  Surface* old_surface = surface_manager().GetSurfaceForId(parent_id1);
  ASSERT_NE(nullptr, old_surface);
  EXPECT_TRUE(old_surface->HasActiveFrame());
  EXPECT_FALSE(old_surface->HasPendingFrame());

  // Submit a frame with a new local surface id and with unresolved
  // dependencies.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2, latency_id2, latency_sequence_number2);

  CompositorFrame frame2 = MakeCompositorFrame({child_id});
  frame2.metadata.latency_info.push_back(info2);

  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         std::move(frame2));

  // Verify that the new surface has a pending frame and no active frame.
  Surface* surface = surface_manager().GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasPendingFrame());
  EXPECT_FALSE(surface->HasActiveFrame());

  // Resolve the dependencies. The frame in parent's surface must become active.
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         MakeCompositorFrame());
  EXPECT_FALSE(surface->HasPendingFrame());
  EXPECT_TRUE(surface->HasActiveFrame());

  // Both latency info elements must exist in the now-activated frame of the
  // new surface.
  std::vector<ui::LatencyInfo> info_list;
  surface->TakeLatencyInfo(&info_list);
  EXPECT_EQ(2u, info_list.size());

  ui::LatencyInfo aggregated_latency_info = info_list[0];
  aggregated_latency_info.AddNewLatencyFrom(info_list[1]);
  EXPECT_EQ(2u, aggregated_latency_info.latency_components().size());

  ui::LatencyInfo::LatencyComponent comp1;
  EXPECT_TRUE(
      aggregated_latency_info.FindLatency(latency_type1, latency_id1, &comp1));
  EXPECT_EQ(latency_sequence_number1, comp1.sequence_number);
}

TEST_F(CompositorFrameSinkSupportTest, PassesOnBeginFrameAcks) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);

  // Request BeginFrames.
  display_support().SetNeedsBeginFrame(true);

  // Issue a BeginFrame.
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_source()->TestOnBeginFrame(args);

  // Check that the support forwards a BeginFrameDidNotSwap ack to the
  // BeginFrameSource.
  BeginFrameAck ack(0, 1, 1, false);
  display_support().BeginFrameDidNotSwap(ack);
  EXPECT_EQ(ack, begin_frame_source()->LastAckForObserver(&display_support()));

  // Issue another BeginFrame.
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  begin_frame_source()->TestOnBeginFrame(args);

  // Check that the support forwards the BeginFrameAck attached
  // to a CompositorFrame to the BeginFrameSource.
  BeginFrameAck ack2(0, 2, 2, true);
  CompositorFrame frame = MakeCompositorFrame();
  frame.metadata.begin_frame_ack = ack2;
  display_support().SubmitCompositorFrame(display_id.local_surface_id(),
                                          std::move(frame));
  EXPECT_EQ(ack2, begin_frame_source()->LastAckForObserver(&display_support()));
}

// Checks that resources and ack are sent together if possible.
TEST_F(CompositorFrameSinkSupportTest, ReturnResourcesWithAck) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  TransferableResource resource;
  resource.id = 1234;
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrameWithResources(empty_surface_ids(), {resource}));
  ReturnedResourceArray returned_resources;
  TransferableResource::ReturnResources({resource}, &returned_resources);
  EXPECT_CALL(support_client_, ReclaimResources(_)).Times(0);
  EXPECT_CALL(support_client_,
              DidReceiveCompositorFrameAck(Eq(returned_resources)));
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame());
}

// Verifies that if a surface is marked destroyed and a new frame arrives for
// it, it will be recovered.
TEST_F(CompositorFrameSinkSupportTest, SurfaceResurrection) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 3);

  // Add a reference from the parent to the child.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id}));

  // Create the child surface by submitting a frame to it.
  EXPECT_EQ(nullptr, surface_manager().GetSurfaceForId(child_id));
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         MakeCompositorFrame());

  // Verify that the child surface is created.
  Surface* surface = surface_manager().GetSurfaceForId(child_id);
  EXPECT_NE(nullptr, surface);

  // Attempt to destroy the child surface. The surface must still exist since
  // the parent needs it but it will be marked as destroyed.
  child_support1().EvictFrame();
  surface = surface_manager().GetSurfaceForId(child_id);
  EXPECT_NE(nullptr, surface);
  EXPECT_TRUE(surface->destroyed());

  // Child submits another frame to the same local surface id that is marked
  // destroyed.
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         MakeCompositorFrame());

  // Verify that the surface that was marked destroyed is recovered and is being
  // used again.
  Surface* surface2 = surface_manager().GetSurfaceForId(child_id);
  EXPECT_EQ(surface, surface2);
  EXPECT_FALSE(surface2->destroyed());
}

// Verifies that if a LocalSurfaceId belonged to a surface that doesn't exist
// anymore, it can still be reused for new surfaces.
TEST_F(CompositorFrameSinkSupportTest, LocalSurfaceIdIsReusable) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 3);

  // Add a reference from parent.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame({child_id}));

  // Submit the first frame. Creates the surface.
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         MakeCompositorFrame());
  EXPECT_NE(nullptr, surface_manager().GetSurfaceForId(child_id));

  // Remove the reference from parant. This allows us to destroy the surface.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         MakeCompositorFrame());

  // Destroy the surface.
  child_support1().EvictFrame();
  EXPECT_EQ(nullptr, surface_manager().GetSurfaceForId(child_id));

  // Submit another frame with the same local surface id. This should work fine
  // and a new surface must be created.
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         MakeCompositorFrame());
  EXPECT_NE(nullptr, surface_manager().GetSurfaceForId(child_id));
}

// This test verifies that a crash does not occur if garbage collection is
// triggered during surface dependency resolution. This test triggers garbage
// collection during surface resolution, by causing an activation to remove
// a surface subtree from the root. Both the old subtree and the new
// activated subtree refer to the same dependency. The old subtree was activated
// by deadline, and the new subtree was activated by a dependency finally
// resolving.
TEST_F(CompositorFrameSinkSupportTest, DependencyTrackingGarbageCollection) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id}));
  display_support().SubmitCompositorFrame(display_id.local_surface_id(),
                                          MakeCompositorFrame({parent_id1}));

  EXPECT_TRUE(dependency_tracker().has_deadline());

  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Advance BeginFrames to trigger a deadline.
  for (int i = 0; i < 3; ++i) {
    begin_frame_source()->TestOnBeginFrame(args);
    EXPECT_TRUE(dependency_tracker().has_deadline());
  }
  begin_frame_source()->TestOnBeginFrame(args);
  EXPECT_FALSE(dependency_tracker().has_deadline());

  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());

  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         MakeCompositorFrame({child_id}));
  display_support().SubmitCompositorFrame(display_id.local_surface_id(),
                                          MakeCompositorFrame({parent_id2}));

  // The display surface now has two CompositorFrames. One that is pending,
  // indirectly blocked on child_id and one that is active, also indirectly
  // referring to child_id, but activated due to the deadline above.
  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(display_surface()->HasPendingFrame());

  // Submitting a CompositorFrame will trigger garbage collection of the
  // |parent_id1| subtree. This should not crash.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(), MakeCompositorFrame(empty_surface_ids()));
}

// This test verifies that a crash does not occur if garbage collection is
// triggered when a deadline forces frame activation. This test triggers garbage
// collection during deadline activation by causing the activation of a display
// frame to replace a previously activated display frame that was referring to
// a now-unreachable surface subtree. That subtree gets garbage collected during
// deadline activation. SurfaceDependencyTracker is also tracking a surface
// from that subtree due to an unresolved dependency. This test verifies that
// this dependency resolution does not crash.
TEST_F(CompositorFrameSinkSupportTest, GarbageCollectionOnDeadline) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  display_support().SubmitCompositorFrame(display_id.local_surface_id(),
                                          MakeCompositorFrame({parent_id1}));

  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(dependency_tracker().has_deadline());
  EXPECT_FALSE(display_surface()->HasActiveFrame());

  // Advance BeginFrames to trigger a deadline. This activates the
  // CompositorFrame submitted above.
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  for (int i = 0; i < 3; ++i) {
    begin_frame_source()->TestOnBeginFrame(args);
    EXPECT_TRUE(dependency_tracker().has_deadline());
  }
  begin_frame_source()->TestOnBeginFrame(args);
  EXPECT_FALSE(dependency_tracker().has_deadline());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());

  // |parent_id1| is blocked on |child_id|, but |display_id|'s CompositorFrame
  // has activated due to a deadline. |parent_id1| will be tracked by the
  // SurfaceDependencyTracker.
  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id}));

  // By submitting a display CompositorFrame, and replacing the parent's
  // CompositorFrame with another surface ID, parent_id1 becomes unreachable and
  // a candidate for garbage collection.
  display_support().SubmitCompositorFrame(display_id.local_surface_id(),
                                          MakeCompositorFrame({parent_id2}));

  // Now |parent_id1| is only kept alive by the active |display_id| frame.
  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         MakeCompositorFrame({child_id}));

  // SurfaceDependencyTracker should now be tracking |display_id|, |parent_id1|
  // and |parent_id2|. By activating the pending |display_id| frame by deadline,
  // |parent_id1| becomes unreachable and is garbage collected while
  // SurfaceDependencyTracker is in the process of activating surfaces. This
  // should not cause a crash or use-after-free.
  for (int i = 0; i < 3; ++i) {
    begin_frame_source()->TestOnBeginFrame(args);
    EXPECT_TRUE(dependency_tracker().has_deadline());
  }
  begin_frame_source()->TestOnBeginFrame(args);
  EXPECT_FALSE(dependency_tracker().has_deadline());
}

// This test verifies that a CompositorFrame will only blocked on embedded
// surfaces but not on other retained surface IDs in the CompositorFrame.
TEST_F(CompositorFrameSinkSupportTest, OnlyBlockOnEmbeddedSurfaces) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);

  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id1}, {parent_id1, parent_id2}));

  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_FALSE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(dependency_tracker().has_deadline());

  // Verify that the display CompositorFrame will only block on |parent_id1| but
  // not |parent_id2|.
  EXPECT_THAT(display_surface()->blocking_surfaces(),
              UnorderedElementsAre(parent_id1));
  // Verify that the display surface holds no references while its
  // CompositorFrame is pending.
  EXPECT_THAT(GetChildReferences(display_id), IsEmpty());

  // Submitting a CompositorFrame with |parent_id1| should unblock the display
  // CompositorFrame.
  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         MakeCompositorFrame());

  EXPECT_FALSE(dependency_tracker().has_deadline());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_THAT(display_surface()->blocking_surfaces(), IsEmpty());

  // Only a reference to |parent_id1| is added because |parent_id2| does not
  // exist.
  EXPECT_THAT(GetChildReferences(display_id), UnorderedElementsAre(parent_id1));
}

// This test verifies that a late arriving CompositorFrame activates immediately
// and does not trigger a new deadline.
TEST_F(CompositorFrameSinkSupportTest, LateArrivingDependency) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);

  display_support().SubmitCompositorFrame(display_id.local_surface_id(),
                                          MakeCompositorFrame({parent_id1}));

  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_FALSE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(dependency_tracker().has_deadline());

  // Advance BeginFrames to trigger a deadline. This activates the
  // CompositorFrame submitted above.
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  for (int i = 0; i < 3; ++i) {
    begin_frame_source()->TestOnBeginFrame(args);
    EXPECT_TRUE(dependency_tracker().has_deadline());
  }
  begin_frame_source()->TestOnBeginFrame(args);
  EXPECT_FALSE(dependency_tracker().has_deadline());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());

  // A late arriving CompositorFrame should activate immediately without
  // scheduling a deadline and without waiting for dependencies to resolve.
  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         MakeCompositorFrame({child_id1}));
  EXPECT_FALSE(dependency_tracker().has_deadline());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
}

}  // namespace test
}  // namespace cc
