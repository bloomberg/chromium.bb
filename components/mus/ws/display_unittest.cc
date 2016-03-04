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
#include "components/mus/ws/ids.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_factory.h"
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

class DisplayTest : public testing::Test {
 public:
  DisplayTest() : cursor_id_(0), platform_display_factory_(&cursor_id_) {}
  ~DisplayTest() override {}

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
    PlatformDisplay::set_factory_for_testing(&platform_display_factory_);
    connection_manager_.reset(new ConnectionManager(
        &connection_manager_delegate_, scoped_refptr<SurfacesState>()));
    connection_manager_delegate_.set_connection_manager(
        connection_manager_.get());
  }

 protected:
  int32_t cursor_id_;
  TestPlatformDisplayFactory platform_display_factory_;
  TestConnectionManagerDelegate connection_manager_delegate_;
  scoped_ptr<ConnectionManager> connection_manager_;
  base::MessageLoop message_loop_;
  TestWindowManagerFactory test_window_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayTest);
};

TEST_F(DisplayTest, CallsCreateDefaultDisplays) {
  const int kNumHostsToCreate = 2;
  connection_manager_delegate_.set_num_displays_to_create(kNumHostsToCreate);

  const UserId kTestId1 = 2;
  const UserId kTestId2 = 21;
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId1, &test_window_manager_factory_);
  // The first register should trigger creation of the default
  // Displays. There should be kNumHostsToCreate Displays.
  EXPECT_EQ(static_cast<size_t>(kNumHostsToCreate),
            connection_manager_->displays().size());

  // Each host should have a WindowManagerState for kTestId1.
  for (Display* display : connection_manager_->displays()) {
    EXPECT_EQ(1u, display->num_window_manger_states());
    EXPECT_TRUE(display->GetWindowManagerStateForUser(kTestId1));
    EXPECT_FALSE(display->GetWindowManagerStateForUser(kTestId2));
  }

  // Add another registry, should trigger creation of another wm.
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId2, &test_window_manager_factory_);
  for (Display* display : connection_manager_->displays()) {
    ASSERT_EQ(2u, display->num_window_manger_states());
    WindowManagerState* state1 =
        display->GetWindowManagerStateForUser(kTestId1);
    ASSERT_TRUE(state1);
    WindowManagerState* state2 =
        display->GetWindowManagerStateForUser(kTestId2);
    ASSERT_TRUE(state2);
    // Verify the two states have different roots.
    EXPECT_NE(state1, state2);
    EXPECT_NE(state1->root(), state2->root());
  }
}

TEST_F(DisplayTest, Destruction) {
  connection_manager_delegate_.set_num_displays_to_create(1);

  const UserId kTestId1 = 2;
  const UserId kTestId2 = 21;
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId1, &test_window_manager_factory_);

  // Add another registry, should trigger creation of another wm.
  WindowManagerFactoryRegistryTestApi(
      connection_manager_->window_manager_factory_registry())
      .AddService(kTestId2, &test_window_manager_factory_);
  ASSERT_EQ(1u, connection_manager_->displays().size());
  Display* display = *connection_manager_->displays().begin();
  ASSERT_EQ(2u, display->num_window_manger_states());
  // There should be two trees, one for each windowmanager.
  EXPECT_EQ(2u, connection_manager_->num_trees());

  {
    WindowManagerState* state = display->GetWindowManagerStateForUser(kTestId1);
    // Destroy the tree associated with |state|. Should result in deleting
    // |state|.
    connection_manager_->DestroyTree(state->tree());
    ASSERT_EQ(1u, display->num_window_manger_states());
    EXPECT_FALSE(display->GetWindowManagerStateForUser(kTestId1));
    EXPECT_EQ(1u, connection_manager_->displays().size());
    EXPECT_EQ(1u, connection_manager_->num_trees());
  }

  EXPECT_FALSE(connection_manager_delegate_.got_on_no_more_connections());
  // Destroy the Display, which should shutdown the trees.
  connection_manager_->DestroyDisplay(display);
  EXPECT_EQ(0u, connection_manager_->num_trees());
  EXPECT_TRUE(connection_manager_delegate_.got_on_no_more_connections());
}

}  // namespace test
}  // namespace ws
}  // namespace mus
