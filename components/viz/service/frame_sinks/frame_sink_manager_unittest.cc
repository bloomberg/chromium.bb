// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

#include <stddef.h>

#include <tuple>

#include "base/run_loop.h"
#include "components/viz/common/constants.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/mock_display_client.h"
#include "components/viz/test/test_display_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr FrameSinkId kFrameSinkIdRoot(1, 1);
constexpr FrameSinkId kFrameSinkIdA(2, 1);
constexpr FrameSinkId kFrameSinkIdB(3, 1);
constexpr FrameSinkId kFrameSinkIdC(4, 1);

// Holds the four interface objects needed to create a RootCompositorFrameSink.
struct RootCompositorFrameSinkData {
  mojom::RootCompositorFrameSinkParamsPtr BuildParams(
      const FrameSinkId& frame_sink_id) {
    auto params = mojom::RootCompositorFrameSinkParams::New();
    params->frame_sink_id = frame_sink_id;
    params->widget = gpu::kNullSurfaceHandle;
    params->compositor_frame_sink = MakeRequest(&compositor_frame_sink);
    params->compositor_frame_sink_client =
        compositor_frame_sink_client.BindInterfacePtr().PassInterface();
    params->display_private = MakeRequest(&display_private);
    params->display_client = display_client.BindInterfacePtr().PassInterface();
    return params;
  }

  mojom::CompositorFrameSinkAssociatedPtr compositor_frame_sink;
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojom::DisplayPrivateAssociatedPtr display_private;
  MockDisplayClient display_client;
};

}  // namespace

class FrameSinkManagerTest : public testing::Test {
 public:
  FrameSinkManagerTest()
      : manager_(&shared_bitmap_manager_,
                 kDefaultActivationDeadlineInFrames,
                 &display_provider_) {}
  ~FrameSinkManagerTest() override = default;

  std::unique_ptr<CompositorFrameSinkSupport> CreateCompositorFrameSinkSupport(
      const FrameSinkId& frame_sink_id) {
    return std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, frame_sink_id, false, false);
  }

  const BeginFrameSource* GetBeginFrameSource(
      const std::unique_ptr<CompositorFrameSinkSupport>& support) {
    return support->begin_frame_source_;
  }

  // Checks if a [Root]CompositorFrameSinkImpl exists for |frame_sink_id|.
  bool CompositorFrameSinkExists(const FrameSinkId& frame_sink_id) {
    return base::ContainsKey(manager_.sink_map_, frame_sink_id);
  }

  // testing::Test implementation.
  void TearDown() override {
    // Make sure that all FrameSinkSourceMappings have been deleted.
    EXPECT_TRUE(manager_.frame_sink_source_map_.empty());

    // Make sure test cleans up all [Root]CompositorFrameSinkImpls.
    EXPECT_TRUE(manager_.support_map_.empty());

    // Make sure test has invalidated all registered FrameSinkIds.
    EXPECT_TRUE(manager_.frame_sink_data_.empty());
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  TestDisplayProvider display_provider_;
  FrameSinkManagerImpl manager_;
};

TEST_F(FrameSinkManagerTest, CreateRootCompositorFrameSink) {
  manager_.RegisterFrameSinkId(kFrameSinkIdRoot);

  // Create a RootCompositorFrameSinkImpl.
  RootCompositorFrameSinkData root_data;
  manager_.CreateRootCompositorFrameSink(
      root_data.BuildParams(kFrameSinkIdRoot));
  EXPECT_TRUE(CompositorFrameSinkExists(kFrameSinkIdRoot));

  // Invalidating should destroy the RootCompositorFrameSinkImpl.
  manager_.InvalidateFrameSinkId(kFrameSinkIdRoot);
  EXPECT_FALSE(CompositorFrameSinkExists(kFrameSinkIdRoot));
}

TEST_F(FrameSinkManagerTest, CreateCompositorFrameSink) {
  manager_.RegisterFrameSinkId(kFrameSinkIdA);

  // Create a CompositorFrameSinkImpl.
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojom::CompositorFrameSinkPtr compositor_frame_sink;
  manager_.CreateCompositorFrameSink(
      kFrameSinkIdA, MakeRequest(&compositor_frame_sink),
      compositor_frame_sink_client.BindInterfacePtr());
  EXPECT_TRUE(CompositorFrameSinkExists(kFrameSinkIdA));

  // Invalidating should destroy the CompositorFrameSinkImpl.
  manager_.InvalidateFrameSinkId(kFrameSinkIdA);
  EXPECT_FALSE(CompositorFrameSinkExists(kFrameSinkIdA));
}

TEST_F(FrameSinkManagerTest, CompositorFrameSinkConnectionLost) {
  manager_.RegisterFrameSinkId(kFrameSinkIdA);

  // Create a CompositorFrameSinkImpl.
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojom::CompositorFrameSinkPtr compositor_frame_sink;
  manager_.CreateCompositorFrameSink(
      kFrameSinkIdA, MakeRequest(&compositor_frame_sink),
      compositor_frame_sink_client.BindInterfacePtr());
  EXPECT_TRUE(CompositorFrameSinkExists(kFrameSinkIdA));

  // Close the connection from the renderer.
  compositor_frame_sink.reset();

  // Closing the connection will destroy the CompositorFrameSinkImpl along with
  // the mojom::CompositorFrameSinkClient binding.
  base::RunLoop run_loop;
  compositor_frame_sink_client.set_connection_error_handler(
      run_loop.QuitClosure());
  run_loop.Run();

  // Check that the CompositorFrameSinkImpl was destroyed.
  EXPECT_FALSE(CompositorFrameSinkExists(kFrameSinkIdA));

  manager_.InvalidateFrameSinkId(kFrameSinkIdA);
}

TEST_F(FrameSinkManagerTest, SingleClients) {
  auto client = CreateCompositorFrameSinkSupport(FrameSinkId(1, 1));
  auto other_client = CreateCompositorFrameSinkSupport(FrameSinkId(2, 2));
  StubBeginFrameSource source;

  EXPECT_EQ(nullptr, GetBeginFrameSource(client));
  EXPECT_EQ(nullptr, GetBeginFrameSource(other_client));

  // Test setting unsetting BFS
  manager_.RegisterBeginFrameSource(&source, client->frame_sink_id());
  EXPECT_EQ(&source, GetBeginFrameSource(client));
  EXPECT_EQ(nullptr, GetBeginFrameSource(other_client));
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(client));
  EXPECT_EQ(nullptr, GetBeginFrameSource(other_client));

  // Set BFS for other namespace
  manager_.RegisterBeginFrameSource(&source, other_client->frame_sink_id());
  EXPECT_EQ(&source, GetBeginFrameSource(other_client));
  EXPECT_EQ(nullptr, GetBeginFrameSource(client));
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(client));
  EXPECT_EQ(nullptr, GetBeginFrameSource(other_client));

  // Re-set BFS for original
  manager_.RegisterBeginFrameSource(&source, client->frame_sink_id());
  EXPECT_EQ(&source, GetBeginFrameSource(client));
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(client));
}

// This test verifies that a client is still connected to the BeginFrameSource
// after restart.
TEST_F(FrameSinkManagerTest, ClientRestart) {
  auto client = CreateCompositorFrameSinkSupport(kFrameSinkIdRoot);
  StubBeginFrameSource source;

  manager_.RegisterBeginFrameSource(&source, kFrameSinkIdRoot);
  EXPECT_EQ(&source, GetBeginFrameSource(client));

  client.reset();

  // |client| is reconnected with |source| after being recreated..
  client = CreateCompositorFrameSinkSupport(kFrameSinkIdRoot);
  EXPECT_EQ(&source, GetBeginFrameSource(client));

  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(client));
}

// This test verifies that a PrimaryBeginFrameSource will receive BeginFrames
// from the first BeginFrameSource registered. If that BeginFrameSource goes
// away then it will receive BeginFrames from the second BeginFrameSource.
TEST_F(FrameSinkManagerTest, PrimaryBeginFrameSource) {
  // This PrimaryBeginFrameSource should track the first BeginFrameSource
  // registered with the SurfaceManager.
  testing::NiceMock<MockBeginFrameObserver> obs;
  BeginFrameSource* begin_frame_source = manager_.GetPrimaryBeginFrameSource();
  begin_frame_source->AddObserver(&obs);

  auto root1 = CreateCompositorFrameSinkSupport(FrameSinkId(1, 1));
  std::unique_ptr<FakeExternalBeginFrameSource> external_source1 =
      std::make_unique<FakeExternalBeginFrameSource>(60.f, false);
  manager_.RegisterBeginFrameSource(external_source1.get(),
                                    root1->frame_sink_id());

  auto root2 = CreateCompositorFrameSinkSupport(FrameSinkId(2, 2));
  std::unique_ptr<FakeExternalBeginFrameSource> external_source2 =
      std::make_unique<FakeExternalBeginFrameSource>(60.f, false);
  manager_.RegisterBeginFrameSource(external_source2.get(),
                                    root2->frame_sink_id());

  // Ticking |external_source2| does not propagate to |begin_frame_source|.
  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, external_source2->source_id(), 1);
    EXPECT_CALL(obs, OnBeginFrame(testing::_)).Times(0);
    external_source2->TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&obs);
  }

  // Ticking |external_source1| does propagate to |begin_frame_source| and
  // |obs|.
  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, external_source1->source_id(), 1);
    EXPECT_CALL(obs, OnBeginFrame(args)).Times(1);
    external_source1->TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&obs);
  }

  // Getting rid of |external_source1| means those BeginFrames will not
  // propagate. Instead, |external_source2|'s BeginFrames will propagate
  // to |begin_frame_source|.
  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, external_source1->source_id(), 2);
    manager_.UnregisterBeginFrameSource(external_source1.get());
    EXPECT_CALL(obs, OnBeginFrame(testing::_)).Times(0);
    external_source1->TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&obs);
  }

  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, external_source2->source_id(), 2);
    EXPECT_CALL(obs, OnBeginFrame(testing::_)).Times(1);
    external_source2->TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&obs);
  }

  // Tear down
  manager_.UnregisterBeginFrameSource(external_source2.get());
  begin_frame_source->RemoveObserver(&obs);
}

TEST_F(FrameSinkManagerTest, MultipleDisplays) {
  StubBeginFrameSource root1_source;
  StubBeginFrameSource root2_source;

  // root1 -> A -> B
  // root2 -> C
  auto root1 = CreateCompositorFrameSinkSupport(FrameSinkId(1, 1));
  auto root2 = CreateCompositorFrameSinkSupport(FrameSinkId(2, 2));
  auto client_a = CreateCompositorFrameSinkSupport(FrameSinkId(3, 3));
  auto client_b = CreateCompositorFrameSinkSupport(FrameSinkId(4, 4));
  auto client_c = CreateCompositorFrameSinkSupport(FrameSinkId(5, 5));

  manager_.RegisterBeginFrameSource(&root1_source, root1->frame_sink_id());
  manager_.RegisterBeginFrameSource(&root2_source, root2->frame_sink_id());
  EXPECT_EQ(GetBeginFrameSource(root1), &root1_source);
  EXPECT_EQ(GetBeginFrameSource(root2), &root2_source);

  // Set up initial hierarchy.
  manager_.RegisterFrameSinkHierarchy(root1->frame_sink_id(),
                                      client_a->frame_sink_id());
  EXPECT_EQ(GetBeginFrameSource(client_a), GetBeginFrameSource(root1));
  manager_.RegisterFrameSinkHierarchy(client_a->frame_sink_id(),
                                      client_b->frame_sink_id());
  EXPECT_EQ(GetBeginFrameSource(client_b), GetBeginFrameSource(root1));
  manager_.RegisterFrameSinkHierarchy(root2->frame_sink_id(),
                                      client_c->frame_sink_id());
  EXPECT_EQ(GetBeginFrameSource(client_c), GetBeginFrameSource(root2));

  // Attach A into root2's subtree, like a window moving across displays.
  // root1 -> A -> B
  // root2 -> C -> A -> B
  manager_.RegisterFrameSinkHierarchy(client_c->frame_sink_id(),
                                      client_a->frame_sink_id());
  // With the heuristic of just keeping existing BFS in the face of multiple,
  // no client sources should change.
  EXPECT_EQ(GetBeginFrameSource(client_a), GetBeginFrameSource(root1));
  EXPECT_EQ(GetBeginFrameSource(client_b), GetBeginFrameSource(root1));
  EXPECT_EQ(GetBeginFrameSource(client_c), GetBeginFrameSource(root2));

  // Detach A from root1->  A and B should now be updated to root2->
  manager_.UnregisterFrameSinkHierarchy(root1->frame_sink_id(),
                                        client_a->frame_sink_id());
  EXPECT_EQ(GetBeginFrameSource(client_a), GetBeginFrameSource(root2));
  EXPECT_EQ(GetBeginFrameSource(client_b), GetBeginFrameSource(root2));
  EXPECT_EQ(GetBeginFrameSource(client_c), GetBeginFrameSource(root2));

  // Detach root1 from BFS.  root1 should now have no source.
  manager_.UnregisterBeginFrameSource(&root1_source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(root1));
  EXPECT_NE(nullptr, GetBeginFrameSource(root2));

  // Detatch root2 from BFS.
  manager_.UnregisterBeginFrameSource(&root2_source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(client_a));
  EXPECT_EQ(nullptr, GetBeginFrameSource(client_b));
  EXPECT_EQ(nullptr, GetBeginFrameSource(client_c));
  EXPECT_EQ(nullptr, GetBeginFrameSource(root2));

  // Cleanup hierarchy.
  manager_.UnregisterFrameSinkHierarchy(root2->frame_sink_id(),
                                        client_c->frame_sink_id());
  manager_.UnregisterFrameSinkHierarchy(client_c->frame_sink_id(),
                                        client_a->frame_sink_id());
  manager_.UnregisterFrameSinkHierarchy(client_a->frame_sink_id(),
                                        client_b->frame_sink_id());
}

// This test verifies that a BeginFrameSource path to the root from a
// FrameSinkId is preserved even if that FrameSinkId has no children
// and does not have a corresponding CompositorFrameSinkSupport.
TEST_F(FrameSinkManagerTest, ParentWithoutClientRetained) {
  StubBeginFrameSource root_source;

  auto root = CreateCompositorFrameSinkSupport(kFrameSinkIdRoot);
  auto client_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);
  auto client_c = CreateCompositorFrameSinkSupport(kFrameSinkIdC);

  manager_.RegisterBeginFrameSource(&root_source, root->frame_sink_id());
  EXPECT_EQ(&root_source, GetBeginFrameSource(root));

  // Set up initial hierarchy: root -> A -> B.
  // Note that A does not have a CompositorFrameSinkSupport.
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdRoot, kFrameSinkIdA);
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  // The root's BeginFrameSource should propagate to B.
  EXPECT_EQ(GetBeginFrameSource(root), GetBeginFrameSource(client_b));

  // Unregister B, and attach C to A: root -> A -> C
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  EXPECT_EQ(nullptr, GetBeginFrameSource(client_b));
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdC);
  // The root's BeginFrameSource should propagate to C.
  EXPECT_EQ(GetBeginFrameSource(root), GetBeginFrameSource(client_c));

  manager_.UnregisterBeginFrameSource(&root_source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(root));
  EXPECT_EQ(nullptr, GetBeginFrameSource(client_c));

  // Unregister all registered hierarchy.
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdRoot, kFrameSinkIdA);
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdC);
}

// This test sets up the same hierarchy as ParentWithoutClientRetained.
// However, this unit test registers the BeginFrameSource AFTER C
// has been attached to A. This test verifies that the BeginFrameSource
// propagates all the way to C.
TEST_F(FrameSinkManagerTest,
       ParentWithoutClientRetained_LateBeginFrameRegistration) {
  StubBeginFrameSource root_source;

  auto root = CreateCompositorFrameSinkSupport(kFrameSinkIdRoot);
  auto client_b = CreateCompositorFrameSinkSupport(kFrameSinkIdB);
  auto client_c = CreateCompositorFrameSinkSupport(kFrameSinkIdC);

  // Set up initial hierarchy: root -> A -> B.
  // Note that A does not have a CompositorFrameSinkSupport.
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdRoot, kFrameSinkIdA);
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  // The root does not yet have a BeginFrameSource so client B should not have
  // one either.
  EXPECT_EQ(nullptr, GetBeginFrameSource(client_b));

  // Unregister B, and attach C to A: root -> A -> C
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdC);

  // Registering a BeginFrameSource at the root should propagate it to C.
  manager_.RegisterBeginFrameSource(&root_source, root->frame_sink_id());
  // The root's BeginFrameSource should propagate to C.
  EXPECT_EQ(&root_source, GetBeginFrameSource(root));
  EXPECT_EQ(GetBeginFrameSource(root), GetBeginFrameSource(client_c));

  manager_.UnregisterBeginFrameSource(&root_source);
  EXPECT_EQ(nullptr, GetBeginFrameSource(root));
  EXPECT_EQ(nullptr, GetBeginFrameSource(client_c));

  // Unregister all registered hierarchy.
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdRoot, kFrameSinkIdA);
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdC);
}

// Verifies that the SurfaceIds passed to EvictSurfaces will be destroyed in the
// next garbage collection.
TEST_F(FrameSinkManagerTest, EvictSurfaces) {
  ParentLocalSurfaceIdAllocator allocator;
  LocalSurfaceId local_surface_id1 = allocator.GenerateId();
  LocalSurfaceId local_surface_id2 = allocator.GenerateId();
  SurfaceId surface_id1(kFrameSinkIdA, local_surface_id1);
  SurfaceId surface_id2(kFrameSinkIdB, local_surface_id2);

  // Create two frame sinks. Each create a surface.
  auto sink1 = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
  auto sink2 = CreateCompositorFrameSinkSupport(kFrameSinkIdB);
  sink1->SubmitCompositorFrame(local_surface_id1, MakeDefaultCompositorFrame());
  sink2->SubmitCompositorFrame(local_surface_id2, MakeDefaultCompositorFrame());

  // |surface_id1| and |surface_id2| should remain alive after garbage
  // collection because they're not marked for destruction.
  manager_.surface_manager()->GarbageCollectSurfaces();
  EXPECT_TRUE(manager_.surface_manager()->GetSurfaceForId(surface_id1));
  EXPECT_TRUE(manager_.surface_manager()->GetSurfaceForId(surface_id2));

  // Call EvictSurfaces. Now the garbage collector can destroy the surfaces.
  manager_.EvictSurfaces({surface_id1, surface_id2});
  manager_.surface_manager()->GarbageCollectSurfaces();
  EXPECT_FALSE(manager_.surface_manager()->GetSurfaceForId(surface_id1));
  EXPECT_FALSE(manager_.surface_manager()->GetSurfaceForId(surface_id2));
}

// Verify that setting debug label works and that debug labels are cleared when
// FrameSinkId is invalidated.
TEST_F(FrameSinkManagerTest, DebugLabel) {
  const std::string label = "Test Label";

  manager_.RegisterFrameSinkId(kFrameSinkIdA);
  manager_.SetFrameSinkDebugLabel(kFrameSinkIdA, label);
  EXPECT_EQ(label, manager_.GetFrameSinkDebugLabel(kFrameSinkIdA));

  manager_.InvalidateFrameSinkId(kFrameSinkIdA);
  EXPECT_EQ("", manager_.GetFrameSinkDebugLabel(kFrameSinkIdA));
}

namespace {

enum RegisterOrder { REGISTER_HIERARCHY_FIRST, REGISTER_CLIENTS_FIRST };
enum UnregisterOrder { UNREGISTER_HIERARCHY_FIRST, UNREGISTER_CLIENTS_FIRST };
enum BFSOrder { BFS_FIRST, BFS_SECOND, BFS_THIRD };

static const RegisterOrder kRegisterOrderList[] = {REGISTER_HIERARCHY_FIRST,
                                                   REGISTER_CLIENTS_FIRST};
static const UnregisterOrder kUnregisterOrderList[] = {
    UNREGISTER_HIERARCHY_FIRST, UNREGISTER_CLIENTS_FIRST};
static const BFSOrder kBFSOrderList[] = {BFS_FIRST, BFS_SECOND, BFS_THIRD};

}  // namespace

// In practice, registering and unregistering both parent/child relationships
// and CompositorFrameSinkSupports can happen in any ordering with respect to
// each other.  These following tests verify that all the data structures
// are properly set up and cleaned up under the four permutations of orderings
// of this nesting.
class FrameSinkManagerOrderingTest : public FrameSinkManagerTest {
 public:
  FrameSinkManagerOrderingTest()
      : hierarchy_registered_(false),
        clients_registered_(false),
        bfs_registered_(false) {
    AssertCorrectBFSState();
  }

  ~FrameSinkManagerOrderingTest() override {
    EXPECT_FALSE(hierarchy_registered_);
    EXPECT_FALSE(clients_registered_);
    EXPECT_FALSE(bfs_registered_);
    AssertCorrectBFSState();
  }

  void RegisterHierarchy() {
    DCHECK(!hierarchy_registered_);
    hierarchy_registered_ = true;
    manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
    manager_.RegisterFrameSinkHierarchy(kFrameSinkIdB, kFrameSinkIdC);
    AssertCorrectBFSState();
  }
  void UnregisterHierarchy() {
    DCHECK(hierarchy_registered_);
    hierarchy_registered_ = false;
    manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
    manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdB, kFrameSinkIdC);
    AssertCorrectBFSState();
  }

  void RegisterClients() {
    DCHECK(!clients_registered_);
    clients_registered_ = true;
    client_a_ = CreateCompositorFrameSinkSupport(kFrameSinkIdA);
    client_b_ = CreateCompositorFrameSinkSupport(kFrameSinkIdB);
    client_c_ = CreateCompositorFrameSinkSupport(kFrameSinkIdC);
    AssertCorrectBFSState();
  }

  void UnregisterClients() {
    DCHECK(clients_registered_);
    clients_registered_ = false;
    client_a_.reset();
    client_b_.reset();
    client_c_.reset();
    AssertCorrectBFSState();
  }

  void RegisterBFS() {
    DCHECK(!bfs_registered_);
    bfs_registered_ = true;
    manager_.RegisterBeginFrameSource(&source_, kFrameSinkIdA);
    AssertCorrectBFSState();
  }
  void UnregisterBFS() {
    DCHECK(bfs_registered_);
    bfs_registered_ = false;
    manager_.UnregisterBeginFrameSource(&source_);
    AssertCorrectBFSState();
  }

  void AssertEmptyBFS() {
    EXPECT_EQ(nullptr, GetBeginFrameSource(client_a_));
    EXPECT_EQ(nullptr, GetBeginFrameSource(client_b_));
    EXPECT_EQ(nullptr, GetBeginFrameSource(client_c_));
  }

  void AssertAllValidBFS() {
    EXPECT_EQ(&source_, GetBeginFrameSource(client_a_));
    EXPECT_EQ(&source_, GetBeginFrameSource(client_b_));
    EXPECT_EQ(&source_, GetBeginFrameSource(client_c_));
  }

 protected:
  void AssertCorrectBFSState() {
    if (!clients_registered_)
      return;

    if (!bfs_registered_) {
      AssertEmptyBFS();
      return;
    }

    if (!hierarchy_registered_) {
      // A valid but not attached to anything.
      EXPECT_EQ(&source_, GetBeginFrameSource(client_a_));
      EXPECT_EQ(nullptr, GetBeginFrameSource(client_b_));
      EXPECT_EQ(nullptr, GetBeginFrameSource(client_c_));
      return;
    }

    AssertAllValidBFS();
  }

  StubBeginFrameSource source_;
  // A -> B -> C hierarchy, with A always having the BFS.
  std::unique_ptr<CompositorFrameSinkSupport> client_a_;
  std::unique_ptr<CompositorFrameSinkSupport> client_b_;
  std::unique_ptr<CompositorFrameSinkSupport> client_c_;

  bool hierarchy_registered_;
  bool clients_registered_;
  bool bfs_registered_;
};

class FrameSinkManagerOrderingParamTest
    : public FrameSinkManagerOrderingTest,
      public ::testing::WithParamInterface<
          std::tuple<RegisterOrder, UnregisterOrder, BFSOrder>> {};

TEST_P(FrameSinkManagerOrderingParamTest, Ordering) {
  // Test the four permutations of client/hierarchy setting/unsetting and test
  // each place the BFS can be added and removed.  The BFS and the
  // client/hierarchy are less related, so BFS is tested independently instead
  // of every permutation of BFS setting and unsetting.
  // The register/unregister functions themselves test most of the state.
  RegisterOrder register_order = std::get<0>(GetParam());
  UnregisterOrder unregister_order = std::get<1>(GetParam());
  BFSOrder bfs_order = std::get<2>(GetParam());

  // Attach everything up in the specified order.
  if (bfs_order == BFS_FIRST)
    RegisterBFS();

  if (register_order == REGISTER_HIERARCHY_FIRST)
    RegisterHierarchy();
  else
    RegisterClients();

  if (bfs_order == BFS_SECOND)
    RegisterBFS();

  if (register_order == REGISTER_HIERARCHY_FIRST)
    RegisterClients();
  else
    RegisterHierarchy();

  if (bfs_order == BFS_THIRD)
    RegisterBFS();

  // Everything hooked up, so should be valid.
  AssertAllValidBFS();

  // Detach everything in the specified order.
  if (bfs_order == BFS_THIRD)
    UnregisterBFS();

  if (unregister_order == UNREGISTER_HIERARCHY_FIRST)
    UnregisterHierarchy();
  else
    UnregisterClients();

  if (bfs_order == BFS_SECOND)
    UnregisterBFS();

  if (unregister_order == UNREGISTER_HIERARCHY_FIRST)
    UnregisterClients();
  else
    UnregisterHierarchy();

  if (bfs_order == BFS_FIRST)
    UnregisterBFS();
}

INSTANTIATE_TEST_CASE_P(
    FrameSinkManagerOrderingParamTestInstantiation,
    FrameSinkManagerOrderingParamTest,
    ::testing::Combine(::testing::ValuesIn(kRegisterOrderList),
                       ::testing::ValuesIn(kUnregisterOrderList),
                       ::testing::ValuesIn(kBFSOrderList)));

}  // namespace viz
