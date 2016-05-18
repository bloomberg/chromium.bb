// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_CONNECTION_SETUP_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_CONNECTION_SETUP_H_

#include <memory>

#include "base/macros.h"
#include "components/mus/public/cpp/window_tree_connection_observer.h"

namespace mus {

class TestWindowTree;
class WindowManagerDelegate;
class WindowTreeClientImpl;
class WindowTreeConnection;
class WindowTreeDelegate;

// TestWindowTreeConnectionSetup is used to create a WindowTreeConnection
// (really a WindowTreeClientImpl) that is not connected to mus.
class TestWindowTreeConnectionSetup : public mus::WindowTreeConnectionObserver {
 public:
  TestWindowTreeConnectionSetup();
  ~TestWindowTreeConnectionSetup() override;

  // Initializes the WindowTreeConnection. |window_manager_delegate| may be
  // null.
  void Init(WindowTreeDelegate* window_tree_delegate,
            WindowManagerDelegate* window_manager_delegate);

  // The WindowTree that WindowTreeConnection talks to.
  TestWindowTree* window_tree() { return window_tree_.get(); }

  WindowTreeConnection* window_tree_connection();

 private:
  // mus::WindowTreeConnectionObserver:
  void OnWillDestroyConnection(mus::WindowTreeConnection* connection) override;

  std::unique_ptr<TestWindowTree> window_tree_;

  // See description of WindowTreeDelegate for details on ownership. The
  // WindowTreeClientImpl may be deleted during shutdown by the test. If not,
  // we own it and must delete it.
  std::unique_ptr<WindowTreeClientImpl> window_tree_client_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeConnectionSetup);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_CONNECTION_SETUP_H_
