// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/shell/public/cpp/shell_test.h"

namespace mash {
namespace wm {

class WindowTreeDelegateImpl : public mus::WindowTreeDelegate {
 public:
  WindowTreeDelegateImpl() {}
  ~WindowTreeDelegateImpl() override {}

 private:
  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override {}
  void OnConnectionLost(mus::WindowTreeConnection* connection) override {}

  DISALLOW_COPY_AND_ASSIGN(WindowTreeDelegateImpl);
};

class WindowManagerTest : public mojo::test::ShellTest {
 public:
  WindowManagerTest() : mojo::test::ShellTest("exe:mash_unittests") {}
  ~WindowManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerTest);
};

void OnEmbed(bool success) {
  ASSERT_TRUE(success);
}

TEST_F(WindowManagerTest, OpenWindow) {
  WindowTreeDelegateImpl window_tree_delegate;

  // Bring up the the desktop_wm.
  connector()->Connect("mojo:desktop_wm");

  // Connect to mus and create a new top level window. The request goes to
  // the |desktop_wm|, but is async.
  std::unique_ptr<mus::WindowTreeConnection> connection(
      mus::WindowTreeConnection::Create(&window_tree_delegate, connector()));
  mus::Window* top_level_window = connection->NewTopLevelWindow(nullptr);
  ASSERT_TRUE(top_level_window);
  mus::Window* child_window = connection->NewWindow();
  ASSERT_TRUE(child_window);
  top_level_window->AddChild(child_window);

  // Create another WindowTreeConnection by way of embedding in
  // |child_window|. This blocks until it succeeds.
  mus::mojom::WindowTreeClientPtr tree_client;
  auto tree_client_request = GetProxy(&tree_client);
  child_window->Embed(std::move(tree_client), base::Bind(&OnEmbed));
  std::unique_ptr<mus::WindowTreeConnection> child_connection(
      mus::WindowTreeConnection::Create(
          &window_tree_delegate, std::move(tree_client_request),
          mus::WindowTreeConnection::CreateType::WAIT_FOR_EMBED));
  ASSERT_TRUE(!child_connection->GetRoots().empty());
}

}  // namespace wm
}  // namespace mash
