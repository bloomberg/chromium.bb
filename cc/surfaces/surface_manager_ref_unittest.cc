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
    LocalFrameId local_frame_id(local_id,
                                base::UnguessableToken::Deserialize(0, 1u));
    GetFactory(frame_sink_id)
        .SubmitCompositorFrame(local_frame_id, CompositorFrame(),
                               SurfaceFactory::DrawCallback());
    return SurfaceId(frame_sink_id, local_frame_id);
  }

  // Destroy Surface with |surface_id|.
  void DestroySurface(const SurfaceId& surface_id) {
    GetFactory(surface_id.frame_sink_id()).EvictSurface();
  }

 protected:
  SurfaceFactory& GetFactory(const FrameSinkId& frame_sink_id) {
    auto& factory_ptr = factories_[frame_sink_id];
    if (!factory_ptr)
      factory_ptr = base::MakeUnique<SurfaceFactory>(frame_sink_id,
                                                     manager_.get(), &client_);
    return *factory_ptr;
  }

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

  const SurfaceManager::SurfaceIdSet& GetReferencesFromRoot() {
    return GetReferencesFrom(manager().GetRootSurfaceId());
  }

  // Returns all the temporary references for the given frame sink id.
  std::vector<LocalFrameId> GetTempReferencesFor(
      const FrameSinkId& frame_sink_id) {
    return manager().temp_references_[frame_sink_id];
  }

  // Temporary references are stored as a map in SurfaceManager. This method
  // converts the map to a vector.
  std::vector<SurfaceId> GetAllTempReferences() {
    std::vector<SurfaceId> temp_references;
    for (auto& map_entry : manager().temp_references_) {
      for (auto local_frame_id : map_entry.second)
        temp_references.push_back(SurfaceId(map_entry.first, local_frame_id));
    }
    return temp_references;
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
  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);

  EXPECT_THAT(GetReferencesFor(id1),
              UnorderedElementsAre(manager().GetRootSurfaceId()));
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
}

TEST_F(SurfaceManagerRefTest, AddRemoveReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);

  EXPECT_THAT(GetReferencesFor(id1),
              UnorderedElementsAre(manager().GetRootSurfaceId()));
  EXPECT_THAT(GetReferencesFor(id2), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFrom(id1), UnorderedElementsAre(id2));
  EXPECT_THAT(GetReferencesFrom(id2), IsEmpty());

  manager().RemoveSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id1), SizeIs(1));
  EXPECT_THAT(GetReferencesFor(id2), IsEmpty());
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());
  EXPECT_THAT(GetReferencesFrom(id2), IsEmpty());
}

TEST_F(SurfaceManagerRefTest, NewSurfaceFromFrameSink) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  manager().AddSurfaceReference(id2, id3);

  // |kFramesink2| received a CompositorFrame with a new size, so it destroys
  // |id2| and creates |id2_next|. No reference have been removed yet.
  SurfaceId id2_next = CreateSurface(kFrameSink2, 2);
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2_next));

  // Add references to and from |id2_next|.
  manager().AddSurfaceReference(id1, id2_next);
  manager().AddSurfaceReference(id2_next, id3);
  EXPECT_THAT(GetReferencesFor(id2), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFor(id2_next), UnorderedElementsAre(id1));
  EXPECT_THAT(GetReferencesFor(id3), UnorderedElementsAre(id2, id2_next));

  manager().RemoveSurfaceReference(id1, id2);
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

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  manager().AddSurfaceReference(id2, id3);

  // This reference forms a cycle between id2 and id3.
  manager().AddSurfaceReference(id3, id2);

  DestroySurface(id3);
  DestroySurface(id2);
  DestroySurface(id1);

  manager().RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);

  // Removing the reference from the root to id1 should allow all three surfaces
  // to be deleted during GC even with a cycle between 2 and 3.
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id3));
}

TEST_F(SurfaceManagerRefTest, CheckGC) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);

  EXPECT_NE(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  DestroySurface(id1);
  DestroySurface(id2);
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));

  // Should delete |id2| when the only reference to it is removed.
  manager().RemoveSurfaceReference(id1, id2);
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id2));

  // Should delete |id1| when the only reference to it is removed.
  manager().RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id1));
}

TEST_F(SurfaceManagerRefTest, CheckGCRecusiveFull) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);
  SurfaceId id3 = CreateSurface(kFrameSink3, 1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  manager().AddSurfaceReference(id2, id3);

  DestroySurface(id3);
  DestroySurface(id2);
  DestroySurface(id1);

  // Destroying the surfaces shouldn't delete them yet, since there is still an
  // active reference on all surfaces.
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id3));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_NE(nullptr, manager().GetSurfaceForId(id1));

  manager().RemoveSurfaceReference(manager().GetRootSurfaceId(), id1);

  // Removing the reference from the root to id1 should allow all three surfaces
  // to be deleted during GC.
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id1));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id2));
  EXPECT_EQ(nullptr, manager().GetSurfaceForId(id3));
}

TEST_F(SurfaceManagerRefTest, TryAddReferenceToBadSurface) {
  // Not creating an accompanying Surface and SurfaceFactory.
  SurfaceId id(FrameSinkId(100u, 200u),
               LocalFrameId(1u, base::UnguessableToken::Create()));

  // Adding reference from root to the Surface should do nothing because
  // SurfaceManager doesn't know Surface for |id| exists.
  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id);
  EXPECT_THAT(GetReferencesFor(id), IsEmpty());
}

TEST_F(SurfaceManagerRefTest, TryDoubleAddReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  manager().AddSurfaceReference(manager().GetRootSurfaceId(), id1);
  manager().AddSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));

  // The second request should be ignored without crashing.
  manager().AddSurfaceReference(id1, id2);
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));
}

TEST_F(SurfaceManagerRefTest, TryAddSelfReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);

  // A temporary reference must exist to |id1|.
  EXPECT_THAT(GetReferencesFor(id1), SizeIs(1));

  // Try to add a self reference. This should fail.
  manager().AddSurfaceReference(id1, id1);

  // Adding a self reference should be ignored without crashing.
  EXPECT_THAT(GetReferencesFrom(id1), IsEmpty());

  // The temporary reference to |id1| must still exist.
  EXPECT_THAT(GetReferencesFor(id1), SizeIs(1));
}

TEST_F(SurfaceManagerRefTest, TryRemoveBadReference) {
  SurfaceId id1 = CreateSurface(kFrameSink1, 1);
  SurfaceId id2 = CreateSurface(kFrameSink2, 1);

  // Removing non-existent reference should be ignored.
  manager().AddSurfaceReference(id1, id2);
  manager().RemoveSurfaceReference(id2, id1);
  EXPECT_THAT(GetReferencesFrom(id1), SizeIs(1));
  EXPECT_THAT(GetReferencesFor(id2), SizeIs(1));
}

TEST_F(SurfaceManagerRefTest, AddSurfaceThenReference) {
  // Create a new surface.
  const SurfaceId surface_id = CreateSurface(kFrameSink2, 1);

  // A temporary reference must be added to |surface_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(surface_id));
  EXPECT_THAT(GetReferencesFromRoot(), ElementsAre(surface_id));

  // Create |parent_id| and add a real reference from it to |surface_id|.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  manager().AddSurfaceReference(parent_id, surface_id);

  // The temporary reference to |surface_id| should be gone.
  // The only temporary reference should be to |parent_id|.
  // There must be a real reference from |parent_id| to |child_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(parent_id));
  EXPECT_THAT(GetReferencesFromRoot(), ElementsAre(parent_id));
  EXPECT_THAT(GetReferencesFrom(parent_id), ElementsAre(surface_id));
}

TEST_F(SurfaceManagerRefTest, AddSurfaceThenRootReference) {
  // Create a new surface.
  const SurfaceId surface_id = CreateSurface(kFrameSink1, 1);

  // Temporary reference should be added to |surface_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(surface_id));
  EXPECT_THAT(GetReferencesFromRoot(), ElementsAre(surface_id));

  // Add a real reference from root to |surface_id|.
  manager().AddSurfaceReference(manager().GetRootSurfaceId(), surface_id);

  // The temporary reference should be gone.
  // There should now be a real reference from root to |surface_id|.
  EXPECT_TRUE(GetAllTempReferences().empty());
  EXPECT_THAT(GetReferencesFromRoot(), ElementsAre(surface_id));
}

TEST_F(SurfaceManagerRefTest, AddTwoSurfacesThenOneReference) {
  // Create two surfaces with different FrameSinkIds.
  const SurfaceId surface_id1 = CreateSurface(kFrameSink2, 1);
  const SurfaceId surface_id2 = CreateSurface(kFrameSink3, 1);

  // Temporary reference should be added for both surfaces.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(surface_id1, surface_id2));
  EXPECT_THAT(GetReferencesFromRoot(),
              UnorderedElementsAre(surface_id1, surface_id2));

  // Create |parent_id| and add a real reference from it to |surface_id1|.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  manager().AddSurfaceReference(parent_id, surface_id1);

  // Real reference must be added to |surface_id1| and the temporary reference
  // to it must be gone.
  // There should still be a temporary reference left to |surface_id2|.
  // A temporary reference to |parent_id| must be created.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(parent_id, surface_id2));
  EXPECT_THAT(GetReferencesFromRoot(),
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
  EXPECT_THAT(
      GetTempReferencesFor(surface_id1.frame_sink_id()),
      ElementsAre(surface_id1.local_frame_id(), surface_id2.local_frame_id()));
  EXPECT_THAT(GetReferencesFromRoot(),
              UnorderedElementsAre(surface_id1, surface_id2));

  // Create |parent_id| and add a reference from it to |surface_id2| which was
  // created later.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  manager().AddSurfaceReference(parent_id, surface_id2);

  // The real reference should be added for |surface_id2| and the temporary
  // references to both |surface_id1| and |surface_id2| should be gone.
  // There should be a temporary reference to |parent_id|.
  EXPECT_THAT(GetAllTempReferences(), ElementsAre(parent_id));
  EXPECT_THAT(GetReferencesFromRoot(), ElementsAre(parent_id));
  EXPECT_THAT(GetReferencesFrom(parent_id), ElementsAre(surface_id2));
}

TEST_F(SurfaceManagerRefTest, RemoveFirstTempRefOnly) {
  // Add two surfaces that have the same FrameSinkId. This would happen when a
  // client submits two CFs before parent submits a new CF.
  const SurfaceId surface_id1 = CreateSurface(kFrameSink2, 1);
  const SurfaceId surface_id2 = CreateSurface(kFrameSink2, 2);

  // Temporary references should be added for both surfaces and they should be
  // stored in the order of creation.
  EXPECT_THAT(
      GetTempReferencesFor(surface_id1.frame_sink_id()),
      ElementsAre(surface_id1.local_frame_id(), surface_id2.local_frame_id()));
  EXPECT_THAT(GetReferencesFromRoot(),
              UnorderedElementsAre(surface_id1, surface_id2));

  // Create |parent_id| and add a reference from it to |surface_id1| which was
  // created earlier.
  const SurfaceId parent_id = CreateSurface(kFrameSink1, 1);
  manager().AddSurfaceReference(parent_id, surface_id1);

  // The real reference should be added for |surface_id1| and its temporary
  // reference should be removed. The temporary reference for |surface_id2|
  // should remain. A temporary reference must be added for |parent_id|.
  EXPECT_THAT(GetAllTempReferences(),
              UnorderedElementsAre(parent_id, surface_id2));
  EXPECT_THAT(GetReferencesFromRoot(),
              UnorderedElementsAre(parent_id, surface_id2));
  EXPECT_THAT(GetReferencesFrom(parent_id), ElementsAre(surface_id1));
}

}  // namespace cc
