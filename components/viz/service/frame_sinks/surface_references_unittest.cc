// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <unordered_map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/test/compositor_frame_helpers.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace viz {
namespace test {
namespace {

constexpr FrameSinkId kFrameSink1(1, 0);
constexpr FrameSinkId kFrameSink2(2, 0);
constexpr FrameSinkId kFrameSink3(3, 0);

}  // namespace

// Tests for reference tracking in CompositorFrameSinkSupport and
// cc::SurfaceManager.
class SurfaceReferencesTest : public testing::Test {
 public:
  cc::SurfaceManager& GetSurfaceManager() {
    return *manager_->surface_manager();
  }

  // Creates a new Surface with the provided |frame_sink_id| and |local_id|.
  // Will first create a Surfacesupport for |frame_sink_id| if necessary.
  SurfaceId CreateSurface(const FrameSinkId& frame_sink_id, uint32_t local_id) {
    LocalSurfaceId local_surface_id(local_id,
                                    base::UnguessableToken::Deserialize(0, 1u));
    GetCompositorFrameSinkSupport(frame_sink_id)
        .SubmitCompositorFrame(local_surface_id,
                               cc::test::MakeCompositorFrame());
    return SurfaceId(frame_sink_id, local_surface_id);
  }

  // Destroy Surface with |surface_id|.
  void DestroySurface(const SurfaceId& surface_id) {
    GetCompositorFrameSinkSupport(surface_id.frame_sink_id())
        .EvictCurrentSurface();
  }

  CompositorFrameSinkSupport& GetCompositorFrameSinkSupport(
      const FrameSinkId& frame_sink_id) {
    auto& support_ptr = supports_[frame_sink_id];
    if (!support_ptr) {
      constexpr bool is_root = false;
      constexpr bool handles_frame_sink_id_invalidation = true;
      constexpr bool needs_sync_points = true;
      support_ptr = CompositorFrameSinkSupport::Create(
          nullptr, manager_.get(), frame_sink_id, is_root,
          handles_frame_sink_id_invalidation, needs_sync_points);
    }
    return *support_ptr;
  }

  void DestroyCompositorFrameSinkSupport(const FrameSinkId& frame_sink_id) {
    auto support_ptr = supports_.find(frame_sink_id);
    ASSERT_NE(support_ptr, supports_.end());
    supports_.erase(support_ptr);
  }

  void RemoveSurfaceReference(const SurfaceId& parent_id,
                              const SurfaceId& child_id) {
    manager_->surface_manager()->RemoveSurfaceReferences(
        {cc::SurfaceReference(parent_id, child_id)});
  }

  void AddSurfaceReference(const SurfaceId& parent_id,
                           const SurfaceId& child_id) {
    manager_->surface_manager()->AddSurfaceReferences(
        {cc::SurfaceReference(parent_id, child_id)});
  }

  // Returns all the references where |surface_id| is the parent.
  const base::flat_set<SurfaceId>& GetReferencesFrom(
      const SurfaceId& surface_id) {
    return GetSurfaceManager().GetSurfacesReferencedByParent(surface_id);
  }

  // Returns all the references where |surface_id| is the child.
  const base::flat_set<SurfaceId>& GetReferencesFor(
      const SurfaceId& surface_id) {
    return GetSurfaceManager().GetSurfacesThatReferenceChild(surface_id);
  }

  // Temporary references are stored as a map in cc::SurfaceManager. This method
  // converts the map to a vector.
  std::vector<SurfaceId> GetAllTempReferences() {
    std::vector<SurfaceId> temp_references;
    for (auto& map_entry : GetSurfaceManager().temporary_references_)
      temp_references.push_back(map_entry.first);
    return temp_references;
  }

 protected:
  // testing::Test:
  void SetUp() override {
    // Start each test with a fresh cc::SurfaceManager instance.
    manager_ = base::MakeUnique<FrameSinkManagerImpl>(
        nullptr /* display_provider */,
        cc::SurfaceManager::LifetimeType::REFERENCES);
  }
  void TearDown() override {
    for (auto& support : supports_)
      support.second->EvictCurrentSurface();
    supports_.clear();
    manager_.reset();
  }

  std::unordered_map<FrameSinkId,
                     std::unique_ptr<CompositorFrameSinkSupport>,
                     FrameSinkIdHash>
      supports_;
  std::unique_ptr<FrameSinkManagerImpl> manager_;
};

TEST_F(SurfaceReferencesTest, AddReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);

  EXPECT_THAT(GetReferencesFor(id1),
              UnorderedElementsAre(GetSurfaceManager().GetRootSurfaceId()));
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
}

TEST_F(SurfaceReferencesTest, AddRemoveReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);

  EXPECT_THAT(GetReferencesFor(id1),
              UnorderedElementsAre(GetSurfaceManager().GetRootSurfaceId()));
  EXPECT_THAT(GetReferencesFor(id2), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFrom(id1), UnorderedElementsAre(id2));
  EXPECT_THAT(GetReferencesFrom(id2), IsEmpty());

  RemoveSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id1), SizeIs(1));
  EXPECT_THAT(GetReferencesFor(id2), IsEmpty());
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
  EXPECT_THAT(GetReferencesFrom(id2), IsEmpty());
}

TEST_F(SurfaceReferencesTest, NewSurfaceFromFrameSink) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  AddSurfaceReference(id2, id3);

  // |kFramesink2| received a CompositorFrame with a new size, so it destroys
  // |id2| and creates |id2_next|. No reference have been removed yet.
  SurfaceId id2_next = CreateSurface(kFrameSink2, 2);
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2_next));

  // Add references to and from |id2_next|.
  AddSurfaceReference(id1, id2_next);
  AddSurfaceReference(id2_next, id3);
  EXPECT_THAT(GetReferencesFor(id2), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFor(id2_next), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFor(id3), UnorderedElementsAre(id2, id2_next));

  RemoveSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id2), IsEmpty());
  EXPECT_THAT(GetReferencesFor(id2_next), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFor(id3), UnorderedElementsAre(id2_next));

  // |id2| should be deleted during GC but other surfaces shouldn't.
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2_next));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id3));
}

TEST_F(SurfaceReferencesTest, ReferenceCycleGetsDeleted) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  AddSurfaceReference(id2, id3);

  // This reference forms a cycle between id2 and id3.
  AddSurfaceReference(id3, id2);

  DestroySurface(id3);
  DestroySurface(id2);
  DestroySurface(id1);

  RemoveSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);

  // Removing the reference from the root to id1 should allow all three surfaces
  // to be deleted during GC even with a cycle between 2 and 3.
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id1));
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id2));
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id3));
}

TEST_F(SurfaceReferencesTest, SurfacesAreDeletedDuringGarbageCollection) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);

  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id1));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2));

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  DestroySurface(id1);
  DestroySurface(id2);
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id1));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2));

  // Should delete |id2| when the only reference to it is removed.
  RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id2));

  // Should delete |id1| when the only reference to it is removed.
  RemoveSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id1));
}

TEST_F(SurfaceReferencesTest, GarbageCollectionWorksRecusively) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  AddSurfaceReference(id2, id3);

  DestroySurface(id3);
  DestroySurface(id2);
  DestroySurface(id1);

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id3));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id1));

  RemoveSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);

  // Removing the reference from the root to id1 should allow all three surfaces
  // to be deleted during GC.
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id1));
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id2));
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id3));
}

TEST_F(SurfaceReferencesTest, TryAddReferenceSameReferenceTwice) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));

  // The second request should be ignored without crashing.
  AddSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));
}

TEST_F(SurfaceReferencesTest, AddingSelfReferenceFails) {
  SurfaceId id1 = CreateSurface(kFrameSink2, 1);

  // A temporary reference must exist to |id1|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(id1));
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
  EXPECT_THAT(GetReferencesFor(id1), IsEmpty());

  // Try to add a self reference. This should fail.
  AddSurfaceReference(id1, id1);

  // Adding a self reference should be ignored without crashing. The temporary
  // reference to |id1| must still exist.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(id1));
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
  EXPECT_THAT(GetReferencesFor(id1), IsEmpty());
}

TEST_F(SurfaceReferencesTest, RemovingNonexistantReferenceFails) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  // Removing non-existent reference should be ignored.
  AddSurfaceReference(id1, id2);
  RemoveSurfaceReference(id2, id1);
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
}

TEST_F(SurfaceReferencesTest, AddSurfaceThenReference) {
  // Create a new surface.
  const SurfaceId surface_id = CreateSurface(kFrameSink2, 1);

  // A temporary reference must be added to |surface_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(surface_id));

  // Create |parent_id| and add a real reference from it to |surface_id|.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(parent_id, surface_id);

  // The temporary reference to |surface_id| should be gone.
  // The only temporary reference should be to |parent_id|.
  // There must be a real reference from |parent_id| to |child_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(parent_id));
  EXPECT_THAT(GetReferencesFrom(parent_id), ElementsAre(surface_id));
}

TEST_F(SurfaceReferencesTest, AddSurfaceThenRootReference) {
  // Create a new surface.
  const SurfaceId surface_id = CreateSurface(kFrameSink1, 1);

  // Temporary reference should be added to |surface_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(surface_id));

  // Add a real reference from root to |surface_id|.
  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), surface_id);

  // The temporary reference should be gone and there should now be a surface
  // reference from root to |surface_id|.
  EXPECT_TRUE(GetAllTempReferences().empty());
  EXPECT_THAT(GetReferencesFrom(GetSurfaceManager().GetRootSurfaceId()),
              ElementsAre(surface_id));
}

TEST_F(SurfaceReferencesTest, AddTwoSurfacesThenOneReference) {
  // Create two surfaces with different FrameSinkIds.
  const SurfaceId surface_id1 = CreateSurface(kFrameSink2, 1);
  const SurfaceId surface_id2 = CreateSurface(kFrameSink3, 1);

  // Temporary reference should be added for both surfaces.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(surface_id1, surface_id2));

  // Create |parent_id| and add a real reference from it to |surface_id1|.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(parent_id, surface_id1);

  // Real reference must be added to |surface_id1| and the temporary reference
  // to it must be gone.
  // There should still be a temporary reference left to |surface_id2|.
  // A temporary reference to |parent_id| must be created.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(parent_id, surface_id2));
  EXPECT_THAT(GetReferencesFrom(parent_id), ElementsAre(surface_id1));
}

TEST_F(SurfaceReferencesTest, AddSurfacesSkipReference) {
  // Add two surfaces that have the same FrameSinkId. This would happen
  // when a client submits two CompositorFrames before parent submits a new
  // CompositorFrame.
  const SurfaceId surface_id1 = CreateSurface(kFrameSink2, 2);
  const SurfaceId surface_id2 = CreateSurface(kFrameSink2, 1);

  // Temporary references should be added for both surfaces and they should be
  // stored in the order of creation.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(surface_id1, surface_id2));

  // Create |parent_id| and add a reference from it to |surface_id2| which was
  // created later.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(parent_id, surface_id2);

  // The real reference should be added for |surface_id2| and the temporary
  // references to both |surface_id1| and |surface_id2| should be gone.
  // There should be a temporary reference to |parent_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(parent_id));
  EXPECT_THAT(GetReferencesFrom(parent_id), ElementsAre(surface_id2));
}

TEST_F(SurfaceReferencesTest, RemoveFirstTempReferenceOnly) {
  // Add two surfaces that have the same FrameSinkId. This would happen
  // when a client submits two CFs before parent submits a new CF.
  const SurfaceId surface_id1 = CreateSurface(kFrameSink2, 1);
  const SurfaceId surface_id2 = CreateSurface(kFrameSink2, 2);

  // Temporary references should be added for both surfaces and they should be
  // stored in the order of creation.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(surface_id1, surface_id2));

  // Create |parent_id| and add a reference from it to |surface_id1| which was
  // created earlier.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(parent_id, surface_id1);

  // The real reference should be added for |surface_id1| and its temporary
  // reference should be removed. The temporary reference for |surface_id2|
  // should remain. A temporary reference must be added for |parent_id|.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(parent_id, surface_id2));
  EXPECT_THAT(GetReferencesFrom(parent_id), ElementsAre(surface_id1));
}

TEST_F(SurfaceReferencesTest, SurfaceWithTemporaryReferenceIsNotDeleted) {
  const SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);

  // We create |id2| and never add a real reference to it. This leaves the
  // temporary reference.
  const SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id2));
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2));

  // Destroy both surfaces so they can be garbage collected. We remove the
  // surface reference to |id1| which will run GarbageCollectSurfaces().
  DestroySurface(id1);
  DestroySurface(id2);
  RemoveSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), id1);

  // |id1| is destroyed and has no references, so it's deleted.
  EXPECT_EQ(nullptr, GetSurfaceManager().GetSurfaceForId(id1));

  // |id2| is destroyed but has a temporary reference, it's not deleted.
  EXPECT_NE(nullptr, GetSurfaceManager().GetSurfaceForId(id2));
}

// Checks that when a temporary reference is assigned an owner, if the owner is
// invalidated then the temporary reference is also removed.
TEST_F(SurfaceReferencesTest, InvalidateTempReferenceOwnerRemovesReference) {
  // Surface |id1| should have a temporary reference on creation.
  const SurfaceId id1 = CreateSurface(kFrameSink2, 1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // |id1| should have a temporary reference after an owner is assigned.
  GetSurfaceManager().AssignTemporaryReference(id1, kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // When |kFrameSink1| is invalidated the temporary reference will be removed.
  GetSurfaceManager().InvalidateFrameSinkId(kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
}

// Checks that adding a surface reference clears the temporary reference and
// ownership. Invalidating the old owner shouldn't do anything.
TEST_F(SurfaceReferencesTest, InvalidateHasNoEffectOnSurfaceReferences) {
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), parent_id);

  const SurfaceId id1 = CreateSurface(kFrameSink2, 1);
  GetSurfaceManager().AssignTemporaryReference(id1, kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // Adding a real surface reference will remove the temporary reference.
  AddSurfaceReference(parent_id, id1);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
  ASSERT_THAT(GetReferencesFor(id1), UnorderedElementsAre(parent_id));

  // When |kFrameSink1| is invalidated it shouldn't change the surface
  // references.
  DestroyCompositorFrameSinkSupport(kFrameSink1);
  ASSERT_THAT(GetReferencesFor(id1), UnorderedElementsAre(parent_id));
}

TEST_F(SurfaceReferencesTest, CheckDropTemporaryReferenceWorks) {
  const SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // An example of why this could happen is the window server doesn't know the
  // owner, maybe it has crashed and been cleanup already, and asks to drop the
  // temporary reference.
  GetSurfaceManager().DropTemporaryReference(id1);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
}

// Checks that we handle ownership and temporary references correctly when there
// are multiple temporary references. This tests something like the parent
// client crashing, so it's
TEST_F(SurfaceReferencesTest, TempReferencesWithClientCrash) {
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(GetSurfaceManager().GetRootSurfaceId(), parent_id);

  const SurfaceId id1a = CreateSurface(kFrameSink2, 1);
  const SurfaceId id1b = CreateSurface(kFrameSink2, 2);

  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1a, id1b));

  // Assign |id1a| to |kFrameSink1|. This doesn't change the temporary
  // reference, it just assigns as owner to it.
  GetSurfaceManager().AssignTemporaryReference(id1a, kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1a, id1b));

  // If the parent client crashes then the FrameSink connection will be closed
  // and the FrameSinkId invalidated. The temporary reference |kFrameSink1|
  // owns to |id2a| will be removed.
  DestroyCompositorFrameSinkSupport(kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1b));

  // If the parent has crashed then the window server will have already removed
  // it from the ServerWindow hierarchy and won't have an owner for |id2b|. The
  // window server will ask to drop the reference instead.
  GetSurfaceManager().DropTemporaryReference(id1b);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
}

}  // namespace test
}  // namespace viz
