// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/test_window_tree_client_setup.h"

#include "components/mus/public/cpp/tests/test_window_tree.h"
#include "components/mus/public/cpp/tests/window_tree_client_private.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "ui/display/display.h"

namespace mus {

TestWindowTreeClientSetup::TestWindowTreeClientSetup() {}

TestWindowTreeClientSetup::~TestWindowTreeClientSetup() {
  std::unique_ptr<WindowTreeClient> window_tree_client =
      std::move(window_tree_client_);
  if (window_tree_client)
    window_tree_client->RemoveObserver(this);
}

void TestWindowTreeClientSetup::Init(
    WindowTreeClientDelegate* window_tree_delegate) {
  CommonInit(window_tree_delegate, nullptr);
  WindowTreeClientPrivate(window_tree_client_.get())
      .OnEmbed(window_tree_.get());
}

void TestWindowTreeClientSetup::InitForWindowManager(
    WindowTreeClientDelegate* window_tree_delegate,
    WindowManagerDelegate* window_manager_delegate,
    const display::Display& display) {
  CommonInit(window_tree_delegate, window_manager_delegate);
  WindowTreeClientPrivate(window_tree_client_.get())
      .SetTreeAndClientId(window_tree_.get(), 1);
}

WindowTreeClient* TestWindowTreeClientSetup::window_tree_client() {
  return window_tree_client_.get();
}

void TestWindowTreeClientSetup::CommonInit(
    WindowTreeClientDelegate* window_tree_delegate,
    WindowManagerDelegate* window_manager_delegate) {
  window_tree_.reset(new TestWindowTree);
  window_tree_client_.reset(new WindowTreeClient(
      window_tree_delegate, window_manager_delegate, nullptr));
  static_cast<WindowTreeClient*>(window_tree_client_.get())
      ->AddObserver(this);
}

void TestWindowTreeClientSetup::OnWillDestroyClient(
    mus::WindowTreeClient* client) {
  // See comment in header as to why we do this.
  window_tree_client_.release();
}

}  // namespace mus
