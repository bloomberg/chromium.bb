// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/display_list.h"
#include "ui/display/screen_base.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/wm_state.h"

namespace ash {
namespace mus {

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
  wm::WMState wm_state_;
  aura::PropertyConverter property_converter_;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientDelegate);
};

class WindowManagerTest : public service_manager::test::ServiceTest {
 public:
  WindowManagerTest() : service_manager::test::ServiceTest("mash_unittests") {}
  ~WindowManagerTest() override {}

  // service_manager::test::ServiceTest:
  void SetUp() override {
    service_manager::test::ServiceTest::SetUp();

    // This test connects to the real mus. This results in
    // aura::WindowTreeClient resetting the context_factory on Env. As all
    // tests share the same Env (see mash_test_suite) we need to restore the
    // context_factory when done.
    aura::Env* env = aura::Env::GetInstance();
    initial_context_factory_ = env->context_factory();
    initial_context_factory_private_ = env->context_factory_private();
  }

  void TearDown() override {
    aura::Env* env = aura::Env::GetInstance();
    env->set_context_factory(initial_context_factory_);
    env->set_context_factory_private(initial_context_factory_private_);
  }

 private:
  ui::ContextFactory* initial_context_factory_ = nullptr;
  ui::ContextFactoryPrivate* initial_context_factory_private_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerTest);
};

void OnEmbed(bool success) {
  ASSERT_TRUE(success);
}

TEST_F(WindowManagerTest, OpenWindow) {
  display::ScreenBase screen;
  screen.display_list().AddDisplay(
      display::Display(1, gfx::Rect(0, 0, 200, 200)),
      display::DisplayList::Type::PRIMARY);
  display::Screen::SetScreenInstance(&screen);

  WindowTreeClientDelegate window_tree_delegate;

  connector()->Connect("ash");

  // Connect to mus and create a new top level window. The request goes to
  // |ash|, but is async.
  aura::WindowTreeClient client(connector(), &window_tree_delegate);
  client.ConnectViaWindowTreeFactory();
  aura::test::EnvTestHelper().SetWindowTreeClient(&client);
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[ui::mojom::WindowManager::kWindowType_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ui::mojom::WindowType::WINDOW));
  aura::WindowTreeHostMus window_tree_host_mus(&client, &properties);
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
                                      nullptr, std::move(tree_client_request));
  window_tree_delegate.WaitForEmbed();
  ASSERT_TRUE(!child_client.GetRoots().empty());
  window_tree_delegate.DestroyWindowTreeHost();
}

}  // namespace mus
}  // namespace ash
