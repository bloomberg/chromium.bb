// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <vector>

#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_service.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/window_properties.mojom.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/session_manager/session_manager_types.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/screen_base.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/wm_state.h"

namespace ash {

class WindowTreeClientDelegate : public aura::WindowTreeClientDelegate {
 public:
  WindowTreeClientDelegate() {}
  ~WindowTreeClientDelegate() override {}

  void WaitForEmbed() { run_loop_.Run(); }

  void DestroyWindowTreeHost() { window_tree_host_.reset(); }

 private:
  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override {
    window_tree_host_ = std::move(window_tree_host);
    run_loop_.Quit();
  }
  void OnEmbedRootDestroyed(
      aura::WindowTreeHostMus* window_tree_host) override {}
  void OnLostConnection(aura::WindowTreeClient* client) override {}
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override {}
  aura::PropertyConverter* GetPropertyConverter() override {
    return &property_converter_;
  }

  base::RunLoop run_loop_;
  ::wm::WMState wm_state_;
  aura::PropertyConverter property_converter_;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientDelegate);
};

class WindowManagerServiceTest : public service_manager::test::ServiceTest {
 public:
  WindowManagerServiceTest()
      : service_manager::test::ServiceTest("mash_unittests") {}
  ~WindowManagerServiceTest() override {}

  void TearDown() override {
    // Unset the screen installed by the test.
    display::Screen::SetScreenInstance(nullptr);
    service_manager::test::ServiceTest::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerServiceTest);
};

void OnEmbed(bool success) {
  ASSERT_TRUE(success);
}

TEST_F(WindowManagerServiceTest, OpenWindow) {
  display::ScreenBase screen;
  screen.display_list().AddDisplay(
      display::Display(1, gfx::Rect(0, 0, 200, 200)),
      display::DisplayList::Type::PRIMARY);
  display::Screen::SetScreenInstance(&screen);

  WindowTreeClientDelegate window_tree_delegate;

  connector()->StartService(mojom::kServiceName);

  // Connect to mus and create a new top level window. The request goes to
  // |ash|, but is async.
  aura::WindowTreeClient client(connector(), &window_tree_delegate, nullptr,
                                nullptr, nullptr, false);
  client.ConnectViaWindowTreeFactory();
  aura::test::EnvTestHelper().SetWindowTreeClient(&client);
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ui::mojom::WindowType::WINDOW));
  aura::WindowTreeHostMus window_tree_host_mus(
      aura::CreateInitParamsForTopLevel(&client, std::move(properties)));
  window_tree_host_mus.InitHost();
  aura::Window* child_window = new aura::Window(nullptr);
  child_window->Init(ui::LAYER_NOT_DRAWN);
  window_tree_host_mus.window()->AddChild(child_window);

  // Create another WindowTreeClient by way of embedding in
  // |child_window|. This blocks until it succeeds.
  ui::mojom::WindowTreeClientPtr tree_client;
  auto tree_client_request = MakeRequest(&tree_client);
  client.Embed(child_window, std::move(tree_client), 0u, base::Bind(&OnEmbed));
  aura::WindowTreeClient child_client(connector(), &window_tree_delegate,
                                      nullptr, std::move(tree_client_request),
                                      nullptr, false);
  window_tree_delegate.WaitForEmbed();
  ASSERT_TRUE(!child_client.GetRoots().empty());
  window_tree_delegate.DestroyWindowTreeHost();
}

using WindowManagerTest = AshTestBase;

TEST_F(WindowManagerTest, SystemModalLockIsntReparented) {
  ash_test_helper()->test_session_controller_client()->SetSessionState(
      session_manager::SessionState::LOCKED);
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  aura::Window* system_modal_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_LockSystemModalContainer);
  system_modal_container->AddChild(window.get());
  aura::WindowManagerDelegate* window_manager_delegate =
      ash_test_helper()->window_manager_service()->window_manager();
  window_manager_delegate->OnWmSetModalType(window.get(),
                                            ui::MODAL_TYPE_SYSTEM);
  ASSERT_TRUE(window->parent());
  // Setting to system modal should not reparent.
  EXPECT_EQ(kShellWindowId_LockSystemModalContainer, window->parent()->id());
}

TEST_F(WindowManagerTest, CanConsumeSystemKeysFromContentBrowser) {
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ash::mojom::kCanConsumeSystemKeys_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int64_t>(true));

  aura::WindowManagerDelegate* window_manager_delegate =
      ash_test_helper()->window_manager_service()->window_manager();
  aura::Window* window = window_manager_delegate->OnWmCreateTopLevelWindow(
      ui::mojom::WindowType::WINDOW, &properties);

  EXPECT_EQ(true, window->GetProperty(kCanConsumeSystemKeysKey));
}

}  // namespace ash
