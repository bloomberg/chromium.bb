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
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace services {
namespace view_manager {

namespace {

base::RunLoop* current_run_loop = NULL;

// Sets |current_run_loop| and runs it. It is expected that someone else quits
// the loop.
void DoRunLoop() {
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

// Callback that results in a vector of INodes. The INodes are converted to
// TestNodes.
void INodesCallback(std::vector<TestNode>* test_nodes,
                    const mojo::Array<INode>& data) {
  for (size_t i = 0; i < data.size(); ++i) {
    TestNode node;
    node.parent_id = data[i].parent_id();
    node.node_id = data[i].node_id();
    node.view_id = data[i].view_id();
    test_nodes->push_back(node);
  }
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
                TransportConnectionSpecificNodeId id) {
  bool result = false;
  view_manager->CreateNode(id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// TODO(sky): make a macro for these functions, they are all the same.

// Deletes a node, blocking until done.
bool DeleteNode(IViewManager* view_manager,
                TransportNodeId node_id,
                TransportChangeId change_id) {
  bool result = false;
  view_manager->DeleteNode(node_id, change_id,
                           base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Adds a node, blocking until done.
bool AddNode(IViewManager* view_manager,
             TransportNodeId parent,
             TransportNodeId child,
             TransportChangeId change_id) {
  bool result = false;
  view_manager->AddNode(parent, child, change_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Removes a node, blocking until done.
bool RemoveNodeFromParent(IViewManager* view_manager,
                          TransportNodeId node_id,
                          TransportChangeId change_id) {
  bool result = false;
  view_manager->RemoveNodeFromParent(node_id, change_id,
                                     base::Bind(&BooleanCallback, &result));
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
                TransportConnectionSpecificViewId id) {
  bool result = false;
  view_manager->CreateView(id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Sets a view on the specified node. Returns true on success. Blocks until we
// get back result from server.
bool SetView(IViewManager* view_manager,
             TransportNodeId node_id,
             TransportViewId view_id,
             TransportChangeId change_id) {
  bool result = false;
  view_manager->SetView(node_id, view_id, change_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

}  // namespace

typedef std::vector<std::string> Changes;

class ViewManagerClientImpl : public IViewManagerClient {
 public:
  ViewManagerClientImpl() : id_(0), quit_count_(0) {}

  TransportConnectionId id() const { return id_; }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

  void WaitForId() {
    if (id_ == 0)
      DoRunLoop();
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
      TransportConnectionId connection_id) OVERRIDE {
    id_ = connection_id;
    if (current_run_loop)
      current_run_loop->Quit();
  }
  virtual void OnNodeHierarchyChanged(TransportNodeId node,
                                      TransportNodeId new_parent,
                                      TransportNodeId old_parent,
                                      TransportChangeId change_id) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "change_id=%d node=%s new_parent=%s old_parent=%s",
            static_cast<int>(change_id), NodeIdToString(node).c_str(),
            NodeIdToString(new_parent).c_str(),
            NodeIdToString(old_parent).c_str()));
    QuitIfNecessary();
  }
  virtual void OnNodeViewReplaced(TransportNodeId node,
                                  TransportViewId new_view_id,
                                  TransportViewId old_view_id,
                                  TransportChangeId change_id) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "change_id=%d node=%s new_view=%s old_view=%s",
            static_cast<int>(change_id), NodeIdToString(node).c_str(),
            NodeIdToString(new_view_id).c_str(),
            NodeIdToString(old_view_id).c_str()));
    QuitIfNecessary();
  }

  void QuitIfNecessary() {
    if (quit_count_ > 0 && --quit_count_ == 0)
      current_run_loop->Quit();
  }

  TransportConnectionId id_;

  // Used to determine when/if to quit the run loop.
  size_t quit_count_;

  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

class ViewManagerConnectionTest : public testing::Test {
 public:
  ViewManagerConnectionTest() {}

  virtual void SetUp() OVERRIDE {
    AllocationScope allocation_scope;

    test_helper_.Init();

    InterfacePipe<IViewManager, AnyInterface> pipe;
    test_helper_.shell()->Connect("mojo:mojo_view_manager",
                                  pipe.handle_to_peer.Pass());
    view_manager_.reset(pipe.handle_to_self.Pass(), &client_);

    client_.WaitForId();
  }

 protected:
  // Creates a second connection to the viewmanager.
  void EstablishSecondConnection() {
    AllocationScope allocation_scope;
    InterfacePipe<IViewManager, AnyInterface> pipe;
    test_helper_.shell()->Connect("mojo:mojo_view_manager",
                                  pipe.handle_to_peer.Pass());
    view_manager2_.reset(pipe.handle_to_self.Pass(), &client2_);

    client2_.WaitForId();
  }

  void DestroySecondConnection() {
    view_manager2_.reset();
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;

  ViewManagerClientImpl client_;
  RemotePtr<IViewManager> view_manager_;

  ViewManagerClientImpl client2_;
  RemotePtr<IViewManager> view_manager2_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnectionTest);
};

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, ValidId) {
  // All these tests assume 1 for the client id. The only real assertion here is
  // the client id is not zero, but adding this as rest of code here assumes 1.
  EXPECT_EQ(1, client_.id());
}

// Verifies two clients/connections get different ids.
TEST_F(ViewManagerConnectionTest, TwoClientsGetDifferentConnectionIds) {
  EstablishSecondConnection();
  EXPECT_NE(0, client2_.id());
  EXPECT_NE(client_.id(), client2_.id());
}

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, CreateNode) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));

  // Can't create a node with the same id.
  ASSERT_FALSE(CreateNode(view_manager_.get(), 1));
}

// Verifies hierarchy changes.
TEST_F(ViewManagerConnectionTest, AddRemoveNotify) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        11));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=11 node=1,2 new_parent=1,1 old_parent=null",
              changes[0]);
  }

  // Remove 2 from its parent.
  {
    AllocationScope scope;
    ASSERT_TRUE(RemoveNodeFromParent(view_manager_.get(),
                                     CreateNodeId(client_.id(), 2),
                                     101));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=101 node=1,2 new_parent=null old_parent=1,1",
              changes[0]);
  }
}

// Verifies hierarchy changes are sent to multiple clients.
TEST_F(ViewManagerConnectionTest, AddRemoveNotifyMultipleConnections) {
  EstablishSecondConnection();

  // Create two nodes in first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        11));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=11 node=1,2 new_parent=1,1 old_parent=null",
              changes[0]);
  }

  // Second client should also have received the change.
  {
    client2_.DoRunLoopUntilChangesCount(1);
    Changes changes(client2_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=0 node=1,2 new_parent=1,1 old_parent=null",
              changes[0]);
  }
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerConnectionTest, AddToRoot) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 21));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 3));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Make 3 a child of 21.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 21),
                        CreateNodeId(client_.id(), 3),
                        11));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=11 node=1,3 new_parent=1,21 old_parent=null",
              changes[0]);
  }

  // Make 21 a child of the root.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 21),
                        44));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=44 node=1,21 new_parent=0,1 old_parent=null",
              changes[0]);
  }
}

// Verifies DeleteNode works.
TEST_F(ViewManagerConnectionTest, DeleteNode) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 2));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateNodeId(client_.id(), 2),
                        11));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=11 node=1,2 new_parent=1,1 old_parent=null",
              changes[0]);
  }

  // Add 1 to the root
  {
    AllocationScope scope;
    ASSERT_TRUE(AddNode(view_manager_.get(),
                        CreateNodeId(0, 1),
                        CreateNodeId(client_.id(), 1),
                        101));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=101 node=1,1 new_parent=0,1 old_parent=null",
              changes[0]);
  }

  // Delete 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(DeleteNode(view_manager_.get(),
                           CreateNodeId(client_.id(), 1),
                           121));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("change_id=121 node=1,1 new_parent=null old_parent=0,1",
              changes[0]);
    EXPECT_EQ("change_id=121 node=1,2 new_parent=null old_parent=1,1",
              changes[1]);
  }
}

// Assertions around setting a view.
TEST_F(ViewManagerConnectionTest, SetView) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 2));
  ASSERT_TRUE(CreateView(view_manager_.get(), 11));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Set view 11 on node 1.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateViewId(client_.id(), 11),
                        21));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=21 node=1,1 new_view=1,11 old_view=null",
              changes[0]);
  }

  // Set view 11 on node 2.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateViewId(client_.id(), 11),
                        22));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(2u, changes.size());
    EXPECT_EQ("change_id=22 node=1,1 new_view=null old_view=1,11",
              changes[0]);
    EXPECT_EQ("change_id=22 node=1,2 new_view=1,11 old_view=null",
              changes[1]);
  }
}

// Verifies deleting a node with a view sends correct notifications.
TEST_F(ViewManagerConnectionTest, DeleteNodeWithView) {
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 2));
  ASSERT_TRUE(CreateView(view_manager_.get(), 11));
  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Set view 11 on node 1.
  ASSERT_TRUE(SetView(view_manager_.get(),
                      CreateNodeId(client_.id(), 1),
                      CreateViewId(client_.id(), 11),
                      21));
  client_.GetAndClearChanges();

  // Delete node 1.
  {
    ASSERT_TRUE(DeleteNode(view_manager_.get(),
                           CreateNodeId(client_.id(), 1),
                           121));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=121 node=1,1 new_view=null old_view=1,11",
              changes[0]);
  }

  // Set view 11 on node 2.
  {
    ASSERT_TRUE(SetView(view_manager_.get(),
                        CreateNodeId(client_.id(), 2),
                        CreateViewId(client_.id(), 11),
                        22));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=22 node=1,2 new_view=1,11 old_view=null", changes[0]);
  }
}

// Sets view from one connection on another.
TEST_F(ViewManagerConnectionTest, SetViewFromSecondConnection) {
  EstablishSecondConnection();

  // Create two nodes in first connection.
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Create a view in the second connection.
  ASSERT_TRUE(CreateView(view_manager2_.get(), 51));

  // Attach view to node 1 in the first connection.
  {
    ASSERT_TRUE(SetView(view_manager2_.get(),
                        CreateNodeId(client_.id(), 1),
                        CreateViewId(client2_.id(), 51),
                        22));
    client_.DoRunLoopUntilChangesCount(1);
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=0 node=1,1 new_view=2,51 old_view=null", changes[0]);

    client2_.DoRunLoopUntilChangesCount(1);
    changes = client2_.GetAndClearChanges();
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=22 node=1,1 new_view=2,51 old_view=null", changes[0]);
  }

  // Shutdown the second connection and verify view is removed.
  {
    DestroySecondConnection();
    client_.DoRunLoopUntilChangesCount(1);

    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=0 node=1,1 new_view=null old_view=2,51", changes[0]);
  }
}

// Assertions for GetNodeTree.
TEST_F(ViewManagerConnectionTest, GetNodeTree) {
  EstablishSecondConnection();

  // Create two nodes in first connection, 1 and 11 (11 is a child of 1).
  ASSERT_TRUE(CreateNode(view_manager_.get(), 1));
  ASSERT_TRUE(CreateNode(view_manager_.get(), 11));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client_.id(), 1),
                      101));
  ASSERT_TRUE(AddNode(view_manager_.get(),
                      CreateNodeId(client_.id(), 1),
                      CreateNodeId(client_.id(), 11),
                      102));

  // Create two nodes in second connection, 2 and 3, both children of the root.
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 2));
  ASSERT_TRUE(CreateNode(view_manager2_.get(), 3));
  ASSERT_TRUE(AddNode(view_manager2_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client2_.id(), 2),
                      99));
  ASSERT_TRUE(AddNode(view_manager2_.get(),
                      CreateNodeId(0, 1),
                      CreateNodeId(client2_.id(), 3),
                      99));

  // Attach view to node 11 in the first connection.
  ASSERT_TRUE(CreateView(view_manager_.get(), 51));
  ASSERT_TRUE(SetView(view_manager_.get(),
                      CreateNodeId(client_.id(), 11),
                      CreateViewId(client_.id(), 51),
                      22));

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

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
