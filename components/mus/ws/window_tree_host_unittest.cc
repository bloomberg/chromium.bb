// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "components/mus/common/types.h"
#include "components/mus/common/util.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/client_connection.h"
#include "components/mus/ws/connection_manager.h"
#include "components/mus/ws/connection_manager_delegate.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/display_manager_factory.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_utils.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_tree_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {
namespace ws {
namespace test {
namespace {

class TestWindowManagerFactory : public mojom::WindowManagerFactory {
 public:
  TestWindowManagerFactory() {}
  ~TestWindowManagerFactory() override {}

  // mojom::WindowManagerFactory:
  void CreateWindowManager(
      mus::mojom::DisplayPtr display,
      mus::mojom::WindowTreeClientRequest client) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerFactory);
};

}  // namespace

// -----------------------------------------------------------------------------

class WindowTreeHostTest : public testing::Test {
 public:
  WindowTreeHostTest() : cursor_id_(0), display_manager_factory_(&cursor_id_) {}
  ~WindowTreeHostTest() override {}

  // WindowTreeImpl for the window manager.
  WindowTreeImpl* wm_connection() {
    return connection_manager_->GetConnection(1);
  }

  TestWindowTreeClient* last_window_tree_client() {
    return connection_manager_delegate_.last_client();
  }

  TestClientConnection* last_client_connection() {
    return connection_manager_delegate_.last_connection();
  }

  ConnectionManager* connection_manager() { return connection_manager_.get(); }

  ServerWindow* GetWindowById(const WindowId& id) {
    return connection_manager_->GetWindow(id);
  }

  mus::mojom::Cursor cursor_id() {
    return static_cast<mus::mojom::Cursor>(cursor_id_);
  }

  void set_window_manager_internal(WindowTreeImpl* connection,
                                   mojom::WindowManager* wm_internal) {
    WindowTreeTestApi(connection).set_window_manager_internal(wm_internal);
  }

 protected:
  // testing::Test:
  void SetUp() override {
    DisplayManager::set_factory_for_testing(&display_manager_factory_);
    connection_manager_.reset(new ConnectionManager(
        &connection_manager_delegate_, scoped_refptr<SurfacesState>()));
    connection_manager_delegate_.set_connection_manager(
        connection_manager_.get());
  }

 protected:
  int32_t cursor_id_;
  TestDisplayManagerFactory display_manager_factory_;
  TestConnectionManagerDelegate connection_manager_delegate_;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;
  TestWindowManagerFactory test_window_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostTest);
};

TEST_F(WindowTreeHostTest, CallsCreateDefaultWindowTreeHosts) {
  const int kNumHostsToCreate = 2;
  connection_manager_delegate_.set_num_tree_hosts_to_create(kNumHostsToCreate);

  const UserId kTestId1 = 2;
  const UserId kTestId2 = 21;
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId1, &test_window_manager_factory_);
  // The first register should trigger creation of the default
  // WindowTreeHosts. There should be kNumHostsToCreate WindowTreeHosts.
  EXPECT_EQ(static_cast<size_t>(kNumHostsToCreate),
            connection_manager_->hosts().size());

  // Each host should have a WindowManagerState for kTestId1.
  for (WindowTreeHostImpl* tree_host : connection_manager_->hosts()) {
    EXPECT_EQ(1u, tree_host->num_window_manger_states());
    EXPECT_TRUE(tree_host->GetWindowManagerStateForUser(kTestId1));
    EXPECT_FALSE(tree_host->GetWindowManagerStateForUser(kTestId2));
  }

  // Add another registry, should trigger creation of another wm.
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId2, &test_window_manager_factory_);
  for (WindowTreeHostImpl* tree_host : connection_manager_->hosts()) {
    ASSERT_EQ(2u, tree_host->num_window_manger_states());
    WindowManagerState* state1 =
        tree_host->GetWindowManagerStateForUser(kTestId1);
    ASSERT_TRUE(state1);
    WindowManagerState* state2 =
        tree_host->GetWindowManagerStateForUser(kTestId2);
    ASSERT_TRUE(state2);
    // Verify the two states have different roots.
    EXPECT_NE(state1, state2);
    EXPECT_NE(state1->root(), state2->root());
  }
}

TEST_F(WindowTreeHostTest, Destruction) {
  connection_manager_delegate_.set_num_tree_hosts_to_create(1);

  const UserId kTestId1 = 2;
  const UserId kTestId2 = 21;
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId1, &test_window_manager_factory_);

  // Add another registry, should trigger creation of another wm.
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId2, &test_window_manager_factory_);
  ASSERT_EQ(1u, connection_manager_->hosts().size());
  WindowTreeHostImpl* tree_host = *connection_manager_->hosts().begin();
  ASSERT_EQ(2u, tree_host->num_window_manger_states());
  // There should be two trees, one for each windowmanager.
  EXPECT_EQ(2u, connection_manager_->num_trees());

  {
    WindowManagerState* state =
        tree_host->GetWindowManagerStateForUser(kTestId1);
    // Destroy the tree associated with |state|. Should result in deleting
    // |state|.
    connection_manager_->DestroyTree(state->tree());
    ASSERT_EQ(1u, tree_host->num_window_manger_states());
    EXPECT_FALSE(tree_host->GetWindowManagerStateForUser(kTestId1));
    EXPECT_EQ(1u, connection_manager_->hosts().size());
    EXPECT_EQ(1u, connection_manager_->num_trees());
  }

  EXPECT_FALSE(connection_manager_delegate_.got_on_no_more_connections());
  // Destroy the WindowTreeHost, which should shutdown the trees.
  connection_manager_->DestroyHost(tree_host);
  EXPECT_EQ(0u, connection_manager_->num_trees());
  EXPECT_TRUE(connection_manager_delegate_.got_on_no_more_connections());
}

}  // namespace test
}  // namespace ws
}  // namespace mus
