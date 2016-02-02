// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "components/mus/public/interfaces/window_tree_host.mojom.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mus {

// WindowServerTestBase is a base class for use with app tests that use
// WindowServer. SetUp() connects to the WindowServer and blocks until OnEmbed()
// has been invoked. window_manager() can be used to access the WindowServer
// established as part of SetUp().
class WindowServerTestBase
    : public mojo::test::ApplicationTestBase,
      public mojo::ApplicationDelegate,
      public WindowTreeDelegate,
      public WindowManagerDelegate,
      public mojo::InterfaceFactory<mojom::WindowTreeClient> {
 public:
  WindowServerTestBase();
  ~WindowServerTestBase() override;

  // True if WindowTreeDelegate::OnConnectionLost() was called.
  bool window_tree_connection_destroyed() const {
    return window_tree_connection_destroyed_;
  }

  // Runs the MessageLoop until QuitRunLoop() is called, or a timeout occurs.
  // Returns true on success. Generally prefer running a RunLoop and
  // explicitly quiting that, but use this for times when that is not possible.
  static bool DoRunLoopWithTimeout() WARN_UNUSED_RESULT;

  // Quits a run loop started by DoRunLoopWithTimeout(). Returns true on
  // success, false if a RunLoop isn't running.
  static bool QuitRunLoop() WARN_UNUSED_RESULT;

  WindowTreeConnection* window_manager() { return window_manager_; }
  WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

 protected:
  mojom::WindowTreeHost* host() { return host_.get(); }
  WindowTreeConnection* most_recent_connection() {
    return most_recent_connection_;
  }

  void set_window_manager_delegate(WindowManagerDelegate* delegate) {
    window_manager_delegate_ = delegate;
  }

  // testing::Test:
  void SetUp() override;

  // test::ApplicationTestBase:
  mojo::ApplicationDelegate* GetApplicationDelegate() override;

  // ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // WindowTreeDelegate:
  void OnEmbed(Window* root) override;
  void OnConnectionLost(WindowTreeConnection* connection) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(WindowManagerClient* client) override;
  bool OnWmSetBounds(Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(Window* window,
                       const std::string& name,
                       scoped_ptr<std::vector<uint8_t>>* new_data) override;
  Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnAccelerator(uint32_t id, mojom::EventPtr event) override;

  // InterfaceFactory<WindowTreeClient>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojom::WindowTreeClient> request) override;

  // Used to receive the most recent window tree connection loaded by an embed
  // action.
  WindowTreeConnection* most_recent_connection_;

 private:
  mojom::WindowTreeHostPtr host_;

  // The window server connection held by the window manager (app running at
  // the root window).
  WindowTreeConnection* window_manager_;

  // A test can override the WM-related behaviour by installing its own
  // WindowManagerDelegate during the test.
  WindowManagerDelegate* window_manager_delegate_;

  WindowManagerClient* window_manager_client_;

  bool window_tree_connection_destroyed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowServerTestBase);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_
