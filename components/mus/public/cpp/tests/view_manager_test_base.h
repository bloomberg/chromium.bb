// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_BASE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_BASE_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/public/interfaces/view_tree_host.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_test_base.h"
#include "mojo/application/public/cpp/interface_factory.h"

namespace mojo {

// ViewManagerTestBase is a base class for use with app tests that use
// ViewManager. SetUp() connects to the ViewManager and blocks until OnEmbed()
// has been invoked. window_manager() can be used to access the ViewManager
// established as part of SetUp().
class ViewManagerTestBase : public test::ApplicationTestBase,
                            public ApplicationDelegate,
                            public ViewTreeDelegate,
                            public InterfaceFactory<ViewTreeClient> {
 public:
  ViewManagerTestBase();
  ~ViewManagerTestBase() override;

  // True if ViewTreeDelegate::OnConnectionLost() was called.
  bool view_tree_connection_destroyed() const {
    return view_tree_connection_destroyed_;
  }

  // Runs the MessageLoop until QuitRunLoop() is called, or a timeout occurs.
  // Returns true on success. Generally prefer running a RunLoop and
  // explicitly quiting that, but use this for times when that is not possible.
  static bool DoRunLoopWithTimeout() WARN_UNUSED_RESULT;

  // Quits a run loop started by DoRunLoopWithTimeout(). Returns true on
  // success, false if a RunLoop isn't running.
  static bool QuitRunLoop() WARN_UNUSED_RESULT;

  ViewTreeConnection* window_manager() { return window_manager_; }

 protected:
  ViewTreeConnection* most_recent_connection() {
    return most_recent_connection_;
  }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // test::ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override;

  // ApplicationDelegate:
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override;

  // ViewTreeDelegate:
  void OnEmbed(View* root) override;
  void OnConnectionLost(ViewTreeConnection* connection) override;

  // InterfaceFactory<ViewTreeClient>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ViewTreeClient> request) override;

  // Used to receive the most recent view tree connection loaded by an embed
  // action.
  ViewTreeConnection* most_recent_connection_;

 private:
  ViewTreeHostPtr host_;

  // The View Manager connection held by the window manager (app running at the
  // root view).
  ViewTreeConnection* window_manager_;

  bool view_tree_connection_destroyed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewManagerTestBase);
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_BASE_H_
