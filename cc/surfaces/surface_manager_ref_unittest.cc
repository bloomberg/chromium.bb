// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <unordered_map>
#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace cc {

namespace {

constexpr FrameSinkId kFrameSink1(1, 0);
constexpr FrameSinkId kFrameSink2(2, 0);
constexpr FrameSinkId kFrameSink3(3, 0);

class StubSurfaceFactoryClient : public SurfaceFactoryClient {
 public:
  void ReturnResources(const ReturnedResourceArray& resources) override {}
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {}
};

}  // namespace

// Tests for reference tracking in SurfaceManager.
class SurfaceManagerRefTest : public testing::Test {
 public:
  SurfaceManager& manager() { return *manager_; }

  // Creates a new Surface with the provided |frame_sink_id| and |local_id|.
  // Will first create a SurfaceFactory for |frame_sink_id| if necessary.
  SurfaceId CreateSurface(const FrameSinkId& frame_sink_id, uint32_t local_id) {
    LocalSurfaceId local_surface_id(local_id,
                                    base::UnguessableToken::Deserialize(0, 1u));
    GetFactory(frame_sink_id)
        .SubmitCompositorFrame(local_surface_id, CompositorFrame(),
                               SurfaceFactory::DrawCallback());
    return SurfaceId(frame_sink_id, local_surface_id);
  }

  // Destroy Surface with |surface_id|.
  void DestroySurface(const SurfaceId& surface_id) {
    GetFactory(surface_id.frame_sink_id()).EvictSurface();
  }

  SurfaceFactory& GetFactory(const FrameSinkId& frame_sink_id) {
    auto& factory_ptr = factories_[frame_sink_id];
    if (!factory_ptr)
      factory_ptr = base::MakeUnique<SurfaceFactory>(frame_sink_id,
                                                     manager_.get(), &client_);
    return *factory_ptr;
  }

  void RemoveSurfaceReference(const SurfaceId& parent_id,
                              const SurfaceId& child_id) {
    manager_->RemoveSurfaceReferences({SurfaceReference(parent_id, child_id)});
  }

  void AddSurfaceReference(const SurfaceId& parent_id,
                           const SurfaceId& child_id) {
    manager_->AddSurfaceReferences({SurfaceReference(parent_id, child_id)});
  }

  // Returns all the references where |surface_id| is the parent.
  const SurfaceManager::SurfaceIdSet& GetReferencesFrom(
      const SurfaceId& surface_id) {
    return manager().parent_to_child_refs_[surface_id];
  }

  // Returns all the references where |surface_id| is the child.
  const SurfaceManager::SurfaceIdSet& GetReferencesFor(
      const SurfaceId& surface_id) {
    return manager().child_to_parent_refs_[surface_id];
  }

  // Temporary references are stored as a map in SurfaceManager. This method
  // converts the map to a vector.
  std::vector<SurfaceId> GetAllTempReferences() {
    std::vector<SurfaceId> temp_references;
    for (auto& map_entry : manager().temporary_references_)
      temp_references.push_back(map_entry.first);
    return temp_references;
  }

 protected:
  // testing::Test:
  void SetUp() override {
    // Start each test with a fresh SurfaceManager instance.
    manager_ = base::MakeUnique<SurfaceManager>(
        SurfaceManager::LifetimeType::REFERENCES);
  }
  void TearDown() override {
    for (auto& factory : factories_)
      factory.second->EvictSurface();
    factories_.clear();
    manager_.reset();
  }

  std::unordered_map<FrameSinkId,
                     std::unique_ptr<SurfaceFactory>,
                     FrameSinkIdHash>
      factories_;
  std::unique_ptr<SurfaceManager> manager_;
  StubSurfaceFactoryClient client_;
};

TEST_F(SurfaceManagerRefTest, AddReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(manager().GetRootSurfaceId(), id1);

  EXPECT_THAT(GetReferencesFor(id1),
              UnorderedElementsAre(manager().GetRootSurfaceId()));
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
}

TEST_F(SurfaceManagerRefTest, AddRemoveReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);

  EXPECT_THAT(GetReferencesFor(id1),
              UnorderedElementsAre(manager().GetRootSurfaceId()));
  EXPECT_THAT(GetReferencesFor(id2), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFrom(id1), UnorderedElementsAre(id2));
  EXPECT_THAT(GetReferencesFrom(id2), IsEmpty());

  RemoveSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id1), SizeIs(1));
  EXPECT_THAT(GetReferencesFor(id2), IsEmpty());
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
  EXPECT_THAT(GetReferencesFrom(id2), IsEmpty());
}

TEST_F(SurfaceManagerRefTest, NewSurfaceFromFrameSink) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  AddSurfaceReference(id2, id3);

  // |kFramesink2| received a CompositorFrame with a new size, so it destroys
  // |id2| and creates |id2_next|. No reference have been removed yet.
  SurfaceId id2_next = CreateSurface(kFrameSink2, 2);
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2_next));

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
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2_next));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id3));
}

TEST_F(SurfaceManagerRefTest, ReferenceCycleGetsDeleted) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  AddSurfaceReference(id2, id3);

  // This reference forms a cycle between id2 and id3.
  AddSurfaceReference(id3, id2);

  DestroySurface(id3);
  DestroySurface(id2);
  DestroySurface(id1);

  RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);

  // Removing the reference from the root to id1 should allow all three surfaces
  // to be deleted during GC even with a cycle between 2 and 3.
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id3));
}

TEST_F(SurfaceManagerRefTest, SurfacesAreDeletedDuringGarbageCollection) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);

  EXPECT_NE(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  DestroySurface(id1);
  DestroySurface(id2);
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));

  // Should delete |id2| when the only reference to it is removed.
  RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id2));

  // Should delete |id1| when the only reference to it is removed.
  RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id1));
}

TEST_F(SurfaceManagerRefTest, GarbageCollectionWorksRecusively) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  AddSurfaceReference(id2, id3);

  DestroySurface(id3);
  DestroySurface(id2);
  DestroySurface(id1);

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id3));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id1));

  RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);

  // Removing the reference from the root to id1 should allow all three surfaces
  // to be deleted during GC.
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id3));
}

TEST_F(SurfaceManagerRefTest, TryAddReferenceSameReferenceTwice) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  AddSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));

  // The second request should be ignored without crashing.
  AddSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));
}

TEST_F(SurfaceManagerRefTest, AddingSelfReferenceFails) {
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

TEST_F(SurfaceManagerRefTest, RemovingNonexistantReferenceFails) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  // Removing non-existent reference should be ignored.
  AddSurfaceReference(id1, id2);
  RemoveSurfaceReference(id2, id1);
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
}

TEST_F(SurfaceManagerRefTest, AddSurfaceThenReference) {
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

TEST_F(SurfaceManagerRefTest, AddSurfaceThenRootReference) {
  // Create a new surface.
  const SurfaceId surface_id = CreateSurface(kFrameSink1, 1);

  // Temporary reference should be added to |surface_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(surface_id));

  // Add a real reference from root to |surface_id|.
  AddSurfaceReference(manager().GetRootSurfaceId(), surface_id);

  // The temporary reference should be gone and there should now be a surface
  // reference from root to |surface_id|.
  EXPECT_TRUE(GetAllTempReferences().empty());
  EXPECT_THAT(GetReferencesFrom(manager().GetRootSurfaceId()),
              ElementsAre(surface_id));
}

TEST_F(SurfaceManagerRefTest, AddTwoSurfacesThenOneReference) {
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

TEST_F(SurfaceManagerRefTest, AddSurfacesSkipReference) {
  // Add two surfaces that have the same FrameSinkId. This would happen when a
  // client submits two CompositorFrames before parent submits a new
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

TEST_F(SurfaceManagerRefTest, RemoveFirstTempReferenceOnly) {
  // Add two surfaces that have the same FrameSinkId. This would happen when a
  // client submits two CFs before parent submits a new CF.
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

TEST_F(SurfaceManagerRefTest, SurfaceWithTemporaryReferenceIsNotDeleted) {
  const SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(manager().GetRootSurfaceId(), id1);

  // We create |id2| and never add a real reference to it. This leaves the
  // temporary reference.
  const SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id2));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));

  // Destroy both surfaces so they can be garbage collected. We remove the
  // surface reference to |id1| which will run GarbageCollectSurfaces().
  DestroySurface(id1);
  DestroySurface(id2);
  RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);

  // |id1| is destroyed and has no references, so it's deleted.
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id1));

  // |id2| is destroyed but has a temporary reference, it's not deleted.
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));
}

// Checks that when a temporary reference is assigned an owner, if the owner is
// invalidated then the temporary reference is also removed.
TEST_F(SurfaceManagerRefTest, InvalidateTempReferenceOwnerRemovesReference) {
  // Surface |id1| should have a temporary reference on creation.
  const SurfaceId id1 = CreateSurface(kFrameSink2, 1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // |id1| should have a temporary reference after an owner is assigned.
  manager().AssignTemporaryReference(id1, kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // When |kFrameSink1| is invalidated the temporary reference will be removed.
  manager().InvalidateFrameSinkId(kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
}

// Checks that adding a surface reference clears the temporary reference and
// ownership. Invalidating the old owner shouldn't do anything.
TEST_F(SurfaceManagerRefTest, InvalidateHasNoEffectOnSurfaceReferences) {
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(manager().GetRootSurfaceId(), parent_id);

  const SurfaceId id1 = CreateSurface(kFrameSink2, 1);
  manager().AssignTemporaryReference(id1, kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // Adding a real surface reference will remove the temporary reference.
  AddSurfaceReference(parent_id, id1);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
  ASSERT_THAT(GetReferencesFor(id1), UnorderedElementsAre(parent_id));

  // When |kFrameSink1| is invalidated it shouldn't change the surface
  // references.
  manager().InvalidateFrameSinkId(kFrameSink1);
  ASSERT_THAT(GetReferencesFor(id1), UnorderedElementsAre(parent_id));
}

TEST_F(SurfaceManagerRefTest, CheckDropTemporaryReferenceWorks) {
  const SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1));

  // An example of why this could happen is the window server doesn't know the
  // owner, maybe it has crashed and been cleanup already, and asks to drop the
  // temporary reference.
  manager().DropTemporaryReference(id1);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
}

// Checks that we handle ownership and temporary references correctly when there
// are multiple temporary references. This tests something like the parent
// client crashing, so it's
TEST_F(SurfaceManagerRefTest, TempReferencesWithClientCrash) {
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  AddSurfaceReference(manager().GetRootSurfaceId(), parent_id);

  const SurfaceId id1a = CreateSurface(kFrameSink2, 1);
  const SurfaceId id1b = CreateSurface(kFrameSink2, 2);

  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1a, id1b));

  // Assign |id1a| to |kFrameSink1|. This doesn't change the temporary
  // reference, it just assigns as owner to it.
  manager().AssignTemporaryReference(id1a, kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1a, id1b));

  // If the parent client crashes then the CompositorFrameSink connection will
  // be closed and the FrameSinkId invalidated. The temporary reference
  // |kFrameSink1| owns to |id2a| will be removed.
  manager().InvalidateFrameSinkId(kFrameSink1);
  ASSERT_THAT(GetAllTempReferences(), UnorderedElementsAre(id1b));

  // If the parent has crashed then the window server will have already removed
  // it from the ServerWindow hierarchy and won't have an owner for |id2b|. The
  // window server will ask to drop the reference instead.
  manager().DropTemporaryReference(id1b);
  ASSERT_THAT(GetAllTempReferences(), IsEmpty());
}

}  // namespace cc
