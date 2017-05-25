// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/frame_sink_manager_client.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeFrameSinkManagerClient : public FrameSinkManagerClient {
 public:
  explicit FakeFrameSinkManagerClient(const FrameSinkId& frame_sink_id)
      : source_(nullptr), manager_(nullptr), frame_sink_id_(frame_sink_id) {}

  FakeFrameSinkManagerClient(const FrameSinkId& frame_sink_id,
                             SurfaceManager* manager)
      : source_(nullptr), manager_(nullptr), frame_sink_id_(frame_sink_id) {
    DCHECK(manager);
    Register(manager);
  }

  ~FakeFrameSinkManagerClient() override {
    if (manager_) {
      Unregister();
    }
    EXPECT_EQ(nullptr, source_);
  }

  BeginFrameSource* source() { return source_; }
  const FrameSinkId& frame_sink_id() { return frame_sink_id_; }

  void Register(SurfaceManager* manager) {
    EXPECT_EQ(nullptr, manager_);
    manager_ = manager;
    manager_->RegisterFrameSinkManagerClient(frame_sink_id_, this);
  }

  void Unregister() {
    EXPECT_NE(manager_, nullptr);
    manager_->UnregisterFrameSinkManagerClient(frame_sink_id_);
    manager_ = nullptr;
  }

  // FrameSinkManagerClient implementation.
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override {
    DCHECK(!source_ || !begin_frame_source);
    source_ = begin_frame_source;
  }

 private:
  BeginFrameSource* source_;
  SurfaceManager* manager_;
  FrameSinkId frame_sink_id_;
};

class SurfaceManagerTest : public testing::Test {
 public:
  // These tests don't care about namespace registration, so just preregister
  // a set of namespaces that tests can use freely without worrying if they're
  // valid or not.
  enum { MAX_FRAME_SINK = 10 };

  SurfaceManagerTest() {
    for (size_t i = 0; i < MAX_FRAME_SINK; ++i)
      manager_.RegisterFrameSinkId(FrameSinkId(i, i));
  }

  ~SurfaceManagerTest() override {
    for (size_t i = 0; i < MAX_FRAME_SINK; ++i)
      manager_.InvalidateFrameSinkId(FrameSinkId(i, i));
  }

 protected:
  SurfaceManager manager_;
};

TEST_F(SurfaceManagerTest, SingleClients) {
  FakeFrameSinkManagerClient client(FrameSinkId(1, 1));
  FakeFrameSinkManagerClient other_client(FrameSinkId(2, 2));
  StubBeginFrameSource source;

  EXPECT_EQ(nullptr, client.source());
  EXPECT_EQ(nullptr, other_client.source());
  client.Register(&manager_);
  other_client.Register(&manager_);
  EXPECT_EQ(nullptr, client.source());
  EXPECT_EQ(nullptr, other_client.source());

  // Test setting unsetting BFS
  manager_.RegisterBeginFrameSource(&source, client.frame_sink_id());
  EXPECT_EQ(&source, client.source());
  EXPECT_EQ(nullptr, other_client.source());
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(nullptr, client.source());
  EXPECT_EQ(nullptr, other_client.source());

  // Set BFS for other namespace
  manager_.RegisterBeginFrameSource(&source, other_client.frame_sink_id());
  EXPECT_EQ(&source, other_client.source());
  EXPECT_EQ(nullptr, client.source());
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(nullptr, client.source());
  EXPECT_EQ(nullptr, other_client.source());

  // Re-set BFS for original
  manager_.RegisterBeginFrameSource(&source, client.frame_sink_id());
  EXPECT_EQ(&source, client.source());
  manager_.UnregisterBeginFrameSource(&source);
  EXPECT_EQ(nullptr, client.source());
}

TEST_F(SurfaceManagerTest, MultipleDisplays) {
  StubBeginFrameSource root1_source;
  StubBeginFrameSource root2_source;

  // root1 -> A -> B
  // root2 -> C
  FakeFrameSinkManagerClient root1(FrameSinkId(1, 1), &manager_);
  FakeFrameSinkManagerClient root2(FrameSinkId(2, 2), &manager_);
  FakeFrameSinkManagerClient client_a(FrameSinkId(3, 3), &manager_);
  FakeFrameSinkManagerClient client_b(FrameSinkId(4, 4), &manager_);
  FakeFrameSinkManagerClient client_c(FrameSinkId(5, 5), &manager_);

  manager_.RegisterBeginFrameSource(&root1_source, root1.frame_sink_id());
  manager_.RegisterBeginFrameSource(&root2_source, root2.frame_sink_id());
  EXPECT_EQ(root1.source(), &root1_source);
  EXPECT_EQ(root2.source(), &root2_source);

  // Set up initial hierarchy.
  manager_.RegisterFrameSinkHierarchy(root1.frame_sink_id(),
                                      client_a.frame_sink_id());
  EXPECT_EQ(client_a.source(), root1.source());
  manager_.RegisterFrameSinkHierarchy(client_a.frame_sink_id(),
                                      client_b.frame_sink_id());
  EXPECT_EQ(client_b.source(), root1.source());
  manager_.RegisterFrameSinkHierarchy(root2.frame_sink_id(),
                                      client_c.frame_sink_id());
  EXPECT_EQ(client_c.source(), root2.source());

  // Attach A into root2's subtree, like a window moving across displays.
  // root1 -> A -> B
  // root2 -> C -> A -> B
  manager_.RegisterFrameSinkHierarchy(client_c.frame_sink_id(),
                                      client_a.frame_sink_id());
  // With the heuristic of just keeping existing BFS in the face of multiple,
  // no client sources should change.
  EXPECT_EQ(client_a.source(), root1.source());
  EXPECT_EQ(client_b.source(), root1.source());
  EXPECT_EQ(client_c.source(), root2.source());

  // Detach A from root1.  A and B should now be updated to root2.
  manager_.UnregisterFrameSinkHierarchy(root1.frame_sink_id(),
                                        client_a.frame_sink_id());
  EXPECT_EQ(client_a.source(), root2.source());
  EXPECT_EQ(client_b.source(), root2.source());
  EXPECT_EQ(client_c.source(), root2.source());

  // Detach root1 from BFS.  root1 should now have no source.
  manager_.UnregisterBeginFrameSource(&root1_source);
  EXPECT_EQ(nullptr, root1.source());
  EXPECT_NE(nullptr, root2.source());

  // Detatch root2 from BFS.
  manager_.UnregisterBeginFrameSource(&root2_source);
  EXPECT_EQ(nullptr, client_a.source());
  EXPECT_EQ(nullptr, client_b.source());
  EXPECT_EQ(nullptr, client_c.source());
  EXPECT_EQ(nullptr, root2.source());

  // Cleanup hierarchy.
  manager_.UnregisterFrameSinkHierarchy(root2.frame_sink_id(),
                                        client_c.frame_sink_id());
  manager_.UnregisterFrameSinkHierarchy(client_c.frame_sink_id(),
                                        client_a.frame_sink_id());
  manager_.UnregisterFrameSinkHierarchy(client_a.frame_sink_id(),
                                        client_b.frame_sink_id());
}

// This test verifies that a BeginFrameSource path to the root from a
// FrameSinkId is preserved even if that FrameSinkId has no children
// and does not have a corresponding FrameSinkManagerClient.
TEST_F(SurfaceManagerTest, ParentWithoutClientRetained) {
  StubBeginFrameSource root_source;

  constexpr FrameSinkId kFrameSinkIdRoot(1, 1);
  constexpr FrameSinkId kFrameSinkIdA(2, 2);
  constexpr FrameSinkId kFrameSinkIdB(3, 3);
  constexpr FrameSinkId kFrameSinkIdC(4, 4);

  FakeFrameSinkManagerClient root(kFrameSinkIdRoot, &manager_);
  FakeFrameSinkManagerClient client_b(kFrameSinkIdB, &manager_);
  FakeFrameSinkManagerClient client_c(kFrameSinkIdC, &manager_);

  manager_.RegisterBeginFrameSource(&root_source, root.frame_sink_id());
  EXPECT_EQ(&root_source, root.source());

  // Set up initial hierarchy: root -> A -> B.
  // Note that A does not have a FrameSinkManagerClient.
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdRoot, kFrameSinkIdA);
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  // The root's BeginFrameSource should propagate to B.
  EXPECT_EQ(root.source(), client_b.source());

  // Unregister B, and attach C to A: root -> A -> C
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  EXPECT_EQ(nullptr, client_b.source());
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdC);
  // The root's BeginFrameSource should propagate to C.
  EXPECT_EQ(root.source(), client_c.source());

  manager_.UnregisterBeginFrameSource(&root_source);
  EXPECT_EQ(nullptr, root.source());
  EXPECT_EQ(nullptr, client_c.source());
}

// This test sets up the same hierarchy as ParentWithoutClientRetained.
// However, this unit test registers the BeginFrameSource AFTER C
// has been attached to A. This test verifies that the BeginFrameSource
// propagates all the way to C.
TEST_F(SurfaceManagerTest,
       ParentWithoutClientRetained_LateBeginFrameRegistration) {
  StubBeginFrameSource root_source;

  constexpr FrameSinkId kFrameSinkIdRoot(1, 1);
  constexpr FrameSinkId kFrameSinkIdA(2, 2);
  constexpr FrameSinkId kFrameSinkIdB(3, 3);
  constexpr FrameSinkId kFrameSinkIdC(4, 4);

  FakeFrameSinkManagerClient root(kFrameSinkIdRoot, &manager_);
  FakeFrameSinkManagerClient client_b(kFrameSinkIdB, &manager_);
  FakeFrameSinkManagerClient client_c(kFrameSinkIdC, &manager_);

  // Set up initial hierarchy: root -> A -> B.
  // Note that A does not have a FrameSinkManagerClient.
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdRoot, kFrameSinkIdA);
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  // The root does not yet have a BeginFrameSource so client B should not have
  // one either.
  EXPECT_EQ(nullptr, client_b.source());

  // Unregister B, and attach C to A: root -> A -> C
  manager_.UnregisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdB);
  manager_.RegisterFrameSinkHierarchy(kFrameSinkIdA, kFrameSinkIdC);

  // Registering a BeginFrameSource at the root should propagate it to C.
  manager_.RegisterBeginFrameSource(&root_source, root.frame_sink_id());
  // The root's BeginFrameSource should propagate to C.
  EXPECT_EQ(&root_source, root.source());
  EXPECT_EQ(root.source(), client_c.source());

  manager_.UnregisterBeginFrameSource(&root_source);
  EXPECT_EQ(nullptr, root.source());
  EXPECT_EQ(nullptr, client_c.source());
}

// In practice, registering and unregistering both parent/child relationships
// and FrameSinkManagerClients can happen in any ordering with respect to
// each other.  These following tests verify that all the data structures
// are properly set up and cleaned up under the four permutations of orderings
// of this nesting.

class SurfaceManagerOrderingTest : public SurfaceManagerTest {
 public:
  SurfaceManagerOrderingTest()
      : client_a_(FrameSinkId(1, 1)),
        client_b_(FrameSinkId(2, 2)),
        client_c_(FrameSinkId(3, 3)),
        hierarchy_registered_(false),
        clients_registered_(false),
        bfs_registered_(false) {
    AssertCorrectBFSState();
  }

  ~SurfaceManagerOrderingTest() override {
    EXPECT_FALSE(hierarchy_registered_);
    EXPECT_FALSE(clients_registered_);
    EXPECT_FALSE(bfs_registered_);
    AssertCorrectBFSState();
  }

  void RegisterHierarchy() {
    DCHECK(!hierarchy_registered_);
    hierarchy_registered_ = true;
    manager_.RegisterFrameSinkHierarchy(client_a_.frame_sink_id(),
                                        client_b_.frame_sink_id());
    manager_.RegisterFrameSinkHierarchy(client_b_.frame_sink_id(),
                                        client_c_.frame_sink_id());
    AssertCorrectBFSState();
  }
  void UnregisterHierarchy() {
    DCHECK(hierarchy_registered_);
    hierarchy_registered_ = false;
    manager_.UnregisterFrameSinkHierarchy(client_a_.frame_sink_id(),
                                          client_b_.frame_sink_id());
    manager_.UnregisterFrameSinkHierarchy(client_b_.frame_sink_id(),
                                          client_c_.frame_sink_id());
    AssertCorrectBFSState();
  }

  void RegisterClients() {
    DCHECK(!clients_registered_);
    clients_registered_ = true;
    client_a_.Register(&manager_);
    client_b_.Register(&manager_);
    client_c_.Register(&manager_);
    AssertCorrectBFSState();
  }

  void UnregisterClients() {
    DCHECK(clients_registered_);
    clients_registered_ = false;
    client_a_.Unregister();
    client_b_.Unregister();
    client_c_.Unregister();
    AssertCorrectBFSState();
  }

  void RegisterBFS() {
    DCHECK(!bfs_registered_);
    bfs_registered_ = true;
    manager_.RegisterBeginFrameSource(&source_, client_a_.frame_sink_id());
    AssertCorrectBFSState();
  }
  void UnregisterBFS() {
    DCHECK(bfs_registered_);
    bfs_registered_ = false;
    manager_.UnregisterBeginFrameSource(&source_);
    AssertCorrectBFSState();
  }

  void AssertEmptyBFS() {
    EXPECT_EQ(nullptr, client_a_.source());
    EXPECT_EQ(nullptr, client_b_.source());
    EXPECT_EQ(nullptr, client_c_.source());
  }

  void AssertAllValidBFS() {
    EXPECT_EQ(&source_, client_a_.source());
    EXPECT_EQ(&source_, client_b_.source());
    EXPECT_EQ(&source_, client_c_.source());
  }

 protected:
  void AssertCorrectBFSState() {
    if (!clients_registered_ || !bfs_registered_) {
      AssertEmptyBFS();
      return;
    }
    if (!hierarchy_registered_) {
      // A valid but not attached to anything.
      EXPECT_EQ(&source_, client_a_.source());
      EXPECT_EQ(nullptr, client_b_.source());
      EXPECT_EQ(nullptr, client_c_.source());
      return;
    }

    AssertAllValidBFS();
  }

  StubBeginFrameSource source_;
  // A -> B -> C hierarchy, with A always having the BFS.
  FakeFrameSinkManagerClient client_a_;
  FakeFrameSinkManagerClient client_b_;
  FakeFrameSinkManagerClient client_c_;

  bool hierarchy_registered_;
  bool clients_registered_;
  bool bfs_registered_;
};

enum RegisterOrder { REGISTER_HIERARCHY_FIRST, REGISTER_CLIENTS_FIRST };
enum UnregisterOrder { UNREGISTER_HIERARCHY_FIRST, UNREGISTER_CLIENTS_FIRST };
enum BFSOrder { BFS_FIRST, BFS_SECOND, BFS_THIRD };

static const RegisterOrder kRegisterOrderList[] = {REGISTER_HIERARCHY_FIRST,
                                                   REGISTER_CLIENTS_FIRST};
static const UnregisterOrder kUnregisterOrderList[] = {
    UNREGISTER_HIERARCHY_FIRST, UNREGISTER_CLIENTS_FIRST};
static const BFSOrder kBFSOrderList[] = {BFS_FIRST, BFS_SECOND, BFS_THIRD};

class SurfaceManagerOrderingParamTest
    : public SurfaceManagerOrderingTest,
      public ::testing::WithParamInterface<
          std::tr1::tuple<RegisterOrder, UnregisterOrder, BFSOrder>> {};

TEST_P(SurfaceManagerOrderingParamTest, Ordering) {
  // Test the four permutations of client/hierarchy setting/unsetting and test
  // each place the BFS can be added and removed.  The BFS and the
  // client/hierarchy are less related, so BFS is tested independently instead
  // of every permutation of BFS setting and unsetting.
  // The register/unregister functions themselves test most of the state.
  RegisterOrder register_order = std::tr1::get<0>(GetParam());
  UnregisterOrder unregister_order = std::tr1::get<1>(GetParam());
  BFSOrder bfs_order = std::tr1::get<2>(GetParam());

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
    SurfaceManagerOrderingParamTestInstantiation,
    SurfaceManagerOrderingParamTest,
    ::testing::Combine(::testing::ValuesIn(kRegisterOrderList),
                       ::testing::ValuesIn(kUnregisterOrderList),
                       ::testing::ValuesIn(kBFSOrderList)));

}  // namespace cc
