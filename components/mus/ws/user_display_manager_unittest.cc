// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/mus/common/types.h"
#include "components/mus/common/util.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/ids.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_factory.h"
#include "components/mus/ws/platform_display_init_params.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/test_utils.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_server_delegate.h"
#include "components/mus/ws/window_tree.h"
#include "components/mus/ws/window_tree_binding.h"
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

class TestDisplayManagerObserver : public mojom::DisplayManagerObserver {
 public:
  TestDisplayManagerObserver() {}
  ~TestDisplayManagerObserver() override {}

  std::string GetAndClearObserverCalls() {
    std::string result;
    std::swap(observer_calls_, result);
    return result;
  }

 private:
  void AddCall(const std::string& call) {
    if (!observer_calls_.empty())
      observer_calls_ += "\n";
    observer_calls_ += call;
  }

  std::string DisplayIdsToString(
      const mojo::Array<mojom::DisplayPtr>& displays) {
    std::string display_ids;
    for (const auto& display : displays) {
      if (!display_ids.empty())
        display_ids += " ";
      display_ids += base::Int64ToString(display->id);
    }
    return display_ids;
  }

  // mojom::DisplayManagerObserver:
  void OnDisplays(mojo::Array<mojom::DisplayPtr> displays) override {
    AddCall("OnDisplays " + DisplayIdsToString(displays));
  }
  void OnDisplaysChanged(mojo::Array<mojom::DisplayPtr> displays) override {
    AddCall("OnDisplaysChanged " + DisplayIdsToString(displays));
  }
  void OnDisplayRemoved(int64_t id) override {
    AddCall("OnDisplayRemoved " + base::Int64ToString(id));
  }

  std::string observer_calls_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayManagerObserver);
};

mojom::FrameDecorationValuesPtr CreateDefaultFrameDecorationValues() {
  mojom::FrameDecorationValuesPtr frame_decoration_values =
      mojom::FrameDecorationValues::New();
  frame_decoration_values->normal_client_area_insets = mojo::Insets::New();
  frame_decoration_values->maximized_client_area_insets = mojo::Insets::New();
  return frame_decoration_values;
}

}  // namespace

// -----------------------------------------------------------------------------

class UserDisplayManagerTest : public testing::Test {
 public:
  UserDisplayManagerTest()
      : cursor_id_(0), platform_display_factory_(&cursor_id_) {}
  ~UserDisplayManagerTest() override {}

 protected:
  // testing::Test:
  void SetUp() override {
    PlatformDisplay::set_factory_for_testing(&platform_display_factory_);
    window_server_.reset(new WindowServer(&window_server_delegate_,
                                          scoped_refptr<SurfacesState>()));
    window_server_delegate_.set_window_server(window_server_.get());
  }

 protected:
  int32_t cursor_id_;
  TestPlatformDisplayFactory platform_display_factory_;
  TestWindowServerDelegate window_server_delegate_;
  std::unique_ptr<WindowServer> window_server_;
  base::MessageLoop message_loop_;
  TestWindowManagerFactory test_window_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserDisplayManagerTest);
};

TEST_F(UserDisplayManagerTest, OnlyNotifyWhenFrameDecorationsSet) {
  window_server_delegate_.set_num_displays_to_create(1);

  const UserId kUserId1 = "2";
  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server_->display_manager();
  WindowManagerFactoryRegistryTestApi(
      window_server_->window_manager_factory_registry())
      .AddService(kUserId1, &test_window_manager_factory_);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);
  UserDisplayManagerTestApi(user_display_manager1)
      .SetTestObserver(&display_manager_observer1);
  // Observer should not have been notified yet.
  EXPECT_EQ(std::string(),
            display_manager_observer1.GetAndClearObserverCalls());

  // Set the frame decoration values, which should trigger sending immediately.
  ASSERT_EQ(1u, display_manager->displays().size());
  Display* display1 = *display_manager->displays().begin();
  display1->GetWindowManagerStateForUser(kUserId1)->SetFrameDecorationValues(
      CreateDefaultFrameDecorationValues());
  EXPECT_EQ("OnDisplays 1",
            display_manager_observer1.GetAndClearObserverCalls());

  UserDisplayManagerTestApi(user_display_manager1).SetTestObserver(nullptr);
}

TEST_F(UserDisplayManagerTest, AddObserverAfterFrameDecorationsSet) {
  window_server_delegate_.set_num_displays_to_create(1);

  const UserId kUserId1 = "2";
  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server_->display_manager();
  WindowManagerFactoryRegistryTestApi(
      window_server_->window_manager_factory_registry())
      .AddService(kUserId1, &test_window_manager_factory_);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);
  ASSERT_EQ(1u, display_manager->displays().size());
  Display* display1 = *display_manager->displays().begin();
  display1->GetWindowManagerStateForUser(kUserId1)->SetFrameDecorationValues(
      CreateDefaultFrameDecorationValues());

  UserDisplayManagerTestApi(user_display_manager1)
      .SetTestObserver(&display_manager_observer1);
  EXPECT_EQ("OnDisplays 1",
            display_manager_observer1.GetAndClearObserverCalls());

  UserDisplayManagerTestApi(user_display_manager1).SetTestObserver(nullptr);
}

TEST_F(UserDisplayManagerTest, AddRemoveDisplay) {
  window_server_delegate_.set_num_displays_to_create(1);

  const UserId kUserId1 = "2";
  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server_->display_manager();
  WindowManagerFactoryRegistryTestApi(
      window_server_->window_manager_factory_registry())
      .AddService(kUserId1, &test_window_manager_factory_);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);
  ASSERT_EQ(1u, display_manager->displays().size());
  Display* display1 = *display_manager->displays().begin();
  display1->GetWindowManagerStateForUser(kUserId1)->SetFrameDecorationValues(
      CreateDefaultFrameDecorationValues());
  UserDisplayManagerTestApi(user_display_manager1)
      .SetTestObserver(&display_manager_observer1);
  EXPECT_EQ("OnDisplays 1",
            display_manager_observer1.GetAndClearObserverCalls());

  // Add another display.
  Display* display2 =
      new Display(window_server_.get(), PlatformDisplayInitParams());
  display2->Init(nullptr);

  // Observer should not have been notified yet as frame decorations not set.
  EXPECT_EQ("", display_manager_observer1.GetAndClearObserverCalls());
  display2->GetWindowManagerStateForUser(kUserId1)->SetFrameDecorationValues(
      CreateDefaultFrameDecorationValues());
  EXPECT_EQ("OnDisplaysChanged 2",
            display_manager_observer1.GetAndClearObserverCalls());

  // Remove the display and verify observer is notified.
  display_manager->DestroyDisplay(display2);
  display2 = nullptr;
  EXPECT_EQ("OnDisplayRemoved 2",
            display_manager_observer1.GetAndClearObserverCalls());

  UserDisplayManagerTestApi(user_display_manager1).SetTestObserver(nullptr);
}

TEST_F(UserDisplayManagerTest, NegativeCoordinates) {
  window_server_delegate_.set_num_displays_to_create(1);

  const UserId kUserId1 = "2";
  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server_->display_manager();
  WindowManagerFactoryRegistryTestApi(
      window_server_->window_manager_factory_registry())
      .AddService(kUserId1, &test_window_manager_factory_);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);

  user_display_manager1->OnMouseCursorLocationChanged(gfx::Point(-10, -11));

  base::subtle::Atomic32* cursor_location_memory = nullptr;
  mojo::ScopedSharedBufferHandle handle =
      user_display_manager1->GetCursorLocationMemory();
  MojoResult result = mojo::MapBuffer(
      handle.get(), 0,
      sizeof(base::subtle::Atomic32),
      reinterpret_cast<void**>(&cursor_location_memory),
      MOJO_MAP_BUFFER_FLAG_NONE);
  ASSERT_EQ(MOJO_RESULT_OK, result);

  base::subtle::Atomic32 location =
      base::subtle::NoBarrier_Load(cursor_location_memory);
  EXPECT_EQ(gfx::Point(static_cast<int16_t>(location >> 16),
                       static_cast<int16_t>(location & 0xFFFF)),
            gfx::Point(-10, -11));
}

}  // namespace test
}  // namespace ws
}  // namespace mus
