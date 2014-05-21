// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/shell/connect.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

// TODO(sky): remove this when Darin is done with cleanup.
template <typename T>
class MOJO_COMMON_EXPORT TypeConverter<T, T> {
 public:
  static T ConvertFrom(T input, Buffer* buf) {
    return input;
  }
  static T ConvertTo(T input) {
    return input;
  }

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

namespace view_manager {
namespace service {

namespace {

base::RunLoop* current_run_loop = NULL;

// Sets |current_run_loop| and runs it. It is expected that someone else quits
// the loop.
void DoRunLoop() {
  DCHECK(!current_run_loop);

  base::RunLoop run_loop;
  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = NULL;
}

// Converts |id| into a string.
std::string NodeIdToString(TransportNodeId id) {
  return (id == 0) ? "null" :
      base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

// Boolean callback. Sets |result_cache| to the value of |result| and quits
// the run loop.
void BooleanCallback(bool* result_cache, bool result) {
  *result_cache = result;
  current_run_loop->Quit();
}

struct TestNode {
  std::string ToString() const {
    return base::StringPrintf("node=%s parent=%s view=%s",
                              NodeIdToString(node_id).c_str(),
                              NodeIdToString(parent_id).c_str(),
                              NodeIdToString(view_id).c_str());
  }

  TransportNodeId parent_id;
  TransportNodeId node_id;
  TransportNodeId view_id;
};

void INodesToTestNodes(const mojo::Array<INode>& data,
                       std::vector<TestNode>* test_nodes) {
  for (size_t i = 0; i < data.size(); ++i) {
    TestNode node;
    node.parent_id = data[i].parent_id();
    node.node_id = data[i].node_id();
    node.view_id = data[i].view_id();
    test_nodes->push_back(node);
  }
}

// Callback that results in a vector of INodes. The INodes are converted to
// TestNodes.
void INodesCallback(std::vector<TestNode>* test_nodes,
                    const mojo::Array<INode>& data) {
  INodesToTestNodes(data, test_nodes);
  current_run_loop->Quit();
}

// Creates an id used for transport from the specified parameters.
TransportNodeId CreateNodeId(TransportConnectionId connection_id,
                             TransportConnectionSpecificNodeId node_id) {
  return (connection_id << 16) | node_id;
}

// Creates an id used for transport from the specified parameters.
TransportViewId CreateViewId(TransportConnectionId connection_id,
                             TransportConnectionSpecificViewId view_id) {
  return (connection_id << 16) | view_id;
}

// Creates a node with the specified id. Returns true on success. Blocks until
// we get back result from server.
bool CreateNode(IViewManager* view_manager,
                TransportConnectionId connection_id,
                TransportConnectionSpecificNodeId node_id) {
  bool result = false;
  view_manager->CreateNode(CreateNodeId(connection_id, node_id),
                           base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Deletes a view, blocking until done.
bool DeleteNode(IViewManager* view_manager, TransportNodeId node_id) {
  bool result = false;
  view_manager->DeleteNode(node_id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Deletes a node, blocking until done.
bool DeleteView(IViewManager* view_manager, TransportViewId view_id) {
  bool result = false;
  view_manager->DeleteView(view_id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Adds a node, blocking until done.
bool AddNode(IViewManager* view_manager,
             TransportNodeId parent,
             TransportNodeId child,
             TransportChangeId server_change_id) {
  bool result = false;
  view_manager->AddNode(parent, child, server_change_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Removes a node, blocking until done.
bool RemoveNodeFromParent(IViewManager* view_manager,
                          TransportNodeId node_id,
                          TransportChangeId server_change_id) {
  bool result = false;
  view_manager->RemoveNodeFromParent(
      node_id, server_change_id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

void GetNodeTree(IViewManager* view_manager,
                 TransportNodeId node_id,
                 std::vector<TestNode>* nodes) {
  view_manager->GetNodeTree(node_id, base::Bind(&INodesCallback, nodes));
  DoRunLoop();
}

// Creates a view with the specified id. Returns true on success. Blocks until
// we get back result from server.
bool CreateView(IViewManager* view_manager,
                TransportConnectionId connection_id,
                TransportConnectionSpecificViewId id) {
  bool result = false;
  view_manager->CreateView(CreateViewId(connection_id, id),
                           base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Sets a view on the specified node. Returns true on success. Blocks until we
// get back result from server.
bool SetView(IViewManager* view_manager,
             TransportNodeId node_id,
             TransportViewId view_id) {
  bool result = false;
  view_manager->SetView(node_id, view_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

bool SetRoots(IViewManager* view_manager,
              TransportConnectionId connection_id,
              const std::vector<uint32_t>& node_ids) {
  bool result = false;
  view_manager->SetRoots(connection_id,
                         Array<uint32_t>::From(node_ids),
                         base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

}  // namespace

typedef std::vector<std::string> Changes;

class ViewManagerClientImpl : public IViewManagerClient {
 public:
  ViewManagerClientImpl()
      : id_(0),
        next_server_change_id_(0),
        quit_count_(0) {}

  TransportConnectionId id() const { return id_; }

  TransportChangeId next_server_change_id() const {
    return next_server_change_id_;
  }
  const std::vector<TestNode>& initial_nodes() const {
    return initial_nodes_;
  }
  const std::vector<TestNode>& hierarchy_changed_nodes() const {
    return hierarchy_changed_nodes_;
  }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

  void ClearId() {
    id_ = 0;
  }

  void WaitForId() {
    DCHECK_EQ(0, id_);
    DoRunLoopUntilChangesCount(1);
  }

  void DoRunLoopUntilChangesCount(size_t count) {
    if (changes_.size() >= count)
      return;
    quit_count_ = count - changes_.size();
    DoRunLoop();
  }

 private:
  // IViewManagerClient overrides:
  virtual void OnConnectionEstablished(
      TransportConnectionId connection_id,
      TransportChangeId next_server_change_id,
      const mojo::Array<INode>& nodes) OVERRIDE {
    id_ = connection_id;
    next_server_change_id_ = next_server_change_id;
    initial_nodes_.clear();
    INodesToTestNodes(nodes, &initial_nodes_);
    changes_.push_back("OnConnectionEstablished");
    QuitIfNecessary();
  }
  virtual void OnServerChangeIdAdvanced(
      uint32_t next_server_change_id) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "ServerChangeIdAdvanced %d",
            static_cast<int>(next_server_change_id)));
    QuitIfNecessary();
  }
  virtual void OnNodeHierarchyChanged(
      TransportNodeId node,
      TransportNodeId new_parent,
      TransportNodeId old_parent,
      TransportChangeId server_change_id,
      const mojo::Array<INode>& nodes) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "HierarchyChanged change_id=%d node=%s new_parent=%s old_parent=%s",
            static_cast<int>(server_change_id),
            NodeIdToString(node).c_str(),
            NodeIdToString(new_parent).c_str(),
            NodeIdToString(old_parent).c_str()));
    hierarchy_changed_nodes_.clear();
    INodesToTestNodes(nodes, &hierarchy_changed_nodes_);
    QuitIfNecessary();
  }
  virtual void OnNodeDeleted(TransportNodeId node,
                             TransportChangeId server_change_id) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "NodeDeleted change_id=%d node=%s",
            static_cast<int>(server_change_id),
            NodeIdToString(node).c_str()));
    QuitIfNecessary();
  }
  virtual void OnViewDeleted(TransportViewId view) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "ViewDeleted view=%s",
            NodeIdToString(view).c_str()));
    QuitIfNecessary();
  }
  virtual void OnNodeViewReplaced(TransportNodeId node,
                                  TransportViewId new_view_id,
                                  TransportViewId old_view_id) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "ViewReplaced node=%s new_view=%s old_view=%s",
            NodeIdToString(node).c_str(),
            NodeIdToString(new_view_id).c_str(),
            NodeIdToString(old_view_id).c_str()));
    QuitIfNecessary();
  }

  void QuitIfNecessary() {
    if (quit_count_ > 0 && --quit_count_ == 0)
      current_run_loop->Quit();
  }

  TransportConnectionId id_;
  TransportChangeId next_server_change_id_;

  // Used to determine when/if to quit the run loop.
  size_t quit_count_;

  Changes changes_;

  // Set of nodes sent when connection created.
  std::vector<TestNode> initial_nodes_;

  // Nodes sent from last OnNodeHierarchyChanged.
  std::vector<TestNode> hierarchy_changed_nodes_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

class ViewManagerConnectionTest : public testing::Test {
 public:
  ViewManagerConnectionTest() {}

  virtual void SetUp() OVERRIDE {
    test_helper_.Init();

    ConnectTo(test_helper_.shell(), "mojo:mojo_view_manager", &view_manager_);
    view_manager_->SetClient(&client_);

    client_.WaitForId();
    client_.GetAndClearChanges();
  }

 protected:
  // Creates a second connection to the viewmanager.
  void EstablishSecondConnection() {
    ConnectTo(test_helper_.shell(), "mojo:mojo_view_manager", &view_manager2_);
    view_manager2_->SetClient(&client2_);

    client2_.WaitForId();
    client2_.GetAndClearChanges();
  }

  void EstablishSecondConnectionWithRoot(TransportNodeId root_id) {
    EstablishSecondConnection();
    client2_.ClearId();

    AllocationScope scope;
    std::vector<uint32_t> roots;
    roots.push_back(root_id);
    ASSERT_TRUE(SetRoots(view_manager_.get(), 2, roots));
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnConnectionEstablished", changes[0]);
    ASSERT_NE(0u, client2_.id());
    const std::vector<TestNode>& nodes(client2_.initial_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,1 parent=null view=null", nodes[0].ToString());
  }

  void DestroySecondConnection() {
    view_manager2_.reset();
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;

  ViewManagerClientImpl client_;
  IViewManagerPtr view_manager_;

  ViewManagerClientImpl client2_;
  IViewManagerPtr view_manager2_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnectionTest);
};

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, ValidId) {
  // All these tests assume 1 for the client id. The only real assertion here is
  // the client id is not zero, but adding this as rest of code here assumes 1.
  EXPECT_EQ(1, client_.id());

  // Change ids start at 1 as well.
  EXPECT_EQ(static_cast<TransportChangeId>(1), client_.next_server_change_id());
}

// Verifies two clients/connections get different ids.
TEST_F(ViewManagerConnectionTest, TwoClientsGetDifferentConnectionIds) {
  EstablishSecondConnection();
  EXPECT_NE(0, client2_.id());
  EXPECT_NE(client_.id(), client2_.id());
}

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, CreateNode) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));

  // Can't create a node with the same id.
  ASSERT_FALSE(CreateNode(view_manager_.get(), 1, 1));
}

TEST_F(ViewManagerConnectionTest, CreateNodeFailsWithBogusConnectionId) {
  EXPECT_FALSE(CreateNode(view_manager_.get(), 2, 1));
}

TEST_F(ViewManagerConnectionTest, CreateViewFailsWithBogusConnectionId) {
  EXPECT_FALSE(CreateView(view_manager_.get(), 2, 1));
}

// Verifies hierarchy changes.
TEST_F(ViewManagerConnectionTest, AddRemoveNotify) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Remove 2 from its parent.
  {
    AllocationScope scope;
    ASSERT_TRUE(RemoveNodeFromParent(view_manager_.get(),
                                     CreateNodeId(client_.id(), 2),
                                     2));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[0]);
  }
}

// Verifies AddNode fails when node is already in position.
TEST_F(ViewManagerConnectionTest, AddNodeWithNoChange) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Try again, this should fail.
  {
    AllocationScope scope;
    EXPECT_FALSE(AddNode(view_manager_.get(),
                         CreateNodeId(client_.id(), 1),
                         CreateNodeId(client_.id(), 2),
                         2));
    Changes changes(client_.GetAndClearChanges());
    EXPECT_TRUE(changes.empty());
  }
}

// Verifies AddNode fails when node is already in position.
TEST_F(ViewManagerConnectionTest, AddAncestorFails) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Try to make 1 a child of 2, this should fail since 1 is an ancestor of 2.
  {
    AllocationScope scope;
    EXPECT_FALSE(AddNode(view_manager_.get(),
                         CreateNodeId(client_.id(), 2),
                         CreateNodeId(client_.id(), 1),
                         2));
    Changes changes(client_.GetAndClearChanges());
    EXPECT_TRUE(changes.empty());
  }
}

// Verifies adding with an invalid id fails.
TEST_F(ViewManagerConnectionTest, AddWithInvalidServerId) {
  // Create two nodes.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  // Make 2 a child of 1. Supply an invalid change id, which should fail.
  {
    AllocationScope scope;
    ASSERT_FALSE(AddNode(view_manager_.get(),
                         CreateNodeId(client_.id(), 1),
                         CreateNodeId(client_.id(), 2),
                         0));
    Changes changes(client_.GetAndClearChanges());
    EXPECT_TRUE(changes.empty());
  }
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerConnectionTest, AddToRoot) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 21));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 3));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 3 a child of 21.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 21),
                        CreateNodeId(client_.id(), 3),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Make 21 a child of the root.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 21),
                        2));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,21 new_parent=0,1 old_parent=null",
        changes[0]);
  }
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerConnectionTest, NodeHierarchyChangedNodes) {
  // Create nodes 1 and 11 with 1 parented to the root and 11 a child of 1.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 11));

  // Make 11 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 11),
                        1));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());
  }

  EstablishSecondConnection();

  // Make 1 a child of the root.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        2));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    // Client 2 should get a hierarchy change that includes the new nodes as it
    // has not yet seen them.
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(2u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=null", nodes[1].ToString());
  }

  // Remove 1 from the root.
  {
    AllocationScope scope;
    ASSERT_TRUE(RemoveNodeFromParent(view_manager_.get(),
                                     CreateNodeId(client_.id(), 1),
                                     3));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,1 new_parent=null old_parent=0,1",
        changes[0]);
  }

  // Create another node, 111, parent it to 11.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 111));
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 11),
                        CreateNodeId(client_.id(), 111),
                        4));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    // Even though 11 isn't attached to the root client 2 is still notified of
    // the change because it was told about 11.
    EXPECT_EQ(
        "HierarchyChanged change_id=4 node=1,111 new_parent=1,11 "
        "old_parent=null", changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,111 parent=1,11 view=null", nodes[0].ToString());
  }

  // Reattach 1 to the root.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        5));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=5 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    ASSERT_TRUE(client2_.hierarchy_changed_nodes().empty());
  }
}

TEST_F(ViewManagerConnectionTest, NodeHierarchyChangedAddingKnownToUnknown) {
  // Create the following structure: root -> 1 -> 11 and 2->21 (2 has no
  // parent).
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 11));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 21));

  // Set up the hierarchy.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        1));
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 11),
                        2));
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateNodeId(client_.id(), 21),
                        3));
  }

  EstablishSecondConnection();

  // Remove 11.
  {
    AllocationScope scope;
    ASSERT_TRUE(RemoveNodeFromParent(view_manager_.get(),
                                     CreateNodeId(client_.id(), 11),
                                     4));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=4 node=1,11 new_parent=null old_parent=1,1",
        changes[0]);
    EXPECT_TRUE(client2_.hierarchy_changed_nodes().empty());
  }

  // Add 11 to 21. As client2 knows about 11 it should receive the new
  // hierarchy.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 21),
                        CreateNodeId(client_.id(), 11),
                        5));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=5 node=1,11 new_parent=1,21 "
        "old_parent=null", changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(2u, nodes.size());
    EXPECT_EQ("node=1,2 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,21 parent=1,2 view=null", nodes[1].ToString());
  }
}

// Verifies connection on told descendants of the root when connecting.
TEST_F(ViewManagerConnectionTest, GetInitialNodesOnInit) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 21));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 3));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Make 3 a child of 21.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 21),
                        CreateNodeId(client_.id(), 3),
                        1));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());
  }

  // Make 21 a child of the root.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 21),
                        2));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());
  }

  EstablishSecondConnection();
  // Should get notification of children of the root.
  const std::vector<TestNode>& nodes(client2_.initial_nodes());
  ASSERT_EQ(3u, nodes.size());
  EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
  EXPECT_EQ("node=1,21 parent=0,1 view=null", nodes[1].ToString());
  EXPECT_EQ("node=1,3 parent=1,21 view=null", nodes[2].ToString());
}

// Verifies DeleteNode works.
TEST_F(ViewManagerConnectionTest, DeleteNode) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        1));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Add 1 to the root
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        2));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=2 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
  }

  // Delete 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 1)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=3 node=1,1", changes[0]);
  }
}

// Verifies DeleteNode isn't allowed from a separate connection.
TEST_F(ViewManagerConnectionTest, DeleteNodeFromAnotherConnectionDisallowed) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  EstablishSecondConnection();
  EXPECT_FALSE(DeleteNode(view_manager2_.get(), CreateNodeId(client_.id(), 1)));
}

// Verifies DeleteView isn't allowed from a separate connection.
TEST_F(ViewManagerConnectionTest, DeleteViewFromAnotherConnectionDisallowed) {
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 1));
  EstablishSecondConnection();
  EXPECT_FALSE(DeleteView(view_manager2_.get(), CreateViewId(client_.id(), 1)));
}

// Verifies if a node was deleted and then reused that other clients are
// properly notified.
TEST_F(ViewManagerConnectionTest, ReusedDeletedId) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();

  // Make 1 a child of the root.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        1));
    EXPECT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    EXPECT_EQ(
        "HierarchyChanged change_id=1 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
  }

  // Delete 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 1)));
    EXPECT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=2 node=1,1", changes[0]);
  }

  // Create 1 again, and add it back to the root. Should get the same
  // notification.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        3));
    EXPECT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes = client2_.GetAndClearChanges();
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,1 new_parent=0,1 old_parent=null",
        changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
  }
}

// Assertions around setting a view.
TEST_F(ViewManagerConnectionTest, SetView) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 11));
  ASSERT_TRUE(AddNode(view_manager_.get(), 1, CreateNodeId(1, 1), 1));
  ASSERT_TRUE(AddNode(view_manager_.get(), 1, CreateNodeId(1, 2), 2));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Set view 11 on node 1.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateViewId(client_.id(), 11)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "ViewReplaced node=1,1 new_view=1,11 old_view=null",
        changes[0]);
  }

  // Set view 11 on node 2.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateViewId(client_.id(), 11)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(2);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=null old_view=1,11",
              changes[0]);
    EXPECT_EQ("ViewReplaced node=1,2 new_view=1,11 old_view=null",
              changes[1]);
  }
}

// Verifies deleting a node with a view sends correct notifications.
TEST_F(ViewManagerConnectionTest, DeleteNodeWithView) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 11));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Set view 11 on node 1.
  ASSERT_TRUE(SetView(view_manager_.get(),
                      CreateNodeId(client_.id(), 1),
                      CreateViewId(client_.id(), 11)));
  client_.GetAndClearChanges();

  EstablishSecondConnection();
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Delete node 1. The second connection should not see this because the node
  // was not known to it.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 1)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 2", changes[0]);
  }

  // Parent 2 to the root.
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));
  client2_.DoRunLoopUntilChangesCount(1);
  client2_.GetAndClearChanges();

  // Set view 11 on node 2.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateViewId(client_.id(), 11)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,2 new_view=1,11 old_view=null", changes[0]);
  }

  // Delete node.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 2)));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_TRUE(changes.empty());

    client2_.DoRunLoopUntilChangesCount(2);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,2 new_view=null old_view=1,11", changes[0]);
    EXPECT_EQ("NodeDeleted change_id=3 node=1,2", changes[1]);
  }
}

// Sets view from one connection on another.
TEST_F(ViewManagerConnectionTest, SetViewFromSecondConnection) {
  EstablishSecondConnection();

  // Create two nodes in first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Create a view in the second connection.
  ASSERT_TRUE(CreateView(view_manager2_.get(), 2, 51));

  // Attach view to node 1 in the first connectoin.
  {
    ASSERT_TRUE(SetView(view_manager2_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateViewId(client2_.id(), 51)));
    client_.DoRunLoopUntilChangesCount(1);
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=2,51 old_view=null", changes[0]);
  }

  // Shutdown the second connection and verify view is removed.
  {
    DestroySecondConnection();
    client_.DoRunLoopUntilChangesCount(2);

    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("ViewReplaced node=1,1 new_view=null old_view=2,51", changes[0]);
    EXPECT_EQ("ViewDeleted view=2,51", changes[1]);
  }
}

// Assertions for GetNodeTree.
TEST_F(ViewManagerConnectionTest, GetNodeTree) {
  EstablishSecondConnection();

  // Create two nodes in first connection, 1 and 11 (11 is a child of 1).
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 11));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(client_.id(), 1),
                      CreateNodeId(client_.id(), 11),
                      2));

  // Create two nodes in second connection, 2 and 3, both children of the root.
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 2, 2));
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 2, 3));
  ASSERT_TRUE(AddNode(view_manager2_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client2_.id(), 2),
                      3));
  ASSERT_TRUE(AddNode(view_manager2_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client2_.id(), 3),
                      4));

  // Attach view to node 11 in the first connection.
  ASSERT_TRUE(CreateView(view_manager_.get(), 1, 51));
  ASSERT_TRUE(SetView(view_manager_.get(),
                      CreateNodeId(client_.id(), 11),
                      CreateViewId(client_.id(), 51)));

  // Verifies GetNodeTree() on the root.
  {
    AllocationScope scope;
    std::vector<TestNode> nodes;
    GetNodeTree(view_manager2_.get(), CreateNodeId(0, 1), &nodes);
    ASSERT_EQ(5u, nodes.size());
    EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[1].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=1,51", nodes[2].ToString());
    EXPECT_EQ("node=2,2 parent=0,1 view=null", nodes[3].ToString());
    EXPECT_EQ("node=2,3 parent=0,1 view=null", nodes[4].ToString());
  }

  // Verifies GetNodeTree() on the node 1,1.
  {
    AllocationScope scope;
    std::vector<TestNode> nodes;
    GetNodeTree(view_manager2_.get(), CreateNodeId(1, 1), &nodes);
    ASSERT_EQ(2u, nodes.size());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,11 parent=1,1 view=1,51", nodes[1].ToString());
  }
}

// Various assertions around SetRoots.
TEST_F(ViewManagerConnectionTest, SetRoots) {
  // Create 1, 2, and 3 in the first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 3));

  // Parent 1 to the root.
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));

  // Establish the second connection and give it the roots 1 and 3.
  EstablishSecondConnection();
  client2_.ClearId();
  {
    AllocationScope scope;
    std::vector<uint32_t> roots;
    roots.push_back(CreateNodeId(1, 1));
    roots.push_back(CreateNodeId(1, 3));
    ASSERT_TRUE(SetRoots(view_manager_.get(), 2, roots));
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("OnConnectionEstablished", changes[0]);
    ASSERT_NE(0u, client2_.id());
    const std::vector<TestNode>& nodes(client2_.initial_nodes());
    ASSERT_EQ(2u, nodes.size());
    EXPECT_EQ("node=1,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,3 parent=null view=null", nodes[1].ToString());
  }

  // Create 4 and add it to the root, connection 2 should only get id advanced.
  {
    ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 4));
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 4),
                        2));
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 3", changes[0]);
  }

  // Move 4 under 3, this should expose 4 to the client.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(1, 3),
                        CreateNodeId(1, 4),
                        3));
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ(
        "HierarchyChanged change_id=3 node=1,4 new_parent=1,3 "
        "old_parent=null", changes[0]);
    const std::vector<TestNode>& nodes(client2_.hierarchy_changed_nodes());
    ASSERT_EQ(1u, nodes.size());
    EXPECT_EQ("node=1,4 parent=1,3 view=null", nodes[0].ToString());
  }

  // Move 4 under 2, since 2 isn't a root client should get a delete.
  {
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(1, 2),
                        CreateNodeId(1, 4),
                        4));
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("NodeDeleted change_id=4 node=1,4", changes[0]);
  }

  // Delete 4, client shouldn't receive a delete since it should no longer know
  // about 4.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(), CreateNodeId(client_.id(), 4)));
    ASSERT_TRUE(client_.GetAndClearChanges().empty());

    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("ServerChangeIdAdvanced 6", changes[0]);
  }
}

// Verify AddNode fails when trying to manipulate nodes in other roots.
TEST_F(ViewManagerConnectionTest, CantMoveNodesFromOtherRoot) {
  // Create 1 and 2 in the first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(
      EstablishSecondConnectionWithRoot(CreateNodeId(1, 1)));

  // Try to move 2 to be a child of 1 from connection 2. This should fail as 2
  // should not be able to access 1.
  ASSERT_FALSE(AddNode(view_manager2_.get(),
                       CreateNodeId(1, 1),
                       CreateNodeId(1, 2),
                       1));

  // Try to reparent 1 to the root. A connection is not allowed to reparent its
  // roots.
  ASSERT_FALSE(AddNode(view_manager2_.get(),
                       CreateNodeId(0, 1),
                       CreateNodeId(1, 1),
                       1));
}


// Verify RemoveNodeFromParent fails for nodes that are descendants of the
// roots.
TEST_F(ViewManagerConnectionTest, CantRemoveNodesInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(
      EstablishSecondConnectionWithRoot(CreateNodeId(1, 1)));

  // Connection 2 should not be able to remove node 2 or 1 from its parent.
  ASSERT_FALSE(RemoveNodeFromParent(view_manager2_.get(),
                                    CreateNodeId(1, 2),
                                    3));
  ASSERT_FALSE(RemoveNodeFromParent(view_manager2_.get(),
                                    CreateNodeId(1, 1),
                                    3));

  // Create nodes 10 and 11 in 2.
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 2, 10));
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 2, 11));

  // Parent 11 to 10.
  ASSERT_TRUE(AddNode(view_manager2_.get(),
                      CreateNodeId(client2_.id(), 10),
                      CreateNodeId(client2_.id(), 11),
                      3));
  // Remove 11 from 10.
  ASSERT_TRUE(RemoveNodeFromParent(view_manager2_.get(),
                                   CreateNodeId(2, 11),
                                   4));

  // Verify nothing was actually removed.
  {
    AllocationScope scope;
    std::vector<TestNode> nodes;
    GetNodeTree(view_manager_.get(), CreateNodeId(0, 1), &nodes);
    ASSERT_EQ(3u, nodes.size());
    EXPECT_EQ("node=0,1 parent=null view=null", nodes[0].ToString());
    EXPECT_EQ("node=1,1 parent=0,1 view=null", nodes[1].ToString());
    EXPECT_EQ("node=1,2 parent=0,1 view=null", nodes[2].ToString());
  }
}

// Verify SetView fails for nodes that are not descendants of the roots.
TEST_F(ViewManagerConnectionTest, CantRemoveSetViewInOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(
      EstablishSecondConnectionWithRoot(CreateNodeId(1, 1)));

  // Create a view in the second connection.
  ASSERT_TRUE(CreateView(view_manager2_.get(), 2, 51));

  // Connection 2 should be able to set the view on node 1 (it's root), but not
  // on 2.
  ASSERT_TRUE(SetView(view_manager2_.get(),
                      CreateNodeId(client_.id(), 1),
                      CreateViewId(client2_.id(), 51)));
  ASSERT_FALSE(SetView(view_manager2_.get(),
                       CreateNodeId(client_.id(), 2),
                       CreateViewId(client2_.id(), 51)));
}

// Verify GetNodeTree fails for nodes that are not descendants of the roots.
TEST_F(ViewManagerConnectionTest, CantGetNodeTreeOfOtherRoots) {
  // Create 1 and 2 in the first connection and parent both to the root.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1, 2));

  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      1));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 2),
                      2));

  // Establish the second connection and give it the root 1.
  ASSERT_NO_FATAL_FAILURE(
      EstablishSecondConnectionWithRoot(CreateNodeId(1, 1)));

  AllocationScope scope;
  std::vector<TestNode> nodes;

  // Should get nothing for the root.
  GetNodeTree(view_manager2_.get(), CreateNodeId(0, 1), &nodes);
  ASSERT_TRUE(nodes.empty());

  // Should get nothing for node 2.
  GetNodeTree(view_manager2_.get(), CreateNodeId(1, 2), &nodes);
  ASSERT_TRUE(nodes.empty());

  // Should get node 1 if asked for.
  GetNodeTree(view_manager2_.get(), CreateNodeId(1, 1), &nodes);
  ASSERT_EQ(1u, nodes.size());
  EXPECT_EQ("node=1,1 parent=null view=null", nodes[0].ToString());
}

// TODO(sky): add coverage of test that destroys connections and ensures other
// connections get deletion notification (or advanced server id).

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
