// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_test_base.h"

namespace mash {
namespace wm {

class WindowManagerAppTest : public mojo::test::ApplicationTestBase,
                             public mus::WindowTreeDelegate {
 public:
  WindowManagerAppTest() {}
  ~WindowManagerAppTest() override {}

 protected:
  void ConnectToWindowManager(
      mus::mojom::WindowManagerDeprecatedPtr* window_manager) {
    application_impl()->ConnectToService("mojo:desktop_wm", window_manager);
  }

  mus::Window* OpenWindow(mus::mojom::WindowManagerDeprecated* window_manager) {
    mus::mojom::WindowTreeClientPtr window_tree_client;
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient>
        window_tree_client_request = GetProxy(&window_tree_client);
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties;
    properties.mark_non_null();
    window_manager->OpenWindow(std::move(window_tree_client),
                               std::move(properties));
    mus::WindowTreeConnection* connection = mus::WindowTreeConnection::Create(
        this, std::move(window_tree_client_request),
        mus::WindowTreeConnection::CreateType::WAIT_FOR_EMBED);
    return *connection->GetRoots().begin();
  }

 private:
  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override {}
  void OnConnectionLost(mus::WindowTreeConnection* connection) override {}

  DISALLOW_COPY_AND_ASSIGN(WindowManagerAppTest);
};

TEST_F(WindowManagerAppTest, OpenWindow) {
  mus::mojom::WindowManagerDeprecatedPtr connection;
  ConnectToWindowManager(&connection);

  ASSERT_TRUE(OpenWindow(connection.get()));
}

}  // namespace wm
}  // namespace mash
