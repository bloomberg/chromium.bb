// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_CLIENT_SETUP_H_
#define COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_CLIENT_SETUP_H_

#include <memory>

#include "base/macros.h"
#include "components/mus/public/cpp/window_tree_client_observer.h"

namespace display {
class Display;
}

namespace mus {

class TestWindowTree;
class WindowManagerDelegate;
class WindowTreeClient;
class WindowTreeClientDelegate;

// TestWindowTreeClientSetup is used to create a WindowTreeClient that is not
// connected to mus.
class TestWindowTreeClientSetup : public mus::WindowTreeClientObserver {
 public:
  TestWindowTreeClientSetup();
  ~TestWindowTreeClientSetup() override;

  // Initializes the WindowTreeClient.
  void Init(WindowTreeClientDelegate* window_tree_delegate);
  void InitForWindowManager(WindowTreeClientDelegate* window_tree_delegate,
                            WindowManagerDelegate* window_manager_delegate,
                            const display::Display& display);

  // The WindowTree that WindowTreeClient talks to.
  TestWindowTree* window_tree() { return window_tree_.get(); }

  WindowTreeClient* window_tree_client();

 private:
  // Called by both implementations of init to perform common initialization.
  void CommonInit(WindowTreeClientDelegate* window_tree_delegate,
                  WindowManagerDelegate* window_manager_delegate);

  // mus::WindowTreeClientObserver:
  void OnWillDestroyClient(mus::WindowTreeClient* client) override;

  std::unique_ptr<TestWindowTree> window_tree_;

  // See description of WindowTreeClientDelegate for details on ownership. The
  // WindowTreeClient may be deleted during shutdown by the test. If not,
  // we own it and must delete it.
  std::unique_ptr<WindowTreeClient> window_tree_client_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClientSetup);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_TESTS_TEST_WINDOW_TREE_CLIENT_SETUP_H_
