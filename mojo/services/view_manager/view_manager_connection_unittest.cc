// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/view_manager_connection.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/services/view_manager/root_node_manager.h"
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
std::string NodeIdToString(uint32_t id) {
  return (id == 0) ? "null" :
      base::StringPrintf("%d,%d", FirstIdFromTransportId(id),
                         SecondIdFromTransportId(id));
}

// Boolean callback. Sets |result_cache| to the value of |result| and quits
// the run loop.
void BooleanCallback(bool* result_cache, bool result) {
  *result_cache = result;
  current_run_loop->Quit();
}

// Creates an id used for transport from the specified parameters.
uint32_t CreateNodeId(uint16_t connection_id, uint16_t node_id) {
  return NodeIdToTransportId(NodeId(connection_id, node_id));
}

// Creates a node with the specified id. Returns true on success. Blocks until
// we get back result from server.
bool CreateNode(ViewManager* view_manager, uint16_t id) {
  bool result = false;
  view_manager->CreateNode(id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Adds a node, blocking until done.
bool AddNode(ViewManager* view_manager,
             uint32_t parent,
             uint32_t child,
             int32_t change_id) {
  bool result = false;
  view_manager->AddNode(parent, child, change_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Removes a node, blocking until done.
bool RemoveNodeFromParent(ViewManager* view_manager,
                          uint32_t node_id,
                          int32_t change_id) {
  bool result = false;
  view_manager->RemoveNodeFromParent(node_id, change_id,
                                     base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

}  // namespace

typedef std::vector<std::string> Changes;

class ViewManagerClientImpl : public ViewManagerClient {
 public:
  ViewManagerClientImpl() : id_(0), quit_count_(0) {}

  void set_quit_count(int count) { quit_count_ = count; }

  uint16_t id() const { return id_; }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // ViewManagerClient overrides:
  virtual void OnConnectionEstablished(uint16_t connection_id) OVERRIDE {
    id_ = connection_id;
    current_run_loop->Quit();
  }
  virtual void OnNodeHierarchyChanged(uint32_t node,
                                      uint32_t new_parent,
                                      uint32_t old_parent,
                                      int32_t change_id) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "change_id=%d node=%s new_parent=%s old_parent=%s",
            change_id, NodeIdToString(node).c_str(),
            NodeIdToString(new_parent).c_str(),
            NodeIdToString(old_parent).c_str()));
    if (quit_count_ > 0) {
      if (--quit_count_ == 0)
        current_run_loop->Quit();
    }
  }

  uint16_t id_;

  // Used to determine when/if to quit the run loop.
  int quit_count_;

  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

class ViewManagerConnectionTest : public testing::Test {
 public:
  ViewManagerConnectionTest() : service_factory_(&root_node_manager_) {}

  virtual void SetUp() OVERRIDE {
    InterfacePipe<ViewManagerClient, ViewManager> pipe;
    view_manager_.reset(pipe.handle_to_peer.Pass(), &client_);
    connection_.Initialize(
        &service_factory_,
        ScopedMessagePipeHandle::From(pipe.handle_to_self.Pass()));
    // Wait for the id.
    DoRunLoop();
  }

 protected:
  // Creates a second connection to the viewmanager.
  void EstablishSecondConnection() {
    connection2_.reset(new ViewManagerConnection);
    InterfacePipe<ViewManagerClient, ViewManager> pipe;
    view_manager2_.reset(pipe.handle_to_peer.Pass(), &client2_);
    connection2_->Initialize(
        &service_factory_,
        ScopedMessagePipeHandle::From(pipe.handle_to_self.Pass()));
    // Wait for the id.
    DoRunLoop();
  }

  Environment env_;
  base::MessageLoop loop_;
  RootNodeManager root_node_manager_;
  ServiceConnector<ViewManagerConnection, RootNodeManager> service_factory_;
  ViewManagerConnection connection_;
  ViewManagerClientImpl client_;
  RemotePtr<ViewManager> view_manager_;

  ViewManagerClientImpl client2_;
  RemotePtr<ViewManager> view_manager2_;
  scoped_ptr<ViewManagerConnection> connection2_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerConnectionTest);
};

// Verifies client gets a valid id.
TEST_F(ViewManagerConnectionTest, ValidId) {
  EXPECT_NE(0, client_.id());
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
    Changes changes(client2_.GetAndClearChanges());
    if (changes.empty()) {
      client2_.set_quit_count(1);
      DoRunLoop();
      changes = client2_.GetAndClearChanges();
    }
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

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
