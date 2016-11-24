// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "services/service_manager/public/cpp/service_test.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/cpp/window_tree_client_delegate.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ash {
namespace mus {

class WindowTreeClientDelegate : public ui::WindowTreeClientDelegate {
 public:
  WindowTreeClientDelegate() {}
  ~WindowTreeClientDelegate() override {}

 private:
  // ui::WindowTreeClientDelegate:
  void OnEmbed(ui::Window* root) override {}
  void OnEmbedRootDestroyed(ui::Window* root) override {}
  void OnLostConnection(ui::WindowTreeClient* client) override {}
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              ui::Window* target) override {}

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientDelegate);
};

class WindowManagerTest : public service_manager::test::ServiceTest {
 public:
  WindowManagerTest() : service_manager::test::ServiceTest("mash_unittests") {}
  ~WindowManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerTest);
};

void OnEmbed(bool success) {
  ASSERT_TRUE(success);
}

// Fails when the Ash material design shelf is enabled by default
// (ash::MaterialDesignController::IsShelfMaterial()). See
// crbug.com/660194 and crbug.com/642879.
// TODO(rockot): Reenable this test.
TEST_F(WindowManagerTest, DISABLED_OpenWindow) {
  WindowTreeClientDelegate window_tree_delegate;

  connector()->Connect("ash");

  // Connect to mus and create a new top level window. The request goes to
  // |ash|, but is async.
  std::unique_ptr<ui::WindowTreeClient> client(
      new ui::WindowTreeClient(&window_tree_delegate));
  client->ConnectViaWindowTreeFactory(connector());
  ui::Window* top_level_window = client->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(top_level_window);
  ui::Window* child_window = client->NewWindow();
  ASSERT_TRUE(child_window);
  top_level_window->AddChild(child_window);

  // Create another WindowTreeClient by way of embedding in
  // |child_window|. This blocks until it succeeds.
  ui::mojom::WindowTreeClientPtr tree_client;
  auto tree_client_request = GetProxy(&tree_client);
  child_window->Embed(std::move(tree_client), base::Bind(&OnEmbed));
  std::unique_ptr<ui::WindowTreeClient> child_client(new ui::WindowTreeClient(
      &window_tree_delegate, nullptr, std::move(tree_client_request)));
  child_client->WaitForEmbed();
  ASSERT_TRUE(!child_client->GetRoots().empty());
}

}  // namespace mus
}  // namespace ash
