// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_BASE_H_
#define COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_BASE_H_

#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_test_base.h"

namespace mojo {

class ViewManagerClientFactory;
class ViewManagerInit;

// ViewManagerTestBase is a base class for use with app tests that use
// ViewManager. SetUp() connects to the ViewManager and blocks until OnEmbed()
// has been invoked. window_manager() can be used to access the ViewManager
// established as part of SetUp().
class ViewManagerTestBase : public test::ApplicationTestBase,
                            public ApplicationDelegate,
                            public ViewManagerDelegate {
 public:
  ViewManagerTestBase();
  ~ViewManagerTestBase() override;

  // True if OnViewManagerDestroyed() was called.
  bool view_manager_destroyed() const { return view_manager_destroyed_; }

  // Runs the MessageLoop until QuitRunLoop() is called, or a timeout occurs.
  // Returns true on success. Generally prefer running a RunLoop and
  // explicitly quiting that, but use this for times when that is not possible.
  static bool DoRunLoopWithTimeout() WARN_UNUSED_RESULT;

  // Quits a run loop started by DoRunLoopWithTimeout(). Returns true on
  // success, false if a RunLoop isn't running.
  static bool QuitRunLoop() WARN_UNUSED_RESULT;

 protected:
  ViewManager* window_manager() { return window_manager_; }
  ViewManager* most_recent_view_manager() { return most_recent_view_manager_; }

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // test::ApplicationTestBase:
  ApplicationDelegate* GetApplicationDelegate() override;

  // ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override;

  // ViewManagerDelegate:
  void OnEmbed(View* root) override;
  void OnViewManagerDestroyed(ViewManager* view_manager) override;

  // Used to receive the most recent view manager loaded by an embed action.
  ViewManager* most_recent_view_manager_;

 private:
  scoped_ptr<ViewManagerInit> view_manager_init_;
  scoped_ptr<ViewManagerClientFactory> view_manager_client_factory_;

  // The View Manager connection held by the window manager (app running at the
  // root view).
  ViewManager* window_manager_;

  bool view_manager_destroyed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewManagerTestBase);
};

}  // namespace mojo

#endif  // COMPONENTS_VIEW_MANAGER_PUBLIC_CPP_TESTS_VIEW_MANAGER_TEST_BASE_H_
