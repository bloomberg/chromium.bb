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
#include "mojo/services/view_manager/root_view_manager.h"
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
std::string ViewIdToString(const ViewId& id) {
  return id.is_null() ? "null" :
      base::StringPrintf("%d,%d", id.manager_id(), id.view_id());
}

// Boolean callback. Sets |result_cache| to the value of |result| and quits
// the run loop.
void BooleanCallback(bool* result_cache, bool result) {
  *result_cache = result;
  current_run_loop->Quit();
}

// Creates a ViewId from the specified parameters.
ViewId CreateViewId(int32_t manager_id, int32_t view_id) {
  ViewId::Builder builder;
  builder.set_manager_id(manager_id);
  builder.set_view_id(view_id);
  return builder.Finish();
}

// Creates a view with the specified id. Returns true on success. Blocks until
// we get back result from server.
bool CreateView(ViewManager* view_manager, int32_t id) {
  bool result = false;
  view_manager->CreateView(id, base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Adds a view, blocking until done.
bool AddView(ViewManager* view_manager,
             const ViewId& parent,
             const ViewId& child,
             int32_t change_id) {
  bool result = false;
  view_manager->AddView(parent, child, change_id,
                        base::Bind(&BooleanCallback, &result));
  DoRunLoop();
  return result;
}

// Removes a view, blocking until done.
bool RemoveViewFromParent(ViewManager* view_manager,
                          const ViewId& view,
                          int32_t change_id) {
  bool result = false;
  view_manager->RemoveViewFromParent(view, change_id,
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

  int32_t id() const { return id_; }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // View overrides:
  virtual void OnConnectionEstablished(int32_t manager_id) OVERRIDE {
    id_ = manager_id;
    current_run_loop->Quit();
  }
  virtual void OnViewHierarchyChanged(const ViewId& view,
                                      const ViewId& new_parent,
                                      const ViewId& old_parent,
                                      int32_t change_id) OVERRIDE {
    changes_.push_back(
        base::StringPrintf(
            "change_id=%d view=%s new_parent=%s old_parent=%s",
            change_id, ViewIdToString(view).c_str(),
            ViewIdToString(new_parent).c_str(),
            ViewIdToString(old_parent).c_str()));
    if (quit_count_ > 0) {
      if (--quit_count_ == 0)
        current_run_loop->Quit();
    }
  }

  int32_t id_;

  // Used to determine when/if to quit the run loop.
  int quit_count_;

  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerClientImpl);
};

class ViewManagerConnectionTest : public testing::Test {
 public:
  ViewManagerConnectionTest() : service_factory_(&root_view_manager_) {}

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
  RootViewManager root_view_manager_;
  ServiceConnector<ViewManagerConnection, RootViewManager> service_factory_;
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
TEST_F(ViewManagerConnectionTest, CreateView) {
  ASSERT_TRUE(CreateView(view_manager_.get(), 1));

  // Can't create a view with the same id.
  ASSERT_FALSE(CreateView(view_manager_.get(), 1));
}

// Verifies hierarchy changes.
TEST_F(ViewManagerConnectionTest, AddRemoveNotify) {
  ASSERT_TRUE(CreateView(view_manager_.get(), 1));
  ASSERT_TRUE(CreateView(view_manager_.get(), 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddView(view_manager_.get(),
                        CreateViewId(client_.id(), 1),
                        CreateViewId(client_.id(), 2),
                        11));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=11 view=1,2 new_parent=1,1 old_parent=null",
              changes[0]);
  }

  // Remove 2 from its parent.
  {
    AllocationScope scope;
    ASSERT_TRUE(RemoveViewFromParent(view_manager_.get(),
                                     CreateViewId(client_.id(), 2),
                                     101));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=101 view=1,2 new_parent=null old_parent=1,1",
              changes[0]);
  }
}

// Verifies hierarchy changes are sent to multiple clients.
TEST_F(ViewManagerConnectionTest, AddRemoveNotifyMultipleConnections) {
  EstablishSecondConnection();

  // Create two views in first connection.
  ASSERT_TRUE(CreateView(view_manager_.get(), 1));
  ASSERT_TRUE(CreateView(view_manager_.get(), 2));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());
  EXPECT_TRUE(client2_.GetAndClearChanges().empty());

  // Make 2 a child of 1.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddView(view_manager_.get(),
                        CreateViewId(client_.id(), 1),
                        CreateViewId(client_.id(), 2),
                        11));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=11 view=1,2 new_parent=1,1 old_parent=null",
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
    EXPECT_EQ("change_id=0 view=1,2 new_parent=1,1 old_parent=null",
              changes[0]);
  }
}

// Verifies adding to root sends right notifications.
TEST_F(ViewManagerConnectionTest, AddToRoot) {
  ASSERT_TRUE(CreateView(view_manager_.get(), 21));
  ASSERT_TRUE(CreateView(view_manager_.get(), 3));

  EXPECT_TRUE(client_.GetAndClearChanges().empty());

  // Make 3 a child of 21.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddView(view_manager_.get(),
                        CreateViewId(client_.id(), 21),
                        CreateViewId(client_.id(), 3),
                        11));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=11 view=1,3 new_parent=1,21 old_parent=null",
              changes[0]);
  }

  // Make 21 a child of the root.
  {
    AllocationScope scope;
    ASSERT_TRUE(AddView(view_manager_.get(),
                        CreateViewId(0, -1),
                        CreateViewId(client_.id(), 21),
                        44));
    Changes changes(client_.GetAndClearChanges());
    ASSERT_EQ(1u, changes.size());
    EXPECT_EQ("change_id=44 view=1,21 new_parent=0,-1 old_parent=null",
              changes[0]);
  }
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
