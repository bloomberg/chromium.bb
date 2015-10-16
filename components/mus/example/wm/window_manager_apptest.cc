// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_test_base.h"

class WindowManagerAppTest : public mojo::test::ApplicationTestBase,
                             public mus::WindowTreeDelegate {
 public:
  WindowManagerAppTest() {}
  ~WindowManagerAppTest() override {}

 protected:
  void ConnectToWindowManager(mus::mojom::WindowManagerPtr* window_manager) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = "mojo:example_wm";
    application_impl()->ConnectToService(request.Pass(), window_manager);
  }

  mus::Window* OpenWindow(mus::mojom::WindowManager* window_manager) {
    mojo::ViewTreeClientPtr view_tree_client;
    mojo::InterfaceRequest<mojo::ViewTreeClient> view_tree_client_request =
        GetProxy(&view_tree_client);
    window_manager->OpenWindow(view_tree_client.Pass());
    mus::WindowTreeConnection* connection = mus::WindowTreeConnection::Create(
        this, view_tree_client_request.Pass(),
        mus::WindowTreeConnection::CreateType::WAIT_FOR_EMBED);
    return connection->GetRoot();
  }

 private:
  // mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override {}
  void OnConnectionLost(mus::WindowTreeConnection* connection) override {}

  DISALLOW_COPY_AND_ASSIGN(WindowManagerAppTest);
};

TEST_F(WindowManagerAppTest, CenterWindow) {
  // TODO(beng): right now this only verifies that requests from other
  //             connections are blocked. We need to be able to reliably
  //             configure the size of the window manager's display before we
  //             can properly validate that centering actually happened.
  mus::mojom::WindowManagerPtr connection1, connection2;
  ConnectToWindowManager(&connection1);
  ConnectToWindowManager(&connection2);

  mus::Window* window_from_connection1 = OpenWindow(connection1.get());
  mus::Window* window_from_connection2 = OpenWindow(connection2.get());

  bool succeeded = false;
  connection1->CenterWindow(
      window_from_connection1->id(), mojo::Size::New(),
      [&succeeded](mus::mojom::WindowManagerErrorCode result) {
        succeeded = result == mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS;
      });
  ASSERT_TRUE(connection1.WaitForIncomingResponse());
  EXPECT_TRUE(succeeded);

  succeeded = false;
  connection1->CenterWindow(
      window_from_connection2->id(), mojo::Size::New(),
      [&succeeded](mus::mojom::WindowManagerErrorCode result) {
        succeeded = result == mus::mojom::WINDOW_MANAGER_ERROR_CODE_SUCCESS;
      });
  ASSERT_TRUE(connection1.WaitForIncomingResponse());
  EXPECT_FALSE(succeeded);
}
