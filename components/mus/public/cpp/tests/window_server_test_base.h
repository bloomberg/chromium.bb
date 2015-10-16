// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "mojo/application/public/cpp/interface_factory.h"

namespace mus {

// WindowServerTestBase is a base class for use with app tests that use
// WindowServer. SetUp() connects to the WindowServer and blocks until OnEmbed()
// has been invoked. window_manager() can be used to access the WindowServer
// established as part of SetUp().
class WindowServerTestBase
    : public mojo::test::ApplicationTestBase,
      public mojo::ApplicationDelegate,
      public WindowTreeDelegate,
      public mojo::InterfaceFactory<mojo::ViewTreeClient> {
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

 protected:
  WindowTreeConnection* most_recent_connection() {
    return most_recent_connection_;
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // test::ApplicationTestBase:
  mojo::ApplicationDelegate* GetApplicationDelegate() override;

  // ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // WindowTreeDelegate:
  void OnEmbed(Window* root) override;
  void OnConnectionLost(WindowTreeConnection* connection) override;

  // InterfaceFactory<ViewTreeClient>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::ViewTreeClient> request) override;

  // Used to receive the most recent view tree connection loaded by an embed
  // action.
  WindowTreeConnection* most_recent_connection_;

 private:
  mojo::ViewTreeHostPtr host_;

  // The View Manager connection held by the window manager (app running at the
  // root view).
  WindowTreeConnection* window_manager_;

  bool window_tree_connection_destroyed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(WindowServerTestBase);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_WINDOW_SERVER_TEST_BASE_H_
